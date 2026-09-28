#ifndef DSEDITORLITE_SMOOTHSCROLLER_H
#define DSEDITORLITE_SMOOTHSCROLLER_H

#include "WheelInputController.h"
#include "TouchClaimFilter.h"

#include <QObject>
#include <QPointer>

class QAbstractScrollArea;
class QScrollBar;
class QWheelEvent;
class QWidget;

/// Smoothly scrolls a managed scroll area: intercepts wheel events on its viewport
/// and drives the scrollbar value via an OutCubic animation. Real mouse wheels
/// (angleDelta multiples of 120) animate; touchpads (fractional/pixel deltas) pass through.
///
/// With TouchKinetic::Enabled (the default) the viewport also gets touch kinetic
/// scrolling through QScroller: single-finger drag pans and release glides, taps
/// still click child widgets via Qt's touch-to-mouse synthesis, and drag-owning
/// children (sliders) keep their gesture through TouchClaimFilter. Wheels and
/// presses stop a running glide.
class SmoothScroller : public QObject {
    Q_OBJECT

public:
    /// Whether touch kinetic scrolling is installed on the attached area.
    enum class TouchKinetic { Disabled, Enabled };

    explicit SmoothScroller(QObject *parent = nullptr);
    /// Binds a scroll area (installs this object as an event filter on its viewport).
    void attachTo(QAbstractScrollArea *area, TouchKinetic touchKinetic = TouchKinetic::Enabled);

    /// Makes \p target own its touch stream (replayed as mouse events by
    /// TouchClaimFilter) instead of touch scrolling. For widgets whose drag
    /// logic must survive inside a touch-kinetic scroll area, e.g. the
    /// SpeakerMixList reorder handle. An optional \p hitTest limits the claim to
    /// part of the target, for widgets that only own a sub-region of themselves.
    static void installClaim(QWidget *target, TouchClaimFilter::HitTest hitTest = {});

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void installTouchKinetic();
    void releaseTouchKinetic();
    void stopGlide();
    void scheduleClaimSweep();
    void sweepForClaims();

    [[nodiscard]] QScrollBar *scrollBar(Qt::Orientation orientation) const;
    [[nodiscard]] bool canAnimateScroll(Qt::Orientation orientation) const;
    [[nodiscard]] double scrollStep(Qt::Orientation orientation) const;

    QAbstractScrollArea *m_area = nullptr;
    WheelInputController m_wheelInput;
    bool m_touchKinetic = false;
    bool m_claimSweepPending = false;
    QPointer<QWidget> m_grabbedViewport;
};

#endif // DSEDITORLITE_SMOOTHSCROLLER_H
