#ifndef OVERLAYSCROLLBAR_H
#define OVERLAYSCROLLBAR_H

#include <lite/GUI/Animation/IAnimatable.h>

#include <QScrollBar>

class QAbstractScrollArea;
class QTimer;
class QVariantAnimation;

class OverlayScrollBar : public QScrollBar, public IAnimatable {
    Q_OBJECT
    Q_PROPERTY(QColor handleColor READ handleColor WRITE setHandleColor)

public:
    explicit OverlayScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr);

    void attachTo(QAbstractScrollArea *scrollArea);
    void attachToViewport(QWidget *viewport);
    void updatePosition();
    /** Whether the bar may auto-show/hide based on range (default true). When set to
     * false it is force-hidden and later rangeChanged will not show it again. */
    void setRangeVisible(bool visible);
    /// Makes the two bars aware of each other so that, when both are shown, their
    /// tails recede at the bottom-right corner instead of overlapping.
    void setCompanion(OverlayScrollBar *companion);

    /// Reparents the bar onto a different geometry host (e.g. a popup container)
    /// while keeping the scroll area as the data source. Position follows the
    /// host widget's coordinate space instead of the scroll area's.
    void setGeometryHost(QWidget *host);

    static OverlayScrollBar *install(QAbstractScrollArea *scrollArea,
                                     Qt::Orientation orientation = Qt::Horizontal);
    static OverlayScrollBar *installOn(QWidget *viewport,
                                       Qt::Orientation orientation = Qt::Horizontal);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void afterSetAnimationEnabled(bool enabled) override;
    void afterSetTimeScale(double scale) override;

private:
    void setHighlightVisible(bool visible);
    void updateVisualState();
    void updateLayout();
    [[nodiscard]] bool willShow() const;
    void updateGeometryAnimation();
    void updateVisibilityAnimation();
    void restartHideTimer();
    void onHideTimeout();
    void pollCursor();
    void updateAnimationSettings();
    void setViewport(QWidget *viewport);
    [[nodiscard]] QColor handleColor() const;
    void setHandleColor(const QColor &color);

    QWidget *m_viewport = nullptr;
    QVariantAnimation *m_animation = nullptr;
    QVariantAnimation *m_geometryAnimation = nullptr;
    QVariantAnimation *m_visibilityAnimation = nullptr;
    QTimer *m_hideTimer = nullptr;
    QTimer *m_cursorPollTimer = nullptr;
    // Base handle color; opacity is animated on top in paintEvent
    QColor m_handleColor = QColor(255, 255, 255);
    qreal m_opacity = 0.0;
    qreal m_geometryProgress = 0.0;
    qreal m_visibility = 0.0;
    QPoint m_lastCursorPos;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_rangeVisible = true;
    bool m_idleVisible = false;
    OverlayScrollBar *m_companion = nullptr;
    QWidget *m_geometryHost = nullptr;
    bool m_targetHighlightVisible = false;
    bool m_targetGeometryVisible = false;
};

#endif // OVERLAYSCROLLBAR_H
