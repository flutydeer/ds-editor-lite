#ifndef OPTIONSUBPAGESTACK_H
#define OPTIONSUBPAGESTACK_H

#include <QParallelAnimationGroup>
#include <QPointer>
#include <QWidget>

#include <lite/GUI/Animation/IAnimatable.h>

namespace OptionSubPage {
    // Slide duration and motion constants of the sub-page transition. The
    // incoming page slides in from the right edge, the outgoing one shifts left
    // by this fraction of the width (that difference is the parallax) and dims
    // under a scrim - the phone-settings push transition, minus the
    // QGraphicsOpacityEffect (an ancestor effect would break any descendant
    // that owns one, see references/qt-graphics-effect-pitfalls.md).
    constexpr int kTransitionMs = 300;
    constexpr qreal kOutgoingShiftRatio = 0.28;
    constexpr qreal kScrimPeakDim = 0.35;
} // namespace OptionSubPage

// Dim overlay painted over the page that is leaving. It is parented to that
// page, so it rides the outgoing position animation without a second animation.
class SubPageTransitionScrim : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal dim READ dim WRITE setDim)

public:
    explicit SubPageTransitionScrim(QWidget *parent = nullptr);

    [[nodiscard]] qreal dim() const;
    void setDim(qreal dim);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_dim = 0.0;
};

// Transparent cover that swallows pointer input while a transition runs: the
// pages are moving, so a click landing mid-flight would hit whatever slid under
// the cursor.
class SubPageTransitionShield : public QWidget {
    Q_OBJECT

public:
    explicit SubPageTransitionShield(QWidget *parent = nullptr);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
};

// Opaque cover filled with the options page backdrop colour. It sits at the
// bottom of the raised page's children, so the page that slides on top blocks
// whatever is behind it: the option pages are transparent by design (they show
// the dialog backdrop), which would otherwise let the page underneath show
// through the sliding one.
class SubPageTransitionBackdrop : public QWidget {
    Q_OBJECT

public:
    explicit SubPageTransitionBackdrop(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

// Manually laid out stack of option sub-pages with the transition above. The
// pages are deliberately NOT in a layout: the stack owns their geometry, so the
// position animations cannot fight a layout manager.
//
// The stack forwards the current page's size hints to its parent, so the
// surrounding options page (a QScrollArea with widgetResizable) keeps sizing
// itself from the page that is actually visible.
class OptionSubPageStack : public QWidget, public IAnimatable {
    Q_OBJECT

public:
    // Forward (push): the incoming page slides in from the right while the
    // leaving one shifts left and dims.
    // Backward (pop): mirrored - the incoming page is revealed from the left
    // while the leaving one exits to the right, the phone-settings back.
    enum class Direction { Forward, Backward };

    explicit OptionSubPageStack(QWidget *parent = nullptr);

    void addPage(QWidget *page);
    // Switches to page: runs the transition when animated is set and animations
    // are enabled, otherwise lands on it directly.
    void setCurrentPage(QWidget *page, Direction direction, bool animated = true);
    [[nodiscard]] QWidget *currentPage() const;
    [[nodiscard]] bool isTransitioning() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void afterSetAnimationEnabled(bool enabled) override;
    void afterSetTimeScale(double scale) override;

private:
    void applyPageGeometry() const;
    void startTransition(QWidget *from, QWidget *to, Direction direction, int duration);
    // Lands on the current page: used both when a transition finishes normally
    // and when it has to be cut short (animation settings changed, page switch
    // during a running transition).
    void stopTransition();

    QList<QWidget *> m_pages;
    QWidget *m_current = nullptr;
    QWidget *m_outgoing = nullptr;
    SubPageTransitionScrim *m_scrim = nullptr;
    SubPageTransitionBackdrop *m_backdrop = nullptr;
    SubPageTransitionShield *m_shield = nullptr;
    QPointer<QParallelAnimationGroup> m_group;
    bool m_transitioning = false;
};

#endif // OPTIONSUBPAGESTACK_H
