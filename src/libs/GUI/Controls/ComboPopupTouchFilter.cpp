#include "ComboPopupTouchFilter.h"

#include "SmoothScroller.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QScroller>
#include <QScrollerProperties>
#include <QTouchEvent>
#include <QWidget>

namespace {
    // Marks a viewport whose popup already runs this filter, so install()
    // stays idempotent.
    const char kInstalledProperty[] = "lite_comboPopupTouch";

    // Tap vs scroll boundary in meters. Shared by the replay threshold and the
    // scroller's DragStartDistance (see install()): a touch stream is either a
    // tap or a scroll, never neither. The Qt default of 5 mm is too sluggish
    // for a picker-sized list; 2 mm matches the mobile tap slop.
    const auto kTapDragDistance = 0.002;

    // Fallback for screens that report no usable physical size (offscreen
    // platforms may report negative values): without it the tap threshold
    // would be garbage exactly where the unit tests run.
    const auto kFallbackDotsPerInch = 96.0;

    QPointF pixelPerMeter(const QWidget *widget) {
        const auto *screen = widget->screen();
        auto dpiX = screen ? screen->physicalDotsPerInchX() : 0.0;
        auto dpiY = screen ? screen->physicalDotsPerInchY() : 0.0;
        if (!qIsFinite(dpiX) || dpiX <= 0.0)
            dpiX = kFallbackDotsPerInch;
        if (!qIsFinite(dpiY) || dpiY <= 0.0)
            dpiY = kFallbackDotsPerInch;
        return QPointF(dpiX, dpiY) / 0.0254;
    }
} // namespace

ComboPopupTouchFilter::ComboPopupTouchFilter(QWidget *viewport)
    : QObject(viewport), m_viewport(viewport) {
}

void ComboPopupTouchFilter::install(QComboBox *combo) {
    if (!combo)
        return;
    auto *viewport = combo->view()->viewport();
    if (!viewport || viewport->property(kInstalledProperty).toBool())
        return;
    viewport->setProperty(kInstalledProperty, true);

    // Kinetic scrolling normally comes from ComboBox::initUi; a raw QComboBox
    // (the LogWindow filters) has none yet and gets the full SmoothScroller
    // here, wheel animation included.
    if (!QScroller::hasScroller(viewport)) {
        auto *smoothScroller = new SmoothScroller(combo);
        smoothScroller->attachTo(combo->view());
    }
    if (auto *scroller = QScroller::scroller(viewport)) {
        QScrollerProperties properties = scroller->scrollerProperties();
        properties.setScrollMetric(QScrollerProperties::DragStartDistance, kTapDragDistance);
        scroller->setScrollerProperties(properties);
    }

    viewport->installEventFilter(new ComboPopupTouchFilter(viewport));
}

bool ComboPopupTouchFilter::eventFilter(QObject *watched, QEvent *event) {
    if (watched != m_viewport || m_viewport.isNull())
        return QObject::eventFilter(watched, event);

    switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseMove:
        case QEvent::MouseButtonDblClick: {
            auto *mouse = static_cast<QMouseEvent *>(event);
            // Real mice and pens drive the item view unchanged. (A pen's mouse
            // stream reports NotSynthesized even where Qt derives it.)
            if (mouse->source() == Qt::MouseEventNotSynthesized)
                break;

            // Touch-derived mouse (ByQt synthesized while the touch is
            // unclaimed, BySystem from the OS legacy path): never forward it
            // to the item view - the release would run the popup container's
            // "commit and close" branch and select the item under the finger.
            // It drives the scroller and the tap replay instead.
            auto *scroller = QScroller::scroller(m_viewport);
            const auto pos = mouse->position();
            switch (event->type()) {
                case QEvent::MouseButtonPress:
                    // Windows produces two synthesized presses per touch-down
                    // (BySystem and ByQt); the second one must not reset the
                    // stream in flight.
                    if (!m_streaming) {
                        m_streaming = true;
                        m_moved = false;
                        m_pressPosition = pos;
                        scroller->handleInput(QScroller::InputPress, pos,
                                              qint64(mouse->timestamp()));
                    }
                    break;
                case QEvent::MouseMove:
                    if (m_streaming) {
                        if (!m_moved) {
                            // The same measure QScroller applies to
                            // DragStartDistance (qscroller.cpp: per-axis
                            // delta / pixelPerMeter, then manhattanLength),
                            // so the boundary matches exactly.
                            const auto delta = pos - m_pressPosition;
                            const auto ppm = pixelPerMeter(m_viewport);
                            m_moved = QPointF(delta.x() / ppm.x(), delta.y() / ppm.y())
                                          .manhattanLength() > kTapDragDistance;
                        }
                        scroller->handleInput(QScroller::InputMove, pos,
                                              qint64(mouse->timestamp()));
                    }
                    break;
                case QEvent::MouseButtonRelease:
                    if (m_streaming) {
                        // Always hand the release to the scroller first
                        // (Dragging glides, Pressed just returns to Inactive)
                        // so a tap replay cannot leave it mid-state.
                        scroller->handleInput(QScroller::InputRelease, pos,
                                              qint64(mouse->timestamp()));
                        if (!m_moved) {
                            // A tap: replayed as a genuine-looking click, the
                            // press highlights and sets the current index, the
                            // release activates the item and lets the popup
                            // container close it.
                            replayTap(pos, mouse->globalPosition(), mouse->timestamp());
                        }
                        m_streaming = false;
                    }
                    break;
                default: // double-click fallout from fast taps
                    break;
            }
            return true;
        }

        case QEvent::TouchBegin: {
            // Qt does not deliver touch into a popup's widget tree at all
            // (QWidgetWindow ignores it so the touch is synthesized into mouse
            // events instead), which is why this filter drives the scroller
            // from the mouse stream. Should a platform route real touch here
            // anyway, keep the scroller's own touch recognizer out so the
            // synthesized mouse stream stays the single input source.
            QScroller::scroller(m_viewport)->stop();
            m_streaming = false;
            event->accept();
            return true;
        }
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            event->accept();
            return true;

        default:
            break;
    }
    return QObject::eventFilter(watched, event);
}

void ComboPopupTouchFilter::replayTap(const QPointF &position, const QPointF &globalPosition,
                                      ulong timestamp) {
    if (m_viewport.isNull())
        return;
    // NotSynthesized so no other filter treats it as touch fallout.
    QMouseEvent press(QEvent::MouseButtonPress, position, globalPosition,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    press.setTimestamp(timestamp);
    QCoreApplication::sendEvent(m_viewport.data(), &press);

    QMouseEvent release(QEvent::MouseButtonRelease, position, globalPosition,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    release.setTimestamp(timestamp);
    QCoreApplication::sendEvent(m_viewport.data(), &release);
}
