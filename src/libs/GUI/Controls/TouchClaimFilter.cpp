#include "TouchClaimFilter.h"

#include <QAbstractScrollArea>
#include <QCoreApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QScroller>
#include <QTouchEvent>
#include <QWidget>

namespace {
    // Marks a widget whose touch stream is already owned by a claim filter,
    // so install() stays idempotent across claim sweeps.
    const char kClaimedProperty[] = "lite_touchClaimed";
}

TouchClaimFilter::TouchClaimFilter(QWidget *target) : QObject(target), m_target(target) {
}

void TouchClaimFilter::install(QWidget *target) {
    if (!target || target->property(kClaimedProperty).toBool())
        return;
    target->setProperty(kClaimedProperty, true);
    // Touch delivery requires the attribute; without it the platform would
    // synthesize mouse events on its own instead of routing through here.
    target->setAttribute(Qt::WA_AcceptTouchEvents);
    target->installEventFilter(new TouchClaimFilter(target));
}

bool TouchClaimFilter::eventFilter(QObject *watched, QEvent *event) {
    if (watched != m_target || m_target.isNull())
        return QObject::eventFilter(watched, event);

    const auto type = event->type();
    if (type != QEvent::TouchBegin && type != QEvent::TouchUpdate && type != QEvent::TouchEnd &&
        type != QEvent::TouchCancel)
        return QObject::eventFilter(watched, event);

    const auto *touchEvent = static_cast<QTouchEvent *>(event);
    if (touchEvent->points().isEmpty())
        return true;

    QEvent::Type mouseType = QEvent::None;
    Qt::MouseButton button = Qt::NoButton;
    Qt::MouseButtons buttons = Qt::NoButton;
    switch (type) {
        case QEvent::TouchBegin:
            mouseType = QEvent::MouseButtonPress;
            button = Qt::LeftButton;
            buttons = Qt::LeftButton;
            m_pressed = true;
            break;
        case QEvent::TouchUpdate:
            mouseType = QEvent::MouseMove;
            buttons = Qt::LeftButton;
            break;
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            mouseType = QEvent::MouseButtonRelease;
            button = Qt::LeftButton;
            break;
        default:
            break;
    }
    // Updates and releases without a replayed press would leave the widget in
    // a stale pressed state; drop them.
    if (mouseType == QEvent::None || (!m_pressed && mouseType != QEvent::MouseButtonPress))
        return true;

    const auto &point = touchEvent->points().constFirst();
    QMouseEvent mouseEvent(mouseType, point.position(), point.globalPosition(), button, buttons,
                           touchEvent->modifiers());
    mouseEvent.setTimestamp(touchEvent->timestamp());
    QCoreApplication::sendEvent(m_target.data(), &mouseEvent);
    if (mouseType == QEvent::MouseButtonRelease)
        m_pressed = false;
    if (type == QEvent::TouchBegin)
        stopAncestorScroller();
    event->accept();
    return true;
}

// The gesture manager intercepts touch events in QApplication::notify before
// any widget event filter runs, so a QScroller grabbed on an ancestor viewport
// also sees touches aimed at this claimed widget and would scroll the page
// while the claim drives it. Pinning the scroller back to Inactive after the
// press neutralizes it: further InputMoves are no-ops while Inactive.
void TouchClaimFilter::stopAncestorScroller() const {
    for (auto *parent = m_target->parentWidget(); parent; parent = parent->parentWidget()) {
        const auto *area = qobject_cast<const QAbstractScrollArea *>(parent);
        if (area && area->viewport()->isAncestorOf(m_target)) {
            QScroller::scroller(area->viewport())->stop();
            return;
        }
    }
}
