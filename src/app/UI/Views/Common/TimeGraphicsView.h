//
// Created by fluty on 2024/2/10.
//

#ifndef TIMEGRAPHICSVIEW_H
#define TIMEGRAPHICSVIEW_H

#include "EdgeAutoScroller.h"
#include "RubberBandView.h"
#include "UI/Utils/IAnimatable.h"
#include "UI/Utils/IScalable.h"

#include <QGraphicsView>
#include <QPropertyAnimation>
#include <QTimer>

#include <optional>

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
    Q_PROPERTY(QColor barLineColor READ barLineColor WRITE setBarLineColor)
    Q_PROPERTY(QColor beatLineColor READ beatLineColor WRITE setBeatLineColor)
    Q_PROPERTY(QColor commonLineColor READ commonLineColor WRITE setCommonLineColor)
    Q_PROPERTY(
        QColor playPosIndicatorColor READ playPosIndicatorColor WRITE setPlayPosIndicatorColor)
    Q_PROPERTY(QColor lastPlayPosIndicatorColor READ lastPlayPosIndicatorColor WRITE
                   setLastPlayPosIndicatorColor)
    Q_PROPERTY(QColor scrollBarHandleColor READ scrollBarHandleColor WRITE setScrollBarHandleColor)
    Q_PROPERTY(
        QColor rubberBandBorderColor READ rubberBandBorderColor WRITE setRubberBandBorderColor)
    Q_PROPERTY(QColor rubberBandFillColor READ rubberBandFillColor WRITE setRubberBandFillColor)

public:
    enum class DragBehavior { None, HandScroll, RectSelect, IntervalSelect };

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
    [[nodiscard]] QRectF logicalVisibleRect() const;
    void ensureSceneRectVisible(const QRectF &rect, int xmargin = 50, int ymargin = 50,
                                bool animated = false);
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
    bool setViewportScale(double horizontalScale, double verticalScale);
    void stopViewportAnimations();

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
    void setSceneLength(int tick);
    void setPlaybackPosition(double tick);
    void setLastPlaybackPosition(double tick);
    void pageAdd();

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

    // --- Edge auto scroll ---
    // Arm when a continuous drag operation begins; the scroll timer starts
    // once the pointer has travelled past startDragDistance() and enters the
    // hot zone, and stops on disarm/release/cancel.
    void armEdgeAutoScroll(Qt::Orientations axes);
    void disarmEdgeAutoScroll();
    [[nodiscard]] bool isEdgeAutoScrollActive() const;
    // Per-frame hook, called after the viewport has been scrolled. The base
    // implementation keeps rubber band selection in sync.
    virtual void onEdgeAutoScrollFrame(const QPoint &clampedViewportPos,
                                       Qt::KeyboardModifiers modifiers);

    // Temporary scene length extension on top of the externally driven base
    // length (used by drag operations that need room past the right edge).
    void setSceneLengthExtension(int ticks);
    [[nodiscard]] int sceneLengthExtension() const;

    QColor barLineColor() const;
    void setBarLineColor(const QColor &color);
    QColor beatLineColor() const;
    void setBeatLineColor(const QColor &color);
    QColor commonLineColor() const;
    void setCommonLineColor(const QColor &color);
    QColor playPosIndicatorColor() const;
    void setPlayPosIndicatorColor(const QColor &color);
    QColor lastPlayPosIndicatorColor() const;
    void setLastPlayPosIndicatorColor(const QColor &color);
    QColor scrollBarHandleColor() const;
    void setScrollBarHandleColor(const QColor &color);
    QColor rubberBandBorderColor() const;
    void setRubberBandBorderColor(const QColor &color);
    QColor rubberBandFillColor() const;
    void setRubberBandFillColor(const QColor &color);

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
    void updateRubberBandSelection(const QPointF &scenePos);
    void onEdgeAutoScrollTimerFrame(double dtMs);
    void updateEdgeAutoScrollState(const QPoint &viewportPos);

    [[nodiscard]] ScrollBarView *scrollBarAt(const QPoint &pos);

    double m_hZoomingStep = 0.4;
    double m_vZoomingStep = 0.3;
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
    std::optional<int> m_logicalHorizontalBarValue;
    std::optional<int> m_logicalVerticalBarValue;

    RubberBandView m_rubberBand;
    ItemType m_prevHoveredItem = ItemType::Content;
    bool m_scrollBarPressed = false;

    // Edge auto scroll state
    EdgeAutoScroller m_edgeAutoScroller;
    Qt::Orientations m_edgeAutoScrollAxes;
    bool m_edgeAutoScrollArmed = false;
    bool m_edgeAutoScrollDistanceReached = false;
    QPoint m_edgeAutoScrollPressPos;

    // Scene length = external base + temporary drag extension
    int m_baseSceneLength = 0;
    int m_sceneLengthExtension = 0;

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
    double m_pendingPosition = 0;
    QTimer m_positionThrottle;

    QColor m_barLineColor = {8, 9, 10};
    QColor m_beatLineColor = {22, 25, 28};
    QColor m_commonLineColor = {28, 32, 36};
    QColor m_playPosIndicatorColor = {200, 200, 200};
    QColor m_lastPlayPosIndicatorColor = {160, 160, 160};
    QColor m_scrollBarHandleColor = {255, 255, 255};
    QColor m_rubberBandBorderColor = {155, 186, 255, 200};
    QColor m_rubberBandFillColor = {155, 186, 255, 64};
};



#endif // TIMEGRAPHICSVIEW_H
