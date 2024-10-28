//
// Created by fluty on 2024/2/10.
//

#ifndef TIMEGRAPHICSVIEW_H
#define TIMEGRAPHICSVIEW_H

#include "RubberBandView.h"
#include "UI/Utils/IAnimatable.h"
#include "UI/Utils/IScalable.h"

#include <QGraphicsView>
#include <QPropertyAnimation>
#include <QTimer>

class TimeGraphicsScene;
class TimeGridView;
class TimeIndicatorView;
class ScrollBarView;

class TimeGraphicsView : public QGraphicsView, public IScalable, public IAnimatable {
    Q_OBJECT
    Q_PROPERTY(double scaleX READ scaleX WRITE setScaleX)
    Q_PROPERTY(double scaleY READ scaleY WRITE setScaleY)
    Q_PROPERTY(double horizontalScrollBarValue READ horizontalBarValue WRITE setHorizontalBarValue)
    Q_PROPERTY(double verticalScrollBarValue READ verticalBarValue WRITE setVerticalBarValue)

public:
    enum class DragBehavior { None, HandScroll, RectSelect, IntervalSelect };
    // enum class ScrollBarVisibility { AlwaysVisible, AutoHide, AlwaysHide };

    explicit TimeGraphicsView(TimeGraphicsScene *scene, bool showLastPlaybackPosition = true,
                              QWidget *parent = nullptr);
    TimeGraphicsScene *scene();
    void setGridItem(TimeGridView *item);
    void setSceneVisibility(bool on);
    [[nodiscard]] double scaleXMax() const;
    void setScaleXMax(double max);
    [[nodiscard]] double scaleYMin() const;
    void setScaleYMin(double min);
    [[nodiscard]] int horizontalBarValue() const;
    void setHorizontalBarValue(int value);
    [[nodiscard]] int verticalBarValue() const;
    void setVerticalBarValue(int value);
    void horizontalBarAnimateTo(int value);
    void verticalBarAnimateTo(int value);
    void horizontalBarVBarAnimateTo(int hValue, int vValue);
    [[nodiscard]] QRectF visibleRect() const;
    void setEnsureSceneFillViewX(bool on);
    void setEnsureSceneFillViewY(bool on);
    [[nodiscard]] DragBehavior dragBehavior() const;
    void setDragBehavior(DragBehavior dragBehaviour);
    void setScrollBarVisibility(Qt::Orientation orientation, bool visibility);
    [[nodiscard]] double startTick() const;
    [[nodiscard]] double endTick() const;
    void setOffset(int tick);
    void setPixelsPerQuarterNote(int px);
    void setAutoTurnPage(bool on);
    void setViewportStartTick(double tick);
    void setViewportCenterAtTick(double tick);

signals:
    void scaleChanged(double sx, double sy);
    void visibleRectChanged(const QRectF &rect);
    void sizeChanged(QSize size);
    void timeRangeChanged(double startTick, double endTick);

public slots:
    void notifyVisibleRectChanged();
    void onWheelHorScale(QWheelEvent *event);
    void onWheelVerScale(QWheelEvent *event);
    void onWheelHorScroll(QWheelEvent *event);
    void onWheelVerScroll(QWheelEvent *event);
    void adjustScaleXToFillView();
    void adjustScaleYToFillView();
    void setSceneLength(int tick) const;
    void setPlaybackPosition(double tick);
    void setLastPlaybackPosition(double tick);
    void pageAdd();
    // void setTimeRange(double startTick, double endTick);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    bool event(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void afterSetScale() override;
    void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override;
    void afterSetTimeScale(double scale) override;

    [[nodiscard]] double sceneXToTick(double pos) const;
    [[nodiscard]] double tickToSceneX(double tick) const;

private:
    enum class ItemType { HorizontalBar, VerticalBar, Content };

    using QGraphicsView::setDragMode;
    using QGraphicsView::setHorizontalScrollBarPolicy;
    using QGraphicsView::setVerticalScrollBarPolicy;

    bool isMouseEventFromWheel(QWheelEvent *event);
    void updateAnimationDuration();
    void handleHoverEnterEvent(QHoverEvent *event);
    void handleHoverLeaveEvent(QHoverEvent *event);
    void handleHoverMoveEvent(QHoverEvent *event);

    [[nodiscard]] ScrollBarView *scrollBarAt(const QPoint &pos);

    double m_hZoomingStep = 0.4;
    double m_vZoomingStep = 0.3;
    // double m_scaleXMin = 0.1;
    double m_scaleXMax = 3; // 3x
    double m_scaleYMin = 0.5;
    double m_scaleYMax = 8;
    bool m_ensureSceneFillViewX = true;
    bool m_ensureSceneFillViewY = true;
    DragBehavior m_dragBehavior = DragBehavior::None;
    bool m_isDraggingContent = false;
    bool m_rubberBandAdded = false;

    bool m_isDraggingScrollBar = false;
    bool m_mouseOnScrollBarHandle = false;
    Qt::Orientation m_draggingScrollbarType = Qt::Horizontal;
    QPointF m_mouseDownPos;
    int m_mouseDownBarValue = 0;
    int m_mouseDownBarMax = 0;

    QPropertyAnimation m_scaleXAnimation;
    QPropertyAnimation m_scaleYAnimation;
    QPropertyAnimation m_hBarAnimation;
    QPropertyAnimation m_vBarAnimation;

    RubberBandView m_rubberBand;
    ItemType m_prevHoveredItem = ItemType::Content;
    bool m_scrollBarPressed = false;

    QTimer m_timer;
    bool m_touchPadLock = false;

    TimeGraphicsScene *m_scene;
    TimeGridView *m_gridItem = nullptr;
    TimeIndicatorView *m_scenePlayPosIndicator = nullptr;
    TimeIndicatorView *m_sceneLastPlayPosIndicator = nullptr;

    int m_offset = 0;
    int m_pixelsPerQuarterNote = 64;
    bool m_autoTurnPage = true;
    double m_playbackPosition = 0;
    double m_lastPlaybackPosition = 0;
};



#endif // TIMEGRAPHICSVIEW_H
