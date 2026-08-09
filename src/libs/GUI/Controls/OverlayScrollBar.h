#ifndef OVERLAYSCROLLBAR_H
#define OVERLAYSCROLLBAR_H

#include <lite/GUI/Animation/IAnimatable.h>

#include <QScrollBar>

class QAbstractScrollArea;
class QVariantAnimation;

class OverlayScrollBar : public QScrollBar, public IAnimatable {
    Q_OBJECT
    Q_PROPERTY(QColor handleColor READ handleColor WRITE setHandleColor)

public:
    explicit OverlayScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr);

    void attachTo(QAbstractScrollArea *scrollArea);
    void updatePosition();

    static OverlayScrollBar *install(QAbstractScrollArea *scrollArea,
                                     Qt::Orientation orientation = Qt::Horizontal);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override;
    void afterSetTimeScale(double scale) override;

private:
    void setHighlightVisible(bool visible);
    void updateVisualState();
    void updateGeometryAnimation();
    void updateAnimationSettings();
    [[nodiscard]] QColor handleColor() const;
    void setHandleColor(const QColor &color);

    QAbstractScrollArea *m_scrollArea = nullptr;
    QVariantAnimation *m_animation = nullptr;
    QVariantAnimation *m_geometryAnimation = nullptr;
    // Base handle color; opacity is animated on top in paintEvent
    QColor m_handleColor = QColor(255, 255, 255);
    qreal m_opacity = 0.0;
    qreal m_geometryProgress = 0.0;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_targetHighlightVisible = false;
    bool m_targetGeometryVisible = false;
};

#endif // OVERLAYSCROLLBAR_H
