#include "ItemViewTouchFilter.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QScreen>
#include <QScroller>
#include <QScrollerProperties>
#include <QWidget>

namespace {
    // Marks a viewport whose item view already runs this filter, so install()
    // stays idempotent.
    const char kInstalledProperty[] = "lite_itemViewTouch";

    // Tap vs scroll boundary in meters, shared by the replay threshold and the
    // scroller's DragStartDistance (see install()): a touch stream is either a
    // tap or a scroll, never neither. The Qt default of 5 mm is too sluggish
    // for a list; 2 mm matches the mobile tap slop (same as the combo popup).
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

ItemViewTouchFilter::ItemViewTouchFilter(QWidget *viewport)
    : QObject(viewport), m_viewport(viewport) {
}

void ItemViewTouchFilter::install(QAbstractItemView *view) {
    if (!view || !view->viewport())
        return;
    auto *viewport = view->viewport();
    if (viewport->property(kInstalledProperty).toBool())
        return;
    viewport->setProperty(kInstalledProperty, true);

    // Keep the scroller's own drag threshold at the replay threshold so a slow
    // short drag cannot scroll AND replay a tap on release. No-op when the
    // touch-kinetic scroller is not attached yet.
    if (QScroller::hasScroller(viewport)) {
        auto *scroller = QScroller::scroller(viewport);
        QScrollerProperties properties = scroller->scrollerProperties();
        properties.setScrollMetric(QScrollerProperties::DragStartDistance, kTapDragDistance);
        scroller->setScrollerProperties(properties);
    }

    viewport->installEventFilter(new ItemViewTouchFilter(viewport));
}

bool ItemViewTouchFilter::eventFilter(QObject *watched, QEvent *event) {
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

            // Touch-derived mouse (ByQt synthesized while the touch is unclaimed,
            // BySystem from the OS legacy path): never forward it to the item view,
            // the press would select the item under the finger. Scrolling is driven
            // by QScroller's own touch recognizer; this stream only feeds the
            // tap/drag discrimination below.
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
                    }
                    break;
                case QEvent::MouseMove:
                    if (m_streaming && !m_moved) {
                        // The same measure QScroller applies to DragStartDistance
                        // (qscroller.cpp: per-axis delta / pixelPerMeter, then
                        // manhattanLength), so the boundary matches exactly.
                        const auto delta = pos - m_pressPosition;
                        const auto ppm = pixelPerMeter(m_viewport);
                        m_moved =
                            QPointF(delta.x() / ppm.x(), delta.y() / ppm.y()).manhattanLength() >
                            kTapDragDistance;
                    }
                    break;
                case QEvent::MouseButtonRelease:
                    if (m_streaming) {
                        if (!m_moved)
                            replayTap(pos, mouse->globalPosition(), mouse->timestamp());
                        m_streaming = false;
                    }
                    break;
                default: // double-click fallout from fast taps
                    break;
            }
            return true;
        }

        case QEvent::TouchBegin:
        case QEvent::TouchCancel:
            // Real touch passes through untouched (QScroller's recognizer needs
            // it); these only bound the stream tracking, because a cancelled
            // stream never sees a synthesized release.
            m_streaming = false;
            break;

        default:
            break;
    }
    return QObject::eventFilter(watched, event);
}

void ItemViewTouchFilter::replayTap(const QPointF &position, const QPointF &globalPosition,
                                    ulong timestamp) {
    if (m_viewport.isNull())
        return;
    // NotSynthesized so no other filter treats it as touch fallout.
    QMouseEvent press(QEvent::MouseButtonPress, position, globalPosition, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    press.setTimestamp(timestamp);
    QCoreApplication::sendEvent(m_viewport.data(), &press);

    QMouseEvent release(QEvent::MouseButtonRelease, position, globalPosition, Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    release.setTimestamp(timestamp);
    QCoreApplication::sendEvent(m_viewport.data(), &release);
}
