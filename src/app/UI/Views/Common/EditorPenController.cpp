#include "EditorPenController.h"

#include "EditorPenHoverWatcher.h"
#include "EditorPointerUtils.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppOptions/Options/DeveloperOption.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QTabletEvent>
#include <QWidget>

#include <lite/GUI/Utils/IconUtils.h>

namespace {
    // How long a hover sighting stays valid. The pen state can change without
    // any Qt event reaching a widget (a barrel press during hover), so the
    // widget that last saw the pen takes the cursor hint; the window keeps a
    // stale sighting from putting an eraser cursor on a widget the pen has
    // long left.
    constexpr qint64 hoverSightingMs = 3000;

    // How long after the barrel button goes up its platform right-click may
    // still show up as a context menu event. The two are milliseconds apart on
    // a click, and a finger's press-and-hold takes an order of magnitude longer
    // than this, so the window cannot be borrowed by somebody else's menu.
    constexpr qint64 barrelGraceMs = 400;

    const char *pointerName(const QPointingDevice::PointerType type) {
        switch (type) {
            case QPointingDevice::PointerType::Eraser:
                return "Eraser";
            case QPointingDevice::PointerType::Pen:
                return "Pen";
            case QPointingDevice::PointerType::Cursor:
                return "Cursor";
            case QPointingDevice::PointerType::Unknown:
                return "Unknown";
            default:
                return "?";
        }
    }

    const char *inputName(const EditorPenStroke::Input input) {
        switch (input) {
            case EditorPenStroke::Input::Tip:
                return "tip";
            case EditorPenStroke::Input::Eraser:
                return "eraser";
            case EditorPenStroke::Input::SideButton:
                return "sidebutton";
        }
        return "?";
    }

    const char *intentName(const EditorPenStroke::Intent::Type type) {
        switch (type) {
            case EditorPenStroke::Intent::Type::Begin:
                return "Begin";
            case EditorPenStroke::Intent::Type::Move:
                return "Move";
            case EditorPenStroke::Intent::Type::End:
                return "End";
            case EditorPenStroke::Intent::Type::ContextMenu:
                return "ContextMenu";
        }
        return "?";
    }

    // The hover eraser cursor: the toolbar's eraser icon with a dark halo, so
    // it reads on both the dark editor canvas and the lighter panels. The hot
    // spot is the tip of the eraser, bottom left of the glyph.
    QCursor buildEraseCursor() {
        constexpr QSize iconSize(24, 24);
        // Rendered at the screen's pixel ratio, or it looks soft on a scaled
        // display. Built once and cached, so it takes the ratio of whatever
        // screen the editor is on when the first erase hint appears.
        const auto *screen = QGuiApplication::primaryScreen();
        const auto ratio = screen ? screen->devicePixelRatio() : 1.0;
        const auto path = QStringLiteral(":/svg/icons/eraser_24_filled.svg");
        const auto halo =
            IconUtils::renderTintedSvgPixmap(path, iconSize, QColor(18, 20, 24, 220), ratio);
        const auto body =
            IconUtils::renderTintedSvgPixmap(path, iconSize, QColor(244, 246, 249), ratio);
        QPixmap pixmap((iconSize.width() + 1) * ratio, (iconSize.height() + 1) * ratio);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.drawPixmap(ratio, ratio, halo);
        painter.drawPixmap(0, 0, body);
        painter.end();
        // The halo adds one logical pixel on each side, so the hot spot moves
        // with it but not with the ratio.
        pixmap.setDevicePixelRatio(ratio);
        return QCursor(pixmap, 3, 21);
    }
}

EditorPenController::EditorPenController(EditorPenTarget *target, QWidget *widget,
                                         QWidget *eventTarget, QObject *parent)
    : QObject(parent ? parent : widget), m_target(target), m_widget(widget),
      m_eventTarget(eventTarget ? eventTarget : widget),
      // Installed once for the whole process, inert until the first editor
      // widget asks for it.
      m_hover(EditorPenHoverWatcher::instance()) {
    m_clock.start();
    connect(m_hover, &EditorPenHoverWatcher::changed, this, [this] {
        // Remember when the barrel was last seen down. During hover no Qt event
        // exists for it at all, and this is the only trace left when a stroke's
        // press never reached us (an open menu had it) — the platform's own
        // context menu then still needs recognising and dropping.
        //
        // Deliberately nothing more than a timestamp: acting on this state's
        // edges is what closed menus the user had just opened, because the flag
        // does not hold still across a barrel click (the popup's SetCapture
        // makes the pointer leave and re-enter, and setState() only emits when
        // the tuple moves, so the round trip reads as a fresh press).
        if (m_hover->state().sideButton)
            m_barrelDownMs = m_clock.elapsed();
        // The pen left the digitizer altogether, so nothing that was in flight
        // can still be alive. This is the net for a stroke whose end frame
        // never reached the widget (the pen was carried out of range with the
        // tip down).
        if (!m_hover->state().inRange && isStrokeActive())
            cancel();
        if (!showsEraseHint()) {
            clearHoverCursor();
            return;
        }
        if (m_lastHoverMs < 0 || m_clock.elapsed() - m_lastHoverMs > hoverSightingMs)
            return;
        refreshHoverCursor(m_lastHoverPosition);
    });
}

EditorPenController::~EditorPenController() {
    // Never leave the process-wide counters unbalanced.
    if (m_eraseIntentActive)
        EditorPointer::endPenEraseIntent();
    if (m_streamActive)
        EditorPointer::endPenStream();
}

bool EditorPenController::isProbeEnabled() {
    return appOptions->developer()->logTouchEvents;
}

bool EditorPenController::isStrokeActive() const {
    return m_stroke.phase() != EditorPenStroke::Phase::Idle;
}

QCursor EditorPenController::eraseCursor() {
    // Never destroyed: a cursor built from a pixmap must not outlive the GUI
    // teardown of the application.
    static const QCursor *cursor = new QCursor(buildEraseCursor());
    return *cursor;
}

bool EditorPenController::penEraserAvailable() const {
    return m_target && m_target->penEraserAction() != EditorPenEraser::Unsupported;
}

bool EditorPenController::eraseHintFor(const EditorPenEraser action) {
    if (action == EditorPenEraser::Unsupported)
        return false;
    const auto *watcher = EditorPenHoverWatcher::existing();
    // existing(), not instance(): asking about a cursor must never be what
    // installs the platform machinery.
    return watcher && watcher->eraseHint();
}

bool EditorPenController::eraseHintActive() {
    const auto *watcher = EditorPenHoverWatcher::existing();
    return watcher && watcher->eraseHint();
}

bool EditorPenController::eraseHintRefused(const EditorPenEraser action) {
    return action == EditorPenEraser::Unsupported && eraseHintActive();
}

bool EditorPenController::showsEraseHint() const {
    return m_target && eraseHintFor(m_target->penEraserAction());
}

bool EditorPenController::penBarrelActive() const {
    return m_barrelDownMs >= 0 && m_clock.elapsed() - m_barrelDownMs <= barrelGraceMs;
}

EditorPenStroke::Sample EditorPenController::sampleFrom(const QTabletEvent *event) {
    EditorPenStroke::Sample sample;
    sample.position = event->position();
    sample.pressure = event->pressure();
    sample.buttons = event->buttons();
    const auto *device = event->pointingDevice();
    sample.pointerType = device ? device->pointerType() : QPointingDevice::PointerType::Unknown;
    return sample;
}

bool EditorPenController::handleEvent(QEvent *event) {
    switch (event->type()) {
        case QEvent::TabletPress:
        case QEvent::TabletMove:
        case QEvent::TabletRelease:
            return handleTabletEvent(static_cast<QTabletEvent *>(event));
        case QEvent::ContextMenu: {
            // The platform's own menu for the barrel button, and only that one:
            // the menu this layer posts itself carries another reason, and a
            // menu that belongs to a real right-click was never armed for.
            // Dropping it is what keeps "no erase happens" from turning into a
            // menu at the point the pen was lifted.
            const auto *menu = static_cast<QContextMenuEvent *>(event);
            // Two ways to recognise it. The armed stroke path: a claimed barrel
            // stroke is in flight or just ended, and the platform's message for
            // it is the next one to arrive. Or the barrel itself: the stroke was
            // swallowed by a popup this layer never saw, and the only trace left
            // is that the pen's barrel was down a moment ago.
            const auto platformCopy = menu->reason() == QContextMenuEvent::Mouse &&
                                      (m_platformMenuPending || penBarrelActive());
            if (platformCopy) {
                // Spent. Only the event it was armed for may clear it: if this
                // layer's own menu happens to be delivered first, the armed
                // state has to survive so the platform's copy is still caught.
                m_platformMenuPending = false;
                if (isProbeEnabled())
                    qDebug().noquote()
                        << QStringLiteral("pen swallowed platform context menu at (%1,%2)")
                               .arg(menu->pos().x())
                               .arg(menu->pos().y());
                event->accept();
                return true;
            }
            return false;
        }
        case QEvent::Leave:
            // The pen (or the mouse) left the widget: the hover hint no longer
            // describes what is under the cursor.
            m_lastHoverMs = -1;
            clearHoverCursor();
            return false;
        case QEvent::MouseMove:
        case QEvent::HoverEnter:
        case QEvent::HoverMove: {
            // A real mouse takes the cursor back from the pen hint. The events
            // the pen layer synthesizes itself carry the pen device and must
            // not clear it.
            const auto *device = static_cast<QSinglePointEvent *>(event)->pointingDevice();
            if (device && device->type() == QInputDevice::DeviceType::Mouse)
                clearHoverCursor();
            return false;
        }
        default:
            return false;
    }
}

bool EditorPenController::handleTabletEvent(QTabletEvent *event) {
    if (!m_target || !m_widget)
        return false;

    const auto sample = sampleFrom(event);
    m_device = event->pointingDevice();
    if (EditorPenStroke::hasSideButton(sample.buttons))
        m_barrelDownMs = m_clock.elapsed();
    const bool inContact = sample.pressure > 0.0;
    const bool probe = isProbeEnabled();
    const auto swallow = [this, event] {
        event->accept();
        return true;
    };

    if (m_trailingRelease) {
        // The claimed stroke ended on a move that already reported the lift.
        // The release that follows it is the other half of that stroke, and
        // letting it through would hand Qt a mouse release it never saw a press
        // for.
        m_trailingRelease = false;
        if (event->type() == QEvent::TabletRelease)
            return swallow();
    }

    if (m_stroke.phase() == EditorPenStroke::Phase::Idle) {
        if (m_foreignContact) {
            // A tip stroke Qt is synthesizing is in flight, so the pen layer
            // only watches. The platform emits an extra press/release pair
            // around a mid-stroke barrel change, and those are noise about a
            // stroke that is already running: they are swallowed, and the
            // stroke stays what it is (the moves keep flowing to Qt).
            switch (event->type()) {
                case QEvent::TabletPress:
                    m_foreignBarrelNoise = true;
                    if (probe)
                        qDebug().noquote()
                            << QStringLiteral("pen swallowed barrel press mid-stroke (buttons=%1)")
                                   .arg(static_cast<int>(sample.buttons));
                    return swallow();
                case QEvent::TabletRelease:
                    if (m_foreignBarrelNoise && inContact) {
                        m_foreignBarrelNoise = false;
                        if (probe)
                            qDebug().noquote()
                                << QStringLiteral("pen swallowed barrel release mid-stroke");
                        return swallow();
                    }
                    m_foreignBarrelNoise = false;
                    m_foreignContact = false;
                    return false;
                case QEvent::TabletMove:
                    if (!inContact)
                        m_foreignContact = false;
                    return false;
                default:
                    return false;
            }
        }

        switch (event->type()) {
            case QEvent::TabletPress:
                if (!EditorPenStroke::needsTranslation(sample)) {
                    // A plain tip: Qt's own tablet-to-mouse synthesis already
                    // reproduces it exactly, double click included.
                    m_foreignContact = inContact;
                    if (probe)
                        qDebug().noquote() << QStringLiteral("pen tip press %1 pressure=%2 -> Qt")
                                                  .arg(QLatin1String(pointerName(sample.pointerType)))
                                                  .arg(sample.pressure);
                    return false;
                }
                break;
            case QEvent::TabletMove:
                if (!inContact) {
                    // Hover. Never taken over, but it is what keeps the eraser
                    // cursor in step with the pen.
                    refreshHoverCursor(sample.position);
                    return false;
                }
                m_foreignContact = true;
                return false;
            case QEvent::TabletRelease:
                return false;
            default:
                return false;
        }

        // The eraser or the barrel button: this stroke is claimed.
        const auto supported = penEraserAvailable();
        const auto intents = m_stroke.pressed(sample, supported);
        // A barrel stroke is the one the platform answers with a right-click of
        // its own, and it does so at the *release* — always after this press.
        // Arming here is what makes the swallow land on time; arming at the end
        // of the stroke would be arming after the event it is meant to catch.
        m_platformMenuPending = m_stroke.input() == EditorPenStroke::Input::SideButton;
        if (probe)
            qDebug().noquote() << QStringLiteral("pen %1 press buttons=%2 pressure=%3 -> %4")
                                      .arg(QLatin1String(inputName(m_stroke.input())))
                                      .arg(static_cast<int>(sample.buttons))
                                      .arg(sample.pressure)
                                      .arg(supported ? QStringLiteral("taken over")
                                                     : QStringLiteral("swallowed"));
        reportIntents(intents);
        dispatch(intents);
        return swallow();
    }

    // The stroke is claimed, so everything up to the pen leaving the glass is
    // translated here. Which report it is barely matters: the contact state
    // decides (EditorPenStroke::feed). A press now can only be the platform
    // reacting to the barrel going down, and a release that still reports
    // contact is the other half of that same noise — neither ends the stroke.
    const auto report = [event] {
        switch (event->type()) {
            case QEvent::TabletPress:
                return EditorPenStroke::Report::Press;
            case QEvent::TabletMove:
                return EditorPenStroke::Report::Move;
            default:
                return EditorPenStroke::Report::Release;
        }
    }();

    const auto intents = m_stroke.feed(report, sample);
    if (m_stroke.phase() == EditorPenStroke::Phase::Idle) {
        // The pen left the glass, so whatever Qt might still think about a
        // stroke that was never claimed is stale. A stroke that ended on a move
        // has already seen the lift, and the platform's own release still
        // follows it: that one belongs to the stroke that is closed.
        m_foreignContact = false;
        m_foreignBarrelNoise = false;
        m_trailingRelease = event->type() != QEvent::TabletRelease;
    } else if (probe && intents.isEmpty()) {
        qDebug().noquote() << QStringLiteral("pen ignored mid-stroke %1 (buttons=%2)")
                                  .arg(event->type() == QEvent::TabletPress
                                           ? QStringLiteral("press")
                                           : QStringLiteral("release"))
                                  .arg(static_cast<int>(sample.buttons));
    }

    reportIntents(intents);
    dispatch(intents);
    // Claimed means claimed: the event is consumed even when it produced no
    // intent at all, so Qt never synthesises a second, contradicting stroke out
    // of the noise.
    return swallow();
}

void EditorPenController::reportIntents(const EditorPenStroke::Intents &intents) {
    if (!isProbeEnabled())
        return;
    for (const auto &intent : intents) {
        qDebug().noquote() << QStringLiteral("pen %1 at (%2,%3)%4")
                                  .arg(QLatin1String(intentName(intent.type)))
                                  .arg(qRound(intent.position.x()))
                                  .arg(qRound(intent.position.y()))
                                  .arg(intent.erase ? QStringLiteral(" erase") : QString());
    }
}

void EditorPenController::dispatch(const EditorPenStroke::Intents &intents) {
    for (const auto &intent : intents) {
        switch (intent.type) {
            case EditorPenStroke::Intent::Type::Begin:
                if (!m_streamActive) {
                    EditorPointer::beginPenStream();
                    m_streamActive = true;
                }
                if (intent.erase && !m_eraseIntentActive) {
                    // The intent is process wide and is what tells a view to
                    // take a plain left-button stream down its erase path.
                    EditorPointer::beginPenEraseIntent();
                    m_eraseIntentActive = true;
                    m_target->beginPenEraserStroke();
                }
                sendSyntheticMouse(QEvent::MouseButtonPress, intent.position, Qt::LeftButton,
                                   Qt::LeftButton);
                break;
            case EditorPenStroke::Intent::Type::Move:
                sendSyntheticMouse(QEvent::MouseMove, intent.position, Qt::NoButton,
                                   Qt::LeftButton);
                break;
            case EditorPenStroke::Intent::Type::End:
                sendSyntheticMouse(QEvent::MouseButtonRelease, intent.position, Qt::LeftButton,
                                   Qt::NoButton);
                // Disarmed after the release, so the view still sees the erase
                // mode it armed while it commits the stroke.
                if (m_eraseIntentActive) {
                    m_target->endPenEraserStroke();
                    EditorPointer::endPenEraseIntent();
                    m_eraseIntentActive = false;
                }
                if (m_streamActive) {
                    EditorPointer::endPenStream();
                    m_streamActive = false;
                }
                break;
            case EditorPenStroke::Intent::Type::ContextMenu:
                // A right-button click, so the interaction layer that reads the
                // press button state stays consistent, followed by the menu
                // itself. The menu event has to be posted: it runs a nested
                // event loop through exec().
                sendSyntheticMouse(QEvent::MouseButtonPress, intent.position, Qt::RightButton,
                                   Qt::RightButton);
                sendSyntheticMouse(QEvent::MouseButtonRelease, intent.position, Qt::RightButton,
                                   Qt::NoButton);
                postContextMenu(intent.position);
                break;
        }
    }
}

void EditorPenController::sendSyntheticMouse(const QEvent::Type type, const QPointF &position,
                                             const Qt::MouseButton button,
                                             const Qt::MouseButtons buttons) {
    auto *target = m_eventTarget ? m_eventTarget.data() : m_widget.data();
    if (!target)
        return;
    const auto global = target->mapToGlobal(position);
    // The pen device travels with the event, the same way the touch device
    // does, so the rest of the codebase can tell where a stream came from.
    QMouseEvent event(type, position, position, global, button, buttons,
                      QApplication::keyboardModifiers(),
                      m_device ? m_device : QPointingDevice::primaryPointingDevice());
    QCoreApplication::sendEvent(target, &event);
}

void EditorPenController::postContextMenu(const QPointF &position) {
    auto *target = m_eventTarget ? m_eventTarget.data() : m_widget.data();
    if (!target)
        return;
    const auto global = target->mapToGlobal(position).toPoint();
    QCoreApplication::postEvent(
        target, new QContextMenuEvent(QContextMenuEvent::Other, position.toPoint(), global,
                                      QApplication::keyboardModifiers()));
}

QWidget *EditorPenController::cursorWidget() const {
    // The cursor goes on the widget the pointer events arrive at: the viewport
    // for the legacy QGraphicsView family, the widget itself for the RHI
    // editors. It has to be that one, because a widget that has a cursor of its
    // own overrides the one it inherits: writing here never destroys a cursor
    // the view set for its own hover states, and clearing it hands control
    // straight back.
    return m_eventTarget ? m_eventTarget.data() : m_widget.data();
}

void EditorPenController::refreshHoverCursor(const QPointF &position) {
    m_lastHoverPosition = position;
    m_lastHoverMs = m_clock.elapsed();
    auto *target = cursorWidget();
    if (!showsEraseHint() || !target || !target->isVisible()) {
        clearHoverCursor();
        return;
    }
    if (m_hoverCursorApplied)
        return;
    target->setCursor(eraseCursor());
    m_hoverCursorApplied = true;
}

void EditorPenController::clearHoverCursor() {
    if (!m_hoverCursorApplied)
        return;
    m_hoverCursorApplied = false;
    if (auto *target = cursorWidget())
        target->unsetCursor();
}

void EditorPenController::cancel() {
    // A cancelled stroke is released, never turned into a menu.
    dispatch(m_stroke.cancelled());
    m_foreignContact = false;
    m_foreignBarrelNoise = false;
    m_trailingRelease = false;
    m_platformMenuPending = false;
    clearHoverCursor();
}
