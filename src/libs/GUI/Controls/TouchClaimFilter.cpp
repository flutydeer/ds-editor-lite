#include "TouchClaimFilter.h"

#include <QAbstractScrollArea>
#include <QCoreApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QScroller>
#include <QTouchEvent>
#include <QWidget>

#include <utility>

namespace {
    // Marks a widget whose touch stream is already owned by a claim filter,
    // so install() stays idempotent across claim sweeps.
    const char kClaimedProperty[] = "lite_touchClaimed";
}

TouchClaimFilter::TouchClaimFilter(QWidget *target, HitTest hitTest, CancelNotice cancelNotice)
    : QObject(target), m_target(target), m_hitTest(std::move(hitTest)),
      m_cancelNotice(std::move(cancelNotice)) {
}

void TouchClaimFilter::install(QWidget *target, HitTest hitTest, CancelNotice cancelNotice) {
    if (!target || target->property(kClaimedProperty).toBool())
        return;
    target->setProperty(kClaimedProperty, true);
    // Touch delivery requires the attribute; without it the platform would
    // synthesize mouse events on its own instead of routing through here.
    target->setAttribute(Qt::WA_AcceptTouchEvents);
    target->installEventFilter(
        new TouchClaimFilter(target, std::move(hitTest), std::move(cancelNotice)));
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

    // A stream the hit test turned away is not ours: every event of it has to
    // pass through untouched, or the ancestor scroller that took it over would
    // lose the drag and the release Qt synthesized for the tap would be
    // stranded.
    //
    // A latch, and one that also drops on the next TouchBegin: the rejected
    // stream is normally taken over by an ancestor - that is what "a touch on the
    // row falls through and scrolls the page" means - so its release is delivered
    // there instead of here. Waiting for an end that never arrives left the claim
    // switched off for good after a single unclaimed touch, and the handle then
    // did nothing at all.
    if (m_rejected) {
        if (type != QEvent::TouchBegin) {
            if (type != QEvent::TouchUpdate)
                m_rejected = false;
            return QObject::eventFilter(watched, event);
        }
        m_rejected = false;
    }

    const auto &point = touchEvent->points().constFirst();

    QEvent::Type mouseType = QEvent::None;
    Qt::MouseButton button = Qt::NoButton;
    Qt::MouseButtons buttons = Qt::NoButton;
    switch (type) {
        case QEvent::TouchBegin:
            if (m_hitTest && !m_hitTest(point.position())) {
                m_rejected = true;
                return QObject::eventFilter(watched, event);
            }
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
            mouseType = QEvent::MouseButtonRelease;
            button = Qt::LeftButton;
            break;
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

    // The replay carries the touch device, not the primary pointer: the events
    // genuinely originate from the touchscreen, and views distinguish a
    // touch-initiated press from a mouse press by exactly this field.
    QMouseEvent mouseEvent(mouseType, point.position(), point.globalPosition(), button, buttons,
                           touchEvent->modifiers(), touchEvent->pointingDevice());
    mouseEvent.setTimestamp(touchEvent->timestamp());
    // A cancel of a claimed stream still ends as a replayed release - the
    // target's pressed state must not stick - but the target hears about it
    // first, so it can drop what the release would otherwise commit. Only a
    // release that will actually be replayed notifies: a cancel with no
    // replayed press behind it would strand the notice with no release to
    // consume it.
    if (type == QEvent::TouchCancel && m_pressed && m_cancelNotice)
        m_cancelNotice();
    QCoreApplication::sendEvent(m_target.data(), &mouseEvent);
    if (mouseType == QEvent::MouseButtonRelease)
        m_pressed = false;
    if (type == QEvent::TouchBegin)
        stopAncestorScroller();
    event->accept();
    return true;
}

// The gesture manager intercepts touch events in QApplication::notify before
// any widget event filter runs, and it collects gesture contexts from the whole
// ancestor chain - not just the widget under the finger. A QScroller grabbed on
// any ancestor viewport therefore sees touches aimed at this claimed widget and
// would drag that area while the claim drives the widget. Pinning those
// scrollers back to Inactive right after the press neutralizes them: an
// InputMove has no handler while the scroller is Inactive, so the rest of the
// stream is a no-op (input/state table in qscroller.cpp).
//
// Every ancestor has to be pinned, not just the nearest one. isAncestorOf()
// counts a widget as its own ancestor (qwidget.cpp:8931), so when the claim
// target is itself a viewport - a list claiming its own viewport - the walk
// matches that inner scroll area first; stopping only it left the page that
// actually scrolls untouched, and the row reordered while the page scrolled.
void TouchClaimFilter::stopAncestorScroller() const {
    for (auto *parent = m_target->parentWidget(); parent; parent = parent->parentWidget()) {
        const auto *area = qobject_cast<const QAbstractScrollArea *>(parent);
        if (!area || !area->viewport()->isAncestorOf(m_target))
            continue;
        // Ask first: QScroller::scroller() would create one just to stop it, on
        // areas that were never scrolling anything.
        if (QScroller::hasScroller(area->viewport()))
            QScroller::scroller(area->viewport())->stop();
    }
}
