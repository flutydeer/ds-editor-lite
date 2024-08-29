//
// Created by fluty on 2024/1/23.
//

#ifndef COMMONGRAPHICSVIEW_H
#define COMMONGRAPHICSVIEW_H

#include "RubberBandGraphicsItem.h"


#include <QGraphicsView>
#include <QPropertyAnimation>
#include <QTimer>

#include "UI/Utils/IScalable.h"
#include "UI/Utils/IAnimatable.h"

class CommonGraphicsView : public QGraphicsView, public IScalable, public IAnimatable {
    Q_OBJECT
    Q_PROPERTY(double scaleX READ scaleX WRITE setScaleX)
    Q_PROPERTY(double scaleY READ scaleY WRITE setScaleY)
    Q_PROPERTY(double horizontalScrollBarValue READ hBarValue WRITE setHBarValue)
    Q_PROPERTY(double verticalScrollBarValue READ vBarValue WRITE setVBarValue)

public:
    enum class DragBehaviour { None, HandScroll, RectSelect };

    explicit CommonGraphicsView(QWidget *parent = nullptr);
    ~CommonGraphicsView() override = default;

    [[nodiscard]] double scaleXMax() const;
    void setScaleXMax(double max);
    [[nodiscard]] double scaleYMin() const;
    void setScaleYMin(double min);
    [[nodiscard]] int hBarValue() const;
    void setHBarValue(int value);
    [[nodiscard]] int vBarValue() const;
    void setVBarValue(int value);
    void hBarAnimateTo(int value);
    void vBarAnimateTo(int value);
    void hBarVBarAnimateTo(int hValue, int vValue);
    [[nodiscard]] QRectF visibleRect() const;
    void setEnsureSceneFillView(bool on);
    [[nodiscard]] DragBehaviour dragBehaviour() const;
    void setDragBehaviour(DragBehaviour dragBehaviour);

signals:
    void scaleChanged(double sx, double sy);
    void visibleRectChanged(const QRectF &rect);
    void sizeChanged(QSize size);

public slots:
    void notifyVisibleRectChanged();
    void onWheelHorScale(QWheelEvent *event);
    void onWheelVerScale(QWheelEvent *event);
    void onWheelHorScroll(QWheelEvent *event);
    void onWheelVerScroll(QWheelEvent *event);

protected:
    bool event(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void afterSetScale() override;
    void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override;
    void afterSetTimeScale(double scale) override;

private:
    using QGraphicsView::setDragMode;

    bool isMouseEventFromWheel(QWheelEvent *event);
    void updateAnimationDuration();

    double m_hZoomingStep = 0.4;
    double m_vZoomingStep = 0.3;
    // double m_scaleXMin = 0.1;
    double m_scaleXMax = 3; // 3x
    double m_scaleYMin = 0.5;
    double m_scaleYMax = 8;
    bool m_ensureSceneFillView = true;
    DragBehaviour m_dragBehaviour = DragBehaviour::None;
    bool m_isDraggingContent = false;
    bool m_rubberBandAdded = false;

    bool m_isDraggingScrollBar = false;
    Qt::Orientation m_draggingScrollbarType = Qt::Horizontal;
    QPointF m_mouseDownPos;
    int m_mouseDownBarValue = 0;
    int m_mouseDownBarMax = 0;

    QPropertyAnimation m_scaleXAnimation;
    QPropertyAnimation m_scaleYAnimation;
    QPropertyAnimation m_hBarAnimation;
    QPropertyAnimation m_vBarAnimation;

    RubberBandGraphicsItem m_rubberBand;

    QTimer m_timer;
    bool m_touchPadLock = false;
};

#endif // COMMONGRAPHICSVIEW_H
