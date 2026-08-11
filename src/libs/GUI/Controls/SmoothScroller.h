#ifndef DSEDITORLITE_SMOOTHSCROLLER_H
#define DSEDITORLITE_SMOOTHSCROLLER_H

#include <lite/GUI/Animation/IAnimatable.h>

#include <QObject>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QVector>

#include <optional>

class QAbstractScrollArea;
class QScrollBar;
class QWheelEvent;

/// Smoothly scrolls a managed scroll area: intercepts wheel events on its viewport
/// and drives the scrollbar value via an OutCubic animation. Real mouse wheels
/// (angleDelta multiples of 120) animate; touchpads (fractional/pixel deltas) pass through.
class SmoothScroller : public QObject, public IAnimatable {
    Q_OBJECT

public:
    explicit SmoothScroller(QObject *parent = nullptr);
    /// Binds a scroll area (installs this object as an event filter on its viewport).
    void attachTo(QAbstractScrollArea *area);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override;
    void afterSetTimeScale(double scale) override;

private:
    void updateAnimationDuration();

    QAbstractScrollArea *m_area = nullptr;
    QPropertyAnimation m_hAnim;
    QPropertyAnimation m_vAnim;
    // Logical scroll target while an animation is running; a new wheel event
    // stacks on top of it instead of restarting from the interrupted position.
    // Cleared when the animation finishes.
    std::optional<int> m_logicalH;
    std::optional<int> m_logicalV;
    // Sliding window of recent wheel steps: any non-120-multiple event latches
    // touchpad mode (pass-through) until that event falls out of the window.
    // Defaults to mouse animation on an empty window, mirroring the editor view.
    QVector<bool> m_stepWindow;
    static constexpr int kWindowSize = 6;
    static constexpr int kBaseMs = 250;
};

#endif // DSEDITORLITE_SMOOTHSCROLLER_H
