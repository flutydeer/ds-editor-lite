#include "SmoothScroller.h"

#include "OverlayScrollBar.h"
#include "SvsSeekbar.h"
#include "TouchClaimFilter.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QChildEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QScroller>
#include <QScrollerProperties>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QTimer>
#include <QWheelEvent>

namespace {
    // Glide damping after a touch flick - a damping, not a duration, even though
    // QScroller derives both from it: travel = 0.5 * pixelPerMeter * velocity^2
    // / factor and duration = velocity / factor, see createScrollingSegments().
    // So a HIGHER factor is the damped direction (stops sooner and shorter) and
    // a lower one throws the page further. Qt ships 0.125, which is loose;
    // 0.30 halves the travel of the original 0.15. Tune on real hardware.
    const auto kTouchDecelerationFactor = 0.30;

    // Rubber band past the first/last edge. Qt keeps two separate overshoot
    // budgets and they have to agree. While the finger is down the page moves by
    // fingerTravel * resistance, capped at viewport * kTouchOvershootMaxDistance.
    // After release the bounce depth is capped by the same
    // viewport * OvershootScrollDistanceFactor, and that bounce takes
    // OvershootScrollTime * 0.7 s to settle. Leaving the scroll side at the Qt
    // defaults (0.5 / 0.7 s) lets a flick throw the page twice as far past the
    // edge as a drag can push it, then crawl back slowly - which reads as a loose
    // rubber band. Both sides therefore share one distance budget, and the return
    // is shortened for a damped, quick settle. Setting a distance factor to 0
    // disables the overshoot on that side entirely.
    const auto kTouchOvershootMaxDistanceFactor = 0.25;
    const auto kTouchOvershootDragResistanceFactor = 0.25;
    const auto kTouchOvershootScrollTime = 0.35;

    // Drag-owning widgets inside a touch-kinetic scroll area: their whole rect
    // is claimed so a touch drag adjusts the control instead of scrolling the
    // page (an unclaimed control would receive the synthesized mouse press and
    // jump its value while QScroller scrolls at the same time).
    bool isDragOwner(const QWidget *widget) {
        return qobject_cast<const SVS::SeekBar *>(widget) != nullptr ||
               qobject_cast<const QSlider *>(widget) != nullptr;
    }
}

SmoothScroller::SmoothScroller(QObject *parent) : QObject(parent) {
    m_wheelInput.setContinuousInputMode(WheelInputController::ContinuousInputMode::PassThrough);

    const auto installTarget = [this](const Qt::Orientation orientation) {
        m_wheelInput.setScrollTarget(
            orientation,
            {
                .value =
                    [this, orientation] {
                        const auto *bar = scrollBar(orientation);
                        return bar ? static_cast<double>(bar->value()) : 0.0;
                    },
                .setValue =
                    [this, orientation](const double value) {
                        if (auto *bar = scrollBar(orientation))
                            bar->setValue(qRound(value));
                    },
                .boundedValue =
                    [this, orientation](const double value) {
                        const auto *bar = scrollBar(orientation);
                        return bar ? static_cast<double>(
                                         qBound(bar->minimum(), qRound(value), bar->maximum()))
                                   : 0.0;
                    },
                .step = [this, orientation] { return scrollStep(orientation); },
                .canScroll = [this, orientation] { return canAnimateScroll(orientation); },
            });
    };
    installTarget(Qt::Horizontal);
    installTarget(Qt::Vertical);
}

void SmoothScroller::attachTo(QAbstractScrollArea *area, TouchKinetic touchKinetic) {
    if (m_area) {
        m_area->viewport()->removeEventFilter(this);
        m_area->removeEventFilter(this);
    }
    releaseTouchKinetic();
    m_wheelInput.stop();
    m_area = area;
    m_touchKinetic = touchKinetic == TouchKinetic::Enabled;
    auto *viewport = area->viewport();
    viewport->installEventFilter(this);
    viewport->setMouseTracking(true);
    // The area filter exists only for the claim sweep: QScrollArea::setWidget
    // parents its content to the viewport, whose ChildAdded passes through the
    // same filter object installed above.
    area->installEventFilter(this);
    if (m_touchKinetic)
        installTouchKinetic();
    scheduleClaimSweep();

    // Dragging an overlay scrollbar must cancel a running glide; the bar sits
    // outside the viewport, so its press never passes through the filter above.
    const auto overlayBars = area->findChildren<OverlayScrollBar *>();
    for (auto *bar : overlayBars)
        connect(bar, &QAbstractSlider::sliderPressed, this, &SmoothScroller::stopGlide,
                Qt::UniqueConnection);
}

void SmoothScroller::installClaim(QWidget *target) {
    TouchClaimFilter::install(target);
}

void SmoothScroller::installTouchKinetic() {
    auto *viewport = m_area->viewport();
    viewport->setAttribute(Qt::WA_AcceptTouchEvents);
    QScroller::grabGesture(viewport, QScroller::TouchGesture);
    auto *scroller = QScroller::scroller(viewport);
    QScrollerProperties properties = scroller->scrollerProperties();
    properties.setScrollMetric(QScrollerProperties::DecelerationFactor, kTouchDecelerationFactor);
    properties.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor,
                               kTouchOvershootDragResistanceFactor);
    properties.setScrollMetric(QScrollerProperties::OvershootDragDistanceFactor,
                               kTouchOvershootMaxDistanceFactor);
    properties.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor,
                               kTouchOvershootMaxDistanceFactor);
    properties.setScrollMetric(QScrollerProperties::OvershootScrollTime,
                               kTouchOvershootScrollTime);
    scroller->setScrollerProperties(properties);
    m_grabbedViewport = viewport;
}

void SmoothScroller::releaseTouchKinetic() {
    if (m_grabbedViewport) {
        QScroller::ungrabGesture(m_grabbedViewport);
        m_grabbedViewport.clear();
    }
}

void SmoothScroller::stopGlide() {
    if (m_touchKinetic && m_area) {
        if (auto *viewport = m_area->viewport())
            QScroller::scroller(viewport)->stop();
    }
}

void SmoothScroller::scheduleClaimSweep() {
    if (m_claimSweepPending || !m_touchKinetic)
        return;
    m_claimSweepPending = true;
    // One sweep per event-loop iteration: page constructors add their whole
    // widget tree synchronously, so a single queued pass sees all of it.
    QTimer::singleShot(0, this, [this] {
        m_claimSweepPending = false;
        sweepForClaims();
    });
}

void SmoothScroller::sweepForClaims() {
    if (!m_area || !m_touchKinetic)
        return;
    // Drag owners only appear inside scroll areas with a content widget
    // (QScrollArea pages); item views have nothing to claim.
    const auto *scrollArea = qobject_cast<const QScrollArea *>(m_area);
    const auto *content = scrollArea ? scrollArea->widget() : nullptr;
    if (!content)
        return;
    const auto widgets = content->findChildren<QWidget *>();
    for (auto *widget : widgets)
        if (isDragOwner(widget))
            TouchClaimFilter::install(widget);
}

bool SmoothScroller::eventFilter(QObject *watched, QEvent *event) {
    if (m_area) {
        switch (event->type()) {
            case QEvent::Wheel: {
                if (watched != m_area->viewport())
                    break;
                stopGlide();
                auto *wheelEvent = static_cast<QWheelEvent *>(event);
                // Pass through wheel events carrying modifiers: Ctrl zooms fonts
                // (PhonicTextEdit / LyricWrapView) and Shift/Alt are handled by the widget itself.
                if (wheelEvent->modifiers() == Qt::NoModifier &&
                    m_wheelInput.handleWheel(wheelEvent))
                    return true;
                break;
            }
            case QEvent::MouseButtonPress:
                // A real mouse press stops the glide. The synthesized press that
                // follows every unaccepted TouchBegin must not: QScroller owns
                // the touch stream and handles its own press handling.
                if (m_touchKinetic && watched == m_area->viewport() &&
                    static_cast<QMouseEvent *>(event)->source() ==
                        Qt::MouseEventNotSynthesized)
                    stopGlide();
                break;
            case QEvent::ChildAdded:
            case QEvent::ChildRemoved: {
                // Content swaps (QScrollArea::setWidget parents the content to
                // the viewport) rebuild the widget tree; re-scan for drag owners
                // after the dust settles.
                const auto *scrollArea = qobject_cast<const QScrollArea *>(m_area);
                if (m_touchKinetic &&
                    (watched == m_area || watched == m_area->viewport() ||
                     (scrollArea && watched == scrollArea->widget())))
                    scheduleClaimSweep();
                break;
            }
            default:
                break;
        }
    }
    return QObject::eventFilter(watched, event);
}

QScrollBar *SmoothScroller::scrollBar(const Qt::Orientation orientation) const {
    if (!m_area)
        return nullptr;
    return orientation == Qt::Horizontal ? m_area->horizontalScrollBar()
                                         : m_area->verticalScrollBar();
}

bool SmoothScroller::canAnimateScroll(const Qt::Orientation orientation) const {
    const auto *bar = scrollBar(orientation);
    if (!bar || bar->maximum() <= bar->minimum())
        return false;
    if (orientation == Qt::Vertical) {
        const auto *itemView = qobject_cast<QAbstractItemView *>(m_area);
        if (itemView && itemView->verticalScrollMode() == QAbstractItemView::ScrollPerItem)
            return false;
    }
    return true;
}

double SmoothScroller::scrollStep(const Qt::Orientation orientation) const {
    if (!m_area)
        return 0.0;
    auto step = (orientation == Qt::Horizontal ? m_area->viewport()->width()
                                               : m_area->viewport()->height()) *
                0.15;
    if (orientation == Qt::Vertical) {
        const auto *itemView = qobject_cast<QAbstractItemView *>(m_area);
        if (itemView && itemView->verticalScrollMode() == QAbstractItemView::ScrollPerPixel) {
            const auto rowHeight = itemView->sizeHintForRow(0);
            const auto scrollLines = QApplication::wheelScrollLines();
            if (rowHeight > 0 && scrollLines > 0)
                step = rowHeight * scrollLines;
        }
    }
    return step;
}
