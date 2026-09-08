#include "EditorTouchController.h"

#include "EditorPointerUtils.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppOptions/Options/AppearanceOption.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QList>
#include <QMouseEvent>
#include <QTimer>
#include <QTouchEvent>
#include <QWidget>

#include <cmath>

namespace {
    // Inertia glide after a flick. Exponential decay is frame-rate independent,
    // which matters because the timer interval is only a hint.
    constexpr int inertiaIntervalMs = 16;
    constexpr double inertiaDecayPerSecond = 3.6;
    constexpr double inertiaStopSpeed = 20.0;
    // Weight kept on the previous estimate when tracking a single-finger pan.
    constexpr double panVelocitySmoothing = 0.55;
    // How long to wait for the platform's own press-and-hold context menu
    // before posting one ourselves. Windows delivers it within a millisecond
    // of the touch ending, so on Windows the fallback never fires.
    constexpr int contextMenuFallbackMs = 400;
    // How long after the last touch event a context menu still counts as
    // coming from that gesture. The platform's press-and-hold menu lands
    // within a millisecond of the finger leaving, so this only has to outlast
    // message queue jitter.
    constexpr int contextMenuOwnershipMs = 600;
}

EditorTouchController::EditorTouchController(EditorTouchTarget *target, QWidget *widget,
                                             QWidget *eventTarget, QObject *parent)
    : QObject(parent ? parent : widget), m_target(target), m_widget(widget),
      m_eventTarget(eventTarget ? eventTarget : widget),
      m_longPressTimer(new QTimer(this)), m_inertiaTimer(new QTimer(this)),
      m_contextMenuFallbackTimer(new QTimer(this)) {
    m_clock.start();

    m_longPressTimer->setSingleShot(true);
    connect(m_longPressTimer, &QTimer::timeout, this,
            [this] { dispatch(m_gesture.longPressTimeout(now())); });

    m_inertiaTimer->setInterval(inertiaIntervalMs);
    connect(m_inertiaTimer, &QTimer::timeout, this, &EditorTouchController::onInertiaFrame);

    m_contextMenuFallbackTimer->setSingleShot(true);
    m_contextMenuFallbackTimer->setInterval(contextMenuFallbackMs);
    connect(m_contextMenuFallbackTimer, &QTimer::timeout, this, [this] {
        if (!m_contextMenuExpected)
            return;
        // The expectation stays armed: the event we are about to post comes
        // back through filterContextMenuEvent(), which is what clears it.
        postContextMenu(m_pendingContextMenuPosition);
    });
}

EditorTouchController::~EditorTouchController() {
    // Never leave the global touch-stream counter unbalanced.
    if (m_syntheticStreamActive)
        EditorPointer::endTouchStream();
}

bool EditorTouchController::isEnabled() {
    return appOptions->appearance()->enableTouchGestures;
}

bool EditorTouchController::isGestureActive() const {
    return m_gesture.phase() != EditorTouchGesture::Phase::Idle;
}

qint64 EditorTouchController::now() const {
    return m_clock.elapsed();
}

bool EditorTouchController::handleEvent(QEvent *event) {
    switch (event->type()) {
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
            return handleTouchEvent(static_cast<QTouchEvent *>(event));
        case QEvent::TouchCancel:
            if (!isGestureActive())
                return false;
            cancel();
            event->accept();
            return true;
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
            return swallowForeignMouseEvent(static_cast<QMouseEvent *>(event));
        case QEvent::ContextMenu:
            return filterContextMenuEvent(static_cast<QContextMenuEvent *>(event));
        default:
            return false;
    }
}

bool EditorTouchController::swallowForeignMouseEvent(QMouseEvent *event) {
    if (!m_target || !isEnabled())
        return false;
    // Our own synthetic events, a real mouse and a stylus all report
    // Qt::MouseEventNotSynthesized. Anything else was made from a touch we are
    // already handling ourselves, so it is a duplicate.
    if (event->source() == Qt::MouseEventNotSynthesized)
        return false;
    event->accept();
    return true;
}

bool EditorTouchController::touchOwnsContextMenu() const {
    if (isGestureActive() || m_syntheticStreamActive || m_panStreamActive)
        return true;
    if (m_lastTouchActivityMs < 0)
        return false;
    return now() - m_lastTouchActivityMs < contextMenuOwnershipMs;
}

bool EditorTouchController::filterContextMenuEvent(QContextMenuEvent *event) {
    if (!m_target || !isEnabled())
        return false;
    if (m_contextMenuExpected) {
        // Either the platform beat us to it, which is the good case because it
        // brings the native press-and-hold feedback and the native timing, or
        // this is the one the fallback timer posted. Both are ours.
        cancelContextMenuFallback();
        // A menu runs a nested event loop and grabs the pointer, so anything
        // still in flight would never see its release.
        if (m_syntheticStreamActive || m_panStreamActive)
            onSingleCancel();
        return false;
    }
    if (!touchOwnsContextMenu())
        return false;
    // Windows raises its press-and-hold menu on release no matter what we did
    // with the same finger in the meantime. On blank canvas a held press is a
    // rubber band here, not a menu, so this one has to go: letting it through
    // pops a menu on top of the selection the finger just made, and the popup
    // grab can swallow the touch release that would have ended the rubber
    // band.
    event->accept();
    return true;
}

void EditorTouchController::armContextMenuFallback(const QPointF &position) {
    m_pendingContextMenuPosition = position;
    m_contextMenuExpected = true;
}

void EditorTouchController::cancelContextMenuFallback() {
    m_contextMenuExpected = false;
    m_contextMenuFallbackTimer->stop();
}

bool EditorTouchController::handleTouchEvent(QTouchEvent *event) {
    if (!m_target || !m_widget)
        return false;
    // Leaving TouchBegin unaccepted is what makes Qt fall back to its own
    // touch-to-mouse synthesis, which is exactly the behaviour we want when the
    // feature is switched off.
    if (!isEnabled()) {
        if (isGestureActive())
            cancel();
        return false;
    }

    m_device = event->pointingDevice();
    const auto timestamp = now();
    m_lastTouchActivityMs = timestamp;
    // A brand new gesture inherits nothing: an expectation left over from a
    // long press whose menu never arrived would otherwise let this gesture's
    // platform menu through.
    if (m_gesture.phase() == EditorTouchGesture::Phase::Idle)
        cancelContextMenuFallback();
    for (const auto &point : event->points()) {
        const auto position = point.position();
        switch (point.state()) {
            case QEventPoint::State::Pressed:
                dispatch(m_gesture.pressed(point.id(), position, timestamp));
                break;
            case QEventPoint::State::Updated:
                dispatch(m_gesture.moved(point.id(), position, timestamp));
                break;
            case QEventPoint::State::Released:
                dispatch(m_gesture.released(point.id(), position, timestamp));
                break;
            case QEventPoint::State::Stationary:
            case QEventPoint::State::Unknown:
                break;
        }
    }
    dispatch(m_gesture.flushNavigation(timestamp));

    QList<int> activeIds;
    for (const auto &point : event->points()) {
        if (point.state() != QEventPoint::State::Released)
            activeIds.append(point.id());
    }
    dispatch(m_gesture.syncActivePoints(activeIds));

    if (m_gesture.longPressDeadline() != 0)
        armLongPressTimer();
    else
        disarmLongPressTimer();

    // The press-and-hold menu belongs to the release, so only start waiting for
    // the platform's once every finger has left the glass.
    if (m_contextMenuExpected && activeIds.isEmpty() && !m_contextMenuFallbackTimer->isActive())
        m_contextMenuFallbackTimer->start();

    event->accept();
    return true;
}

void EditorTouchController::armLongPressTimer() {
    const auto remaining = m_gesture.longPressDeadline() - now();
    if (remaining <= 0) {
        dispatch(m_gesture.longPressTimeout(now()));
        return;
    }
    m_longPressTimer->start(static_cast<int>(remaining));
}

void EditorTouchController::disarmLongPressTimer() {
    m_longPressTimer->stop();
}

void EditorTouchController::dispatch(const EditorTouchGesture::Events &events) {
    for (const auto &event : events) {
        switch (event.type) {
            case EditorTouchGesture::Event::Type::SingleBegin:
                onSingleBegin(event);
                break;
            case EditorTouchGesture::Event::Type::SingleMove:
                onSingleMove(event);
                break;
            case EditorTouchGesture::Event::Type::SingleEnd:
                onSingleEnd(event);
                break;
            case EditorTouchGesture::Event::Type::SingleCancel:
                onSingleCancel();
                break;
            case EditorTouchGesture::Event::Type::LongPress:
                onLongPress(event);
                break;
            case EditorTouchGesture::Event::Type::NavigationBegin:
                stopInertia();
                m_target->stopTouchViewportAnimation();
                break;
            case EditorTouchGesture::Event::Type::NavigationUpdate:
                if (!event.panDelta.isNull())
                    m_target->panTouchViewportBy(event.panDelta);
                if (event.horizontalFactor != 1.0 || event.verticalFactor != 1.0) {
                    m_target->zoomTouchViewportBy(event.horizontalFactor, event.verticalFactor,
                                                  event.anchor);
                }
                break;
            case EditorTouchGesture::Event::Type::NavigationEnd:
                startInertia(event.velocity);
                break;
        }
    }
}

void EditorTouchController::onSingleBegin(const EditorTouchGesture::Event &event) {
    stopInertia();
    m_target->stopTouchViewportAnimation();
    m_lastStreamPosition = event.position;
    m_panVelocity = {};
    m_panTimestamp = now();

    // A held press always means "select", never "create": it is the gesture
    // that reaches rubber band / interval selection when the plain drag is
    // already taken by scrolling.
    // A tap always goes straight to the interaction layer: it must select or
    // deselect, never create content and never scroll.
    auto action = EditorTouchTarget::BlankDragAction::SyntheticMouse;
    if (!event.tap && !event.fromLongPress) {
        // Only a plain drag has to pick a side, and it may move content just
        // when the finger has already selected it. Over anything else the drag
        // does what it does over blank canvas, so a finger resting on a note it
        // never selected still scrolls instead of dragging that note away.
        if (m_target->touchContentAt(event.position) != EditorTouchTarget::ContentHit::Selected)
            action = m_target->touchBlankDragAction();
    }

    if (action == EditorTouchTarget::BlankDragAction::Pan) {
        m_panStreamActive = true;
        return;
    }

    m_syntheticStreamActive = true;
    EditorPointer::beginTouchStream();

    // A held press only ever reaches here over blank canvas, because a long
    // press on an object is consumed without a stream. Where that blank canvas
    // is navigation territory the same hold has to serve two gestures: move
    // and it is a rubber band, stay put and it is the platform's press and
    // hold menu. Holding the press back until the finger travels keeps both,
    // and stops a motionless hold from clearing the selection on the way.
    if (event.fromLongPress &&
        m_target->touchBlankDragAction() == EditorTouchTarget::BlankDragAction::Pan) {
        m_pressDeferred = true;
        m_deferredPressPosition = event.position;
        return;
    }

    sendSyntheticMouse(QEvent::MouseButtonPress, event.position, Qt::LeftButton, Qt::LeftButton);
    if (event.doubleTap) {
        sendSyntheticMouse(QEvent::MouseButtonDblClick, event.position, Qt::LeftButton,
                           Qt::LeftButton);
    }
}

void EditorTouchController::onSingleMove(const EditorTouchGesture::Event &event) {
    if (m_pressDeferred) {
        const auto travel = event.position - m_deferredPressPosition;
        if (std::hypot(travel.x(), travel.y()) <= m_gesture.config().longPressSlopPx) {
            m_lastStreamPosition = event.position;
            return;
        }
        // The hold turned into a drag after all. Press where the finger was
        // held, so the rubber band is anchored there and not where it crossed
        // the threshold.
        m_pressDeferred = false;
        sendSyntheticMouse(QEvent::MouseButtonPress, m_deferredPressPosition, Qt::LeftButton,
                           Qt::LeftButton);
    }
    const auto delta = event.position - m_lastStreamPosition;
    if (m_panStreamActive) {
        const auto timestamp = now();
        const auto dt = static_cast<double>(timestamp - m_panTimestamp) / 1000.0;
        if (dt > 0.0) {
            const QPointF instant(delta.x() / dt, delta.y() / dt);
            m_panVelocity = m_panVelocity * panVelocitySmoothing +
                            instant * (1.0 - panVelocitySmoothing);
            m_panTimestamp = timestamp;
        }
        m_lastStreamPosition = event.position;
        m_target->panTouchViewportBy(delta);
        return;
    }
    m_lastStreamPosition = event.position;
    if (m_syntheticStreamActive)
        sendSyntheticMouse(QEvent::MouseMove, event.position, Qt::NoButton, Qt::LeftButton);
}

void EditorTouchController::onSingleEnd(const EditorTouchGesture::Event &event) {
    if (m_pressDeferred) {
        // Held over blank canvas and never travelled, so nothing was ever
        // pressed. Let the menu happen and leave the selection alone.
        m_pressDeferred = false;
        armContextMenuFallback(m_deferredPressPosition);
        finishStream();
        return;
    }
    if (m_panStreamActive) {
        m_panStreamActive = false;
        startInertia(m_panVelocity);
        return;
    }
    if (m_syntheticStreamActive)
        sendSyntheticMouse(QEvent::MouseButtonRelease, event.position, Qt::LeftButton,
                           Qt::NoButton);
    finishStream();
}

void EditorTouchController::onSingleCancel() {
    if (m_panStreamActive) {
        m_panStreamActive = false;
        return;
    }
    if (m_pressDeferred) {
        m_pressDeferred = false;
        finishStream();
        return;
    }
    if (m_syntheticStreamActive)
        m_target->cancelTouchPointerInteraction();
    finishStream();
}

void EditorTouchController::finishStream() {
    if (m_syntheticStreamActive) {
        m_syntheticStreamActive = false;
        EditorPointer::endTouchStream();
    }
}

void EditorTouchController::onLongPress(const EditorTouchGesture::Event &event) {
    if (m_target->touchHitsContent(event.position)) {
        // The finger is spent: it must not drag the object it is resting on.
        // The menu itself is left to the platform, so that the press-and-hold
        // feedback and the open-on-release timing match every other Windows
        // surface. The fallback below only runs where the platform has no such
        // gesture of its own.
        armContextMenuFallback(event.position);
        dispatch(m_gesture.confirmLongPress(false, now()));
        return;
    }
    // Blank area: a held press starts the selection drag straight away, so the
    // rubber band shows up under the finger instead of waiting for a release.
    dispatch(m_gesture.confirmLongPress(true, now()));
}

void EditorTouchController::sendSyntheticMouse(const QEvent::Type type, const QPointF &position,
                                               const Qt::MouseButton button,
                                               const Qt::MouseButtons buttons) {
    auto *target = m_eventTarget ? m_eventTarget.data() : m_widget.data();
    if (!target)
        return;
    const auto global = target->mapToGlobal(position);
    // The touch device travels with the event: that is how the rest of the
    // codebase recognizes a finger-driven mouse stream (EditorPointer).
    QMouseEvent event(type, position, position, global, button, buttons,
                      QApplication::keyboardModifiers(),
                      m_device ? m_device : QPointingDevice::primaryPointingDevice());
    QCoreApplication::sendEvent(target, &event);
}

void EditorTouchController::postContextMenu(const QPointF &position) {
    auto *target = m_eventTarget ? m_eventTarget.data() : m_widget.data();
    if (!target)
        return;
    const auto global = target->mapToGlobal(position).toPoint();
    // Posted, never sent: context menus run a nested event loop through exec(),
    // and doing that from inside touch delivery wedges the gesture stream.
    QCoreApplication::postEvent(
        target, new QContextMenuEvent(QContextMenuEvent::Other, position.toPoint(), global,
                                      QApplication::keyboardModifiers()));
}

void EditorTouchController::startInertia(const QPointF &velocity) {
    const auto speed = std::hypot(velocity.x(), velocity.y());
    if (speed < m_gesture.config().inertiaMinVelocityPxPerSec) {
        stopInertia();
        return;
    }
    m_inertiaVelocity = velocity;
    m_inertiaTimestamp = now();
    m_inertiaTimer->start();
}

void EditorTouchController::stopInertia() {
    m_inertiaTimer->stop();
    m_inertiaVelocity = {};
}

void EditorTouchController::onInertiaFrame() {
    if (!m_target || !m_widget || !m_widget->isVisible()) {
        stopInertia();
        return;
    }
    const auto timestamp = now();
    const auto dt = static_cast<double>(timestamp - m_inertiaTimestamp) / 1000.0;
    m_inertiaTimestamp = timestamp;
    if (dt <= 0.0)
        return;

    m_target->panTouchViewportBy(m_inertiaVelocity * dt);
    m_inertiaVelocity *= std::exp(-inertiaDecayPerSecond * dt);
    if (std::hypot(m_inertiaVelocity.x(), m_inertiaVelocity.y()) < inertiaStopSpeed)
        stopInertia();
}

void EditorTouchController::cancel() {
    disarmLongPressTimer();
    stopInertia();
    cancelContextMenuFallback();
    const auto events = m_gesture.cancelled();
    for (const auto &event : events) {
        if (event.type == EditorTouchGesture::Event::Type::SingleCancel)
            onSingleCancel();
    }
    m_panStreamActive = false;
    if (m_syntheticStreamActive) {
        if (!m_pressDeferred)
            m_target->cancelTouchPointerInteraction();
        m_pressDeferred = false;
        finishStream();
    }
}
