//
// Created by fluty on 2024/2/10.
//

#include "TimeGraphicsView.h"

#include <QApplication>
#include <QCursor>
#include <QScrollBar>
#include <QWheelEvent>

#include <cmath>

#include "TimeGraphicsScene.h"
#include "TimeGridView.h"
#include "TimeIndicatorView.h"
#include "Controller/PlaybackController.h"
#include "Model/AppStatus/AppStatus.h"
#include "Model/AppOptions/AppOptions.h"
#include "Global/AppGlobal.h"

#if defined(Q_OS_MAC) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  define SUPPORTS_MOUSEWHEEL_DETECT_NATIVE
#endif

static inline bool isDirectManipulationEnabled() {
#if defined(WITH_DIRECT_MANIPULATION)
    return appOptions->appearance()->enableDirectManipulation;
#else
    return false;
#endif
}

TimeGraphicsView::TimeGraphicsView(TimeGraphicsScene *scene, bool showLastPlaybackPosition,
                                   QWidget *parent)
    : QGraphicsView(parent), m_scene(scene) {
    setRenderHint(QPainter::Antialiasing);
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_Hover);
    setMinimumHeight(150);
    setAcceptDrops(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_scaleXAnimation.setTargetObject(this);
    m_scaleXAnimation.setPropertyName("scaleX");
    m_scaleXAnimation.setEasingCurve(QEasingCurve::OutCubic);

    m_scaleYAnimation.setTargetObject(this);
    m_scaleYAnimation.setPropertyName("scaleY");
    m_scaleYAnimation.setEasingCurve(QEasingCurve::OutCubic);

    m_hBarAnimation.setTargetObject(this);
    m_hBarAnimation.setPropertyName("horizontalScrollBarValue");
    m_hBarAnimation.setEasingCurve(QEasingCurve::OutCubic);

    m_vBarAnimation.setTargetObject(this);
    m_vBarAnimation.setPropertyName("verticalScrollBarValue");
    m_vBarAnimation.setEasingCurve(QEasingCurve::OutCubic);

    const auto clearLogicalViewport = [this] {
        if (m_hBarAnimation.state() == QAbstractAnimation::Running ||
            m_vBarAnimation.state() == QAbstractAnimation::Running)
            return;
        m_logicalHorizontalBarValue.reset();
        m_logicalVerticalBarValue.reset();
    };
    connect(&m_hBarAnimation, &QPropertyAnimation::finished, this, clearLogicalViewport);
    connect(&m_vBarAnimation, &QPropertyAnimation::finished, this, clearLogicalViewport);

    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &TimeGraphicsView::notifyVisibleRectChanged);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
            &TimeGraphicsView::notifyVisibleRectChanged);

    initializeAnimation();
    updateAnimationDuration();

#ifndef SUPPORTS_MOUSEWHEEL_DETECT_NATIVE
    m_timer.setInterval(400);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this]() { m_touchPadLock = false; });
#endif

    connect(this, &TimeGraphicsView::visibleRectChanged,
            [this](const QRectF &rect) { m_scene->setVisibleRect(rect); });
    connect(this, &TimeGraphicsView::scaleChanged,
            [this](double sx, double sy) { m_scene->setScaleXY(sx, sy); });
    connect(m_scene, &TimeGraphicsScene::baseSizeChanged, this,
            &TimeGraphicsView::adjustScaleXToFillView);

    m_scenePlayPosIndicator = new TimeIndicatorView;
    m_scenePlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
    QPen curPlayPosPen;
    curPlayPosPen.setWidth(1);
    curPlayPosPen.setColor(m_playPosIndicatorColor);
    m_scenePlayPosIndicator->setPen(curPlayPosPen);
    m_scene->addTimeIndicator(m_scenePlayPosIndicator);

    m_sceneLastPlayPosIndicator = new TimeIndicatorView;
    m_sceneLastPlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
    QPen lastPlayPosPen;
    lastPlayPosPen.setWidth(1);
    lastPlayPosPen.setColor(m_lastPlayPosIndicatorColor);
    lastPlayPosPen.setStyle(Qt::DashLine);
    m_sceneLastPlayPosIndicator->setPen(lastPlayPosPen);
    if (showLastPlaybackPosition)
        m_scene->addTimeIndicator(m_sceneLastPlayPosIndicator);

    setScene(m_scene);
    setEnsureSceneFillViewX(true);

    connect(this, &TimeGraphicsView::scaleChanged, this,
            [this] { emit timeRangeChanged(startTick(), endTick()); });
    connect(this, &TimeGraphicsView::visibleRectChanged, this,
            [this] { emit timeRangeChanged(startTick(), endTick()); });

    m_positionThrottle.setSingleShot(true);
    m_positionThrottle.setInterval(33);
    connect(&m_positionThrottle, &QTimer::timeout, this, [this] {
        double tick = m_pendingPosition;
        m_playbackPosition = tick;
        if (m_scenePlayPosIndicator != nullptr)
            m_scenePlayPosIndicator->setPosition(tick);

        if (!m_autoTurnPage || appStatus->currentEditObject != AppStatus::EditObjectType::None)
            return;
        // Do not auto turn pages while edge auto scroll drives the viewport
        if (isEdgeAutoScrollActive())
            return;

        auto viewWidth = viewport()->width();
        auto hBarValue = horizontalBarValue();
        auto targetEndTick = sceneXToTick(hBarValue + viewWidth) + m_offset;
        auto tickRange = targetEndTick - sceneXToTick(hBarValue) - m_offset;

        if (m_playbackPosition > targetEndTick) {
            if (m_playbackPosition > targetEndTick + tickRange)
                setViewportStartTick(m_playbackPosition);
            else
                pageAdd();
        } else if (m_playbackPosition < startTick())
            setViewportStartTick(m_playbackPosition);
    });

    connect(&m_edgeAutoScroller, &EdgeAutoScroller::frame, this,
            &TimeGraphicsView::onEdgeAutoScrollTimerFrame);
}

TimeGraphicsScene *TimeGraphicsView::scene() {
    return m_scene;
}

void TimeGraphicsView::setGridItem(TimeGridView *item) {
    if (m_gridItem)
        m_scene->removeCommonItem(m_gridItem);
    m_gridItem = item;
    m_scene->addTimeGrid(item);
    m_gridItem->setOffset(m_offset);
    setBarLineColor(barLineColor());
    setBeatLineColor(beatLineColor());
    setCommonLineColor(commonLineColor());
}

void TimeGraphicsView::setSceneVisibility(bool on) {
    setVisible(on);
}

qreal TimeGraphicsView::scaleXMax() const {
    return m_scaleXMax;
}

void TimeGraphicsView::setScaleXMax(qreal max) {
    m_scaleXMax = max;
}

double TimeGraphicsView::scaleYMin() const {
    return m_scaleYMin;
}

void TimeGraphicsView::setScaleYMin(double min) {
    m_scaleYMin = min;
}

int TimeGraphicsView::horizontalBarValue() const {
    return horizontalScrollBar()->value();
}

void TimeGraphicsView::setHorizontalBarValue(const int value) {
    if (m_hBarAnimation.state() != QAbstractAnimation::Running)
        m_logicalHorizontalBarValue.reset();
    horizontalScrollBar()->setValue(value);
}

int TimeGraphicsView::verticalBarValue() const {
    return verticalScrollBar()->value();
}

void TimeGraphicsView::setVerticalBarValue(const int value) {
    if (m_vBarAnimation.state() != QAbstractAnimation::Running)
        m_logicalVerticalBarValue.reset();
    verticalScrollBar()->setValue(value);
}

void TimeGraphicsView::horizontalBarAnimateTo(int value) {
    m_hBarAnimation.stop();
    m_hBarAnimation.setStartValue(horizontalBarValue());
    m_hBarAnimation.setEndValue(value);
    m_hBarAnimation.start();
}

void TimeGraphicsView::verticalBarAnimateTo(int value) {
    m_vBarAnimation.stop();
    m_vBarAnimation.setStartValue(verticalBarValue());
    m_vBarAnimation.setEndValue(value);
    m_vBarAnimation.start();
}

void TimeGraphicsView::horizontalBarVBarAnimateTo(int hValue, int vValue) {
    m_hBarAnimation.stop();
    m_vBarAnimation.stop();

    m_hBarAnimation.setStartValue(horizontalBarValue());
    m_hBarAnimation.setEndValue(hValue);
    m_vBarAnimation.setStartValue(verticalBarValue());
    m_vBarAnimation.setEndValue(vValue);

    m_hBarAnimation.start();
    m_vBarAnimation.start();
}

QRectF TimeGraphicsView::visibleRect() const {
    auto viewportRect = viewport()->rect();
    auto leftTop = mapToScene(viewportRect.left(), viewportRect.top());
    auto rightBottom = mapToScene(viewportRect.width(), viewportRect.height());
    auto rect = QRectF(leftTop, rightBottom);
    return rect;
}

QRectF TimeGraphicsView::logicalVisibleRect() const {
    const auto rect = visibleRect();
    if ((!m_logicalHorizontalBarValue.has_value() && !m_logicalVerticalBarValue.has_value()) ||
        scaleX() <= 0 || scaleY() <= 0)
        return rect;

    const auto horizontalValue = m_logicalHorizontalBarValue.value_or(horizontalBarValue());
    const auto verticalValue = m_logicalVerticalBarValue.value_or(verticalBarValue());
    return rect.translated((horizontalValue - horizontalBarValue()) / scaleX(),
                           (verticalValue - verticalBarValue()) / scaleY());
}

void TimeGraphicsView::ensureSceneRectVisible(const QRectF &rect, const int xmargin,
                                              const int ymargin, const bool animated) {
    stopViewportAnimations();
    if (!animated || (m_hBarAnimation.duration() <= 0 && m_vBarAnimation.duration() <= 0)) {
        QGraphicsView::ensureVisible(rect, xmargin, ymargin);
        return;
    }

    const auto currentHorizontalValue = horizontalBarValue();
    const auto currentVerticalValue = verticalBarValue();
    QGraphicsView::ensureVisible(rect, xmargin, ymargin);
    const auto targetHorizontalValue = horizontalBarValue();
    const auto targetVerticalValue = verticalBarValue();
    setHorizontalBarValue(currentHorizontalValue);
    setVerticalBarValue(currentVerticalValue);

    m_logicalHorizontalBarValue = targetHorizontalValue;
    m_logicalVerticalBarValue = targetVerticalValue;
    if (targetHorizontalValue != currentHorizontalValue)
        horizontalBarAnimateTo(targetHorizontalValue);
    if (targetVerticalValue != currentVerticalValue)
        verticalBarAnimateTo(targetVerticalValue);
}

void TimeGraphicsView::setEnsureSceneFillViewX(bool on) {
    m_ensureSceneFillViewX = on;
}

void TimeGraphicsView::setEnsureSceneFillViewY(bool on) {
    m_ensureSceneFillViewY = on;
}

TimeGraphicsView::DragBehavior TimeGraphicsView::dragBehavior() const {
    return m_dragBehavior;
}

void TimeGraphicsView::setDragBehavior(DragBehavior dragBehaviour) {
    m_dragBehavior = dragBehaviour;
    // TODO: 实现手型拖动视图
}

void TimeGraphicsView::setScrollBarVisibility(Qt::Orientation orientation, bool visibility) {
    if (auto commonScene = scene()) {
        if (orientation == Qt::Horizontal) {
            commonScene->setHorizontalBarVisibility(visibility);
        } else
            commonScene->setVerticalBarVisibility(visibility);
    } else
        qCritical() << "Scene is not TimeGraphicsView";
}

void TimeGraphicsView::notifyVisibleRectChanged() {
    emit visibleRectChanged(visibleRect());
}

void TimeGraphicsView::onWheelHorScale(QWheelEvent *event) {
    auto cursorPos = event->position().toPoint();
    auto scenePos = mapToScene(cursorPos);

    auto deltaY = event->angleDelta().y();

    auto targetScaleX = scaleX();
    if (deltaY > 0)
        targetScaleX = scaleX() * (1 + m_hZoomingStep * deltaY / 120);
    else if (deltaY < 0)
        targetScaleX = scaleX() / (1 + m_hZoomingStep * -deltaY / 120);

    if (targetScaleX > m_scaleXMax)
        targetScaleX = m_scaleXMax;

    auto scaledSceneWidth = sceneRect().width() * (targetScaleX / scaleX());
    if (scaledSceneWidth < viewport()->width()) {
        auto targetSceneWidth = viewport()->width();
        targetScaleX = targetSceneWidth / (sceneRect().width() / scaleX());
    }

    auto ratio = targetScaleX / scaleX();
    auto targetSceneX = scenePos.x() * ratio;
    auto targetValue = qRound(targetSceneX - cursorPos.x());
    if (isDirectManipulationEnabled() || !isMouseEventFromWheel(event)) {
        setScaleX(targetScaleX);
        setHorizontalBarValue(targetValue);
    } else {
        m_scaleXAnimation.stop();
        m_scaleXAnimation.setStartValue(scaleX());
        m_scaleXAnimation.setEndValue(targetScaleX);
        m_scaleXAnimation.start();

        horizontalBarAnimateTo(targetValue);
    }
}

void TimeGraphicsView::onWheelVerScale(QWheelEvent *event) {
    auto cursorPos = event->position().toPoint();
    auto scenePos = mapToScene(cursorPos);

    auto deltaX = event->angleDelta().x();
    auto targetScaleY = scaleY();
    if (deltaX > 0)
        targetScaleY = scaleY() * (1 + m_vZoomingStep * deltaX / 120);
    else if (deltaX < 0)
        targetScaleY = scaleY() / (1 + m_vZoomingStep * -deltaX / 120);

    if (targetScaleY < m_scaleYMin)
        targetScaleY = m_scaleYMin;
    else if (targetScaleY > m_scaleYMax)
        targetScaleY = m_scaleYMax;

    auto scaledSceneHeight = sceneRect().height() * (targetScaleY / scaleY());
    if (m_ensureSceneFillViewY && scaledSceneHeight < viewport()->height()) {
        auto targetSceneHeight = viewport()->height();
        targetScaleY = targetSceneHeight / (sceneRect().height() / scaleY());
    }

    auto ratio = targetScaleY / scaleY();
    auto targetSceneY = scenePos.y() * ratio;
    auto targetValue = qRound(targetSceneY - cursorPos.y());
    if (isDirectManipulationEnabled() || !isMouseEventFromWheel(event)) {
        setScaleY(targetScaleY);
        setVerticalBarValue(targetValue);
    } else {
        m_scaleYAnimation.stop();
        m_scaleYAnimation.setStartValue(scaleY());
        m_scaleYAnimation.setEndValue(targetScaleY);
        m_scaleYAnimation.start();

        verticalBarAnimateTo(targetValue);
    }
}

void TimeGraphicsView::onWheelHorScroll(QWheelEvent *event) {
    auto deltaY = event->angleDelta().y();
    auto scrollLength = -1 * viewport()->width() * 0.2 * deltaY / 120;
    auto startValue = horizontalBarValue();
    auto endValue = static_cast<int>(startValue + scrollLength);
    if (isDirectManipulationEnabled() || !isMouseEventFromWheel(event))
        setHorizontalBarValue(endValue);
    else {
        horizontalBarAnimateTo(endValue);
    }
}

void TimeGraphicsView::onWheelVerScroll(QWheelEvent *event) {
    auto deltaY = event->angleDelta().y();
    if (isDirectManipulationEnabled() || !isMouseEventFromWheel(event)) {
        QGraphicsView::wheelEvent(event);
    } else {
        auto scrollLength = -1 * viewport()->height() * 0.15 * deltaY / 120;
        auto startValue = verticalBarValue();
        auto endValue = static_cast<int>(startValue + scrollLength);
        verticalBarAnimateTo(endValue);
    }
}

void TimeGraphicsView::adjustScaleXToFillView() {
    if (sceneRect().width() < viewport()->width()) {
        auto targetSceneWidth = viewport()->width();
        auto targetScaleX = targetSceneWidth / (sceneRect().width() / scaleX());
        setScaleX(targetScaleX);
        qDebug() << "Scene width < viewport width, adjust scaleX to" << targetScaleX;
    }
}

void TimeGraphicsView::adjustScaleYToFillView() {
    if (sceneRect().height() < viewport()->height()) {
        auto targetSceneHeight = viewport()->height();
        auto targetScaleY = targetSceneHeight / (sceneRect().height() / scaleY());
        setScaleY(targetScaleY);
        qDebug() << "Scene height < viewport height, adjust scaleY to" << targetScaleY;
    }
}

void TimeGraphicsView::dragEnterEvent(QDragEnterEvent *event) {
    QGraphicsView::dragEnterEvent(event);
    event->ignore();
}

void TimeGraphicsView::dragMoveEvent(QDragMoveEvent *event) {
    QGraphicsView::dragMoveEvent(event);
    event->ignore();
}

void TimeGraphicsView::dragLeaveEvent(QDragLeaveEvent *event) {
    QGraphicsView::dragLeaveEvent(event);
    event->ignore();
}

bool TimeGraphicsView::event(QEvent *event) {
    // Touchpad smooth zooming
    if (event->type() == QEvent::NativeGesture) {
        auto gestureEvent = static_cast<QNativeGestureEvent *>(event);

        if (gestureEvent->gestureType() == Qt::ZoomNativeGesture) {
            stopViewportAnimations();
            auto cursorGlobalPos = gestureEvent->globalPosition().toPoint();
            auto cursorPos = mapFromGlobal(cursorGlobalPos);
            auto scenePos = mapToScene(cursorPos);

            auto multiplier = gestureEvent->value() + 1;

            // Prevent negative zoom factors
            if (multiplier <= 0) {
                return true;
            }

            auto targetScaleX = scaleX() * multiplier;

            targetScaleX = qMin(targetScaleX, scaleXMax());

            auto scaledSceneWidth = sceneRect().width() * (targetScaleX / scaleX());
            if (scaledSceneWidth < viewport()->width()) {
                auto targetSceneWidth = viewport()->width();
                targetScaleX = targetSceneWidth / (sceneRect().width() / scaleX());
            }

            auto ratio = targetScaleX / scaleX();
            auto targetSceneX = scenePos.x() * ratio;
            auto targetValue = qRound(targetSceneX - cursorPos.x());

            setScaleX(targetScaleX);
            setHorizontalBarValue(targetValue);

            return true;
        }
    }

    if (event->type() == QEvent::HoverEnter)
        handleHoverEnterEvent(dynamic_cast<QHoverEvent *>(event));
    else if (event->type() == QEvent::HoverLeave)
        handleHoverLeaveEvent(dynamic_cast<QHoverEvent *>(event));
    else if (event->type() == QEvent::HoverMove)
        handleHoverMoveEvent(dynamic_cast<QHoverEvent *>(event));
    return QGraphicsView::event(event);
}

void TimeGraphicsView::wheelEvent(QWheelEvent *event) {
    stopViewportAnimations();
    if (event->modifiers() == Qt::ControlModifier) {
        onWheelHorScale(event);
    } else if (event->modifiers() == Qt::AltModifier) {
        onWheelVerScale(event);
    } else if (event->modifiers() == Qt::ShiftModifier) {
        onWheelHorScroll(event);
    } else if (event->modifiers() == Qt::NoModifier) {
        onWheelVerScroll(event);
    }
    notifyVisibleRectChanged();
}

void TimeGraphicsView::resizeEvent(QResizeEvent *event) {
    if (scene()) {
        if (m_ensureSceneFillViewX) {
            adjustScaleXToFillView();
        } else if (m_ensureSceneFillViewY) {
            adjustScaleYToFillView();
        }
    }

    QGraphicsView::resizeEvent(event);
    emit sizeChanged(viewport()->size());
    notifyVisibleRectChanged();
}

void TimeGraphicsView::mousePressEvent(QMouseEvent *event) {
    stopViewportAnimations();
    if (scene()) {
        if (auto scrollBar = scrollBarAt(event->position().toPoint())) {
            m_isDraggingScrollBar = true;
            m_draggingScrollbarType = scrollBar->orientation();
            m_mouseDownPos = event->position().toPoint();
            auto bar = scrollBar->orientation() == Qt::Horizontal ? horizontalScrollBar()
                                                                  : verticalScrollBar();
            if (scrollBar->mouseOnHandle(event->pos())) {
                scrollBar->moveToPressedState();
                m_mouseOnScrollBarHandle = true;
            }
            m_scrollBarPressed = true;
            m_mouseDownBarValue = bar->value();
            m_mouseDownBarMax = bar->maximum();
            event->ignore();
            return;
        }
    }

    const auto isSelect = m_dragBehavior == DragBehavior::RectSelect ||
                          m_dragBehavior == DragBehavior::IntervalSelect;
    if (isSelect && event->button() == Qt::LeftButton) {
        if (scene()) {
            m_isDraggingContent = true;
            if (m_dragBehavior == DragBehavior::RectSelect) {
                m_rubberBand.setSelectMode(RubberBandView::SelectMode::RectSelect);
                armEdgeAutoScroll(Qt::Horizontal | Qt::Vertical);
            } else {
                m_rubberBand.setSelectMode(RubberBandView::SelectMode::BeamSelect);
                armEdgeAutoScroll(Qt::Horizontal);
            }
            m_edgeAutoScrollPressPos = event->pos();
            m_rubberBand.mouseDown(mapToScene(event->pos()));
            m_rubberBandAdded = false;
        }
    }
    QGraphicsView::mousePressEvent(event);
    event->ignore();
}

void TimeGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDraggingScrollBar && m_mouseOnScrollBarHandle) {
        int barWidth = 14;
        auto value0 = m_mouseDownBarValue;
        auto max = m_mouseDownBarMax;
        if (max <= 0) {
            QGraphicsView::mouseMoveEvent(event);
            return;
        }
        auto ratio = 1.0 * value0 / max;
        if (m_draggingScrollbarType == Qt::Horizontal) {
            auto step = horizontalScrollBar()->pageStep();
            auto x = event->position().x();
            auto x0 = m_mouseDownPos.x();
            auto dx = x - x0;
            auto handleStep = 1.0 * step / (max + step) * (rect().width() - barWidth);
            auto scrollingLength = rect().width() - handleStep - barWidth;
            auto handleStart = ratio * scrollingLength;
            auto value = (handleStart + dx) / scrollingLength * max;
            setHorizontalBarValue(qRound(value));
        } else {
            auto step = verticalScrollBar()->pageStep();
            auto y = event->position().y();
            auto y0 = m_mouseDownPos.y();
            auto dy = y - y0;
            auto handleStep = 1.0 * step / (max + step) * (rect().height() - barWidth);
            auto scrollingLength = rect().height() - handleStep - barWidth;
            auto handleStart = ratio * scrollingLength;
            auto value = (handleStart + dy) / scrollingLength * max;
            setVerticalBarValue(qRound(value));
        }
        QGraphicsView::mouseMoveEvent(event);
        return;
    }
    if (m_isDraggingContent) {
        updateRubberBandSelection(mapToScene(event->pos()));
    }
    if (m_edgeAutoScrollArmed)
        updateEdgeAutoScrollState(event->pos());
    QGraphicsView::mouseMoveEvent(event);
}

void TimeGraphicsView::updateRubberBandSelection(const QPointF &scenePos) {
    if (!m_rubberBandAdded) {
        scene()->addCommonItem(&m_rubberBand);
        m_rubberBandAdded = true;
    }
    m_rubberBand.mouseMove(scenePos);
    QPainterPath path;
    path.addRect(QRectF(m_rubberBand.pos(), m_rubberBand.boundingRect().size()));
    scene()->setSelectionArea(path);
}

void TimeGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    // 模拟从按下切换到悬停状态
    m_scrollBarPressed = false;
    m_mouseOnScrollBarHandle = false;
    if (m_isDraggingScrollBar) {
        handleHoverEnterEvent(
            new QHoverEvent{QEvent::HoverEnter, event->position(), event->pos(), event->pos()});
    }

    m_isDraggingScrollBar = false;

    if (m_isDraggingContent) {
        if (m_rubberBandAdded)
            scene()->removeCommonItem(&m_rubberBand);
        m_isDraggingContent = false;
    }
    disarmEdgeAutoScroll();
    QGraphicsView::mouseReleaseEvent(event);
}

bool TimeGraphicsView::isMouseEventFromWheel(QWheelEvent *event) {
#ifdef SUPPORTS_MOUSEWHEEL_DETECT_NATIVE
    return event->deviceType() == QInputDevice::DeviceType::Mouse;
#else
    auto deltaX = event->angleDelta().x();
    auto deltaY = event->angleDelta().y();
    auto absDx = qAbs(deltaX);
    auto absDy = qAbs(deltaY);
    if (m_touchPadLock) {
        m_timer.start();
        return false;
    }

    // touchpad lock off
    // event might from wheel
    if ((absDx == 0 && absDy % 120 == 0) || (absDx % 120 == 0 && absDy == 0))
        return true;

    // event might from touchpad
    m_touchPadLock = true;
    m_timer.start();
    return false;
#endif
}

void TimeGraphicsView::updateAnimationDuration() {
    const int animationDurationBase = 250;
    auto duration = animationLevel() == AnimationGlobal::Full
                        ? getScaledAnimationTime(animationDurationBase)
                        : 0;
    m_scaleXAnimation.setDuration(duration);
    m_scaleYAnimation.setDuration(duration);
    m_hBarAnimation.setDuration(duration);
    m_vBarAnimation.setDuration(duration);
}

void TimeGraphicsView::handleHoverEnterEvent(QHoverEvent *event) {
    if (!scene() || m_scrollBarPressed)
        return;

    auto pos = event->position().toPoint();
    if (auto scrollBar = scrollBarAt(pos)) {
        m_prevHoveredItem = scrollBar->orientation() == Qt::Horizontal ? ItemType::HorizontalBar
                                                                       : ItemType::VerticalBar;
        if (scrollBar->mouseOnHandle(pos))
            scrollBar->moveToHoverState();
        else
            scrollBar->moveToNormalState();
    } else {
        scene()->horizontalBar()->moveToNormalState();
        scene()->verticalBar()->moveToNormalState();
        m_prevHoveredItem = ItemType::Content;
    }
}

void TimeGraphicsView::handleHoverLeaveEvent(QHoverEvent *event) {
    if (!scene() || m_scrollBarPressed)
        return;
    scene()->horizontalBar()->moveToNormalState();
    scene()->verticalBar()->moveToNormalState();
}

void TimeGraphicsView::handleHoverMoveEvent(QHoverEvent *event) {
    if (!scene() || m_scrollBarPressed)
        return;

    auto isSameBar = [&](Qt::Orientation orientation) {
        if (orientation == Qt::Horizontal && m_prevHoveredItem == ItemType::HorizontalBar)
            return true;
        if (orientation == Qt::Vertical && m_prevHoveredItem == ItemType::VerticalBar)
            return true;
        return false;
    };

    auto pos = event->position().toPoint();
    if (auto scrollBar = scrollBarAt(pos)) {
        auto orientation = scrollBar->orientation();

        if (scrollBar->mouseOnHandle(pos)) {
            scrollBar->moveToHoverState();
        } else {
            scene()->horizontalBar()->moveToNormalState();
            scene()->verticalBar()->moveToNormalState();
        }
        m_prevHoveredItem =
            orientation == Qt::Horizontal ? ItemType::HorizontalBar : ItemType::VerticalBar;
    } else {
        scene()->horizontalBar()->moveToNormalState();
        scene()->verticalBar()->moveToNormalState();
        m_prevHoveredItem = ItemType::Content;
    }
}

ScrollBarView *TimeGraphicsView::scrollBarAt(const QPoint &pos) {
    if (!scene())
        return nullptr;

    for (const auto item : items(pos))
        if (auto bar = dynamic_cast<ScrollBarView *>(item))
            return bar;
    return nullptr;
}

void TimeGraphicsView::afterSetScale() {
    emit scaleChanged(scaleX(), scaleY());
}

void TimeGraphicsView::afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) {
    updateAnimationDuration();
}

void TimeGraphicsView::afterSetTimeScale(double scale) {
    updateAnimationDuration();
}

double TimeGraphicsView::startTick() const {
    return sceneXToTick(visibleRect().left()) + m_offset;
}

double TimeGraphicsView::endTick() const {
    return sceneXToTick(visibleRect().right()) + m_offset;
}

void TimeGraphicsView::setOffset(int tick) {
    m_offset = tick;
    if (m_gridItem)
        m_gridItem->setOffset(tick);
    m_sceneLastPlayPosIndicator->setOffset(tick);
    m_scenePlayPosIndicator->setOffset(tick);
    emit timeRangeChanged(startTick(), endTick());
}

void TimeGraphicsView::setPixelsPerQuarterNote(int px) {
    m_pixelsPerQuarterNote = px;
    m_scenePlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
    m_sceneLastPlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
}

void TimeGraphicsView::setAutoTurnPage(bool on) {
    m_autoTurnPage = on;
    if (m_playbackPosition > endTick())
        pageAdd();
}

void TimeGraphicsView::setSceneLength(int tick) {
    m_baseSceneLength = tick;
    m_scene->setSceneLength(m_baseSceneLength + m_sceneLengthExtension);
}

void TimeGraphicsView::setSceneLengthExtension(int ticks) {
    if (m_sceneLengthExtension == ticks)
        return;
    m_sceneLengthExtension = ticks;
    m_scene->setSceneLength(m_baseSceneLength + m_sceneLengthExtension);
}

int TimeGraphicsView::sceneLengthExtension() const {
    return m_sceneLengthExtension;
}

void TimeGraphicsView::armEdgeAutoScroll(Qt::Orientations axes) {
    if (!m_edgeAutoScrollArmed) {
        m_edgeAutoScrollAxes = axes;
        m_edgeAutoScrollArmed = !!axes;
        m_edgeAutoScrollDistanceReached = false;
        m_edgeAutoScrollPressPos = viewport()->mapFromGlobal(QCursor::pos());
        return;
    }
    // Already armed (callers may re-arm on every move event): refresh the axes
    // and re-evaluate the hot zone, keeping the press position intact. This
    // also covers subclasses whose mouseMoveEvent returns before reaching the
    // base class implementation.
    m_edgeAutoScrollAxes = axes;
    updateEdgeAutoScrollState(viewport()->mapFromGlobal(QCursor::pos()));
}

void TimeGraphicsView::disarmEdgeAutoScroll() {
    m_edgeAutoScrollArmed = false;
    m_edgeAutoScrollDistanceReached = false;
    m_edgeAutoScroller.stop();
}

bool TimeGraphicsView::isEdgeAutoScrollActive() const {
    return m_edgeAutoScroller.isRunning();
}

void TimeGraphicsView::updateEdgeAutoScrollState(const QPoint &viewportPos) {
    if (!m_edgeAutoScrollArmed)
        return;

    if (!m_edgeAutoScrollDistanceReached) {
        const auto delta = viewportPos - m_edgeAutoScrollPressPos;
        if (delta.manhattanLength() < QApplication::startDragDistance())
            return;
        m_edgeAutoScrollDistanceReached = true;
    }

    const QRectF vpRect(QPointF(0, 0), viewport()->size());
    const auto v = EdgeAutoScroller::velocity(viewportPos, vpRect, m_edgeAutoScrollAxes,
                                              m_edgeAutoScroller.config());
    const bool inHotZone = !v.isNull();
    if (inHotZone && !m_edgeAutoScroller.isRunning()) {
        // Direct scroll bar writes must not fight the bar animations
        m_hBarAnimation.stop();
        m_vBarAnimation.stop();
        m_edgeAutoScroller.start();
    } else if (!inHotZone && m_edgeAutoScroller.isRunning()) {
        m_edgeAutoScroller.stop();
    }
}

void TimeGraphicsView::onEdgeAutoScrollTimerFrame(double dtMs) {
    // Safety net: stop if the mouse button was released without us seeing the
    // event (e.g. release outside the window swallowed by a popup).
    if (!m_edgeAutoScrollArmed || QGuiApplication::mouseButtons() == Qt::NoButton || !isVisible()) {
        disarmEdgeAutoScroll();
        return;
    }

    const auto pointerPos = QPointF(viewport()->mapFromGlobal(QCursor::pos()));
    const QRectF vpRect(QPointF(0, 0), viewport()->size());

    const auto step =
        m_edgeAutoScroller.computeStep(pointerPos, vpRect, m_edgeAutoScrollAxes, dtMs);
    if (step.x() != 0)
        setHorizontalBarValue(horizontalBarValue() + step.x());
    if (step.y() != 0)
        setVerticalBarValue(verticalBarValue() + step.y());

    const auto clamped = EdgeAutoScroller::clampToRect(pointerPos, vpRect).toPoint();
    onEdgeAutoScrollFrame(clamped, QGuiApplication::queryKeyboardModifiers());

    // Stop the timer once the pointer left the hot zone (it may re-enter later)
    updateEdgeAutoScrollState(pointerPos.toPoint());
}

void TimeGraphicsView::onEdgeAutoScrollFrame(const QPoint &clampedViewportPos,
                                             Qt::KeyboardModifiers modifiers) {
    Q_UNUSED(modifiers);
    if (m_isDraggingContent && scene())
        updateRubberBandSelection(mapToScene(clampedViewportPos));
}

void TimeGraphicsView::setPlaybackPosition(double tick) {
    m_pendingPosition = tick;
    if (!m_positionThrottle.isActive())
        m_positionThrottle.start();
}

void TimeGraphicsView::setLastPlaybackPosition(double tick) {
    m_lastPlaybackPosition = tick;
    if (m_sceneLastPlayPosIndicator != nullptr)
        m_sceneLastPlayPosIndicator->setPosition(tick);
}

void TimeGraphicsView::setViewportStartTick(double tick) {
    auto sceneX = qRound(tickToSceneX(tick - m_offset));
    setHorizontalBarValue(sceneX);
}

void TimeGraphicsView::setViewportCenterAtTick(double tick) {
    auto tickRange = endTick() - startTick();
    auto targetStart = tick - tickRange / 2;
    setViewportStartTick(targetStart);
}

bool TimeGraphicsView::setViewportScale(double horizontalScale, double verticalScale) {
    if (!std::isfinite(horizontalScale) || !std::isfinite(verticalScale) || horizontalScale <= 0 ||
        verticalScale <= 0) {
        return false;
    }

    stopViewportAnimations();

    double minimumHorizontalScale = 0.0001;
    if (m_ensureSceneFillViewX && scaleX() > 0 && sceneRect().width() > 0) {
        const auto unscaledSceneWidth = sceneRect().width() / scaleX();
        if (unscaledSceneWidth > 0)
            minimumHorizontalScale = viewport()->width() / unscaledSceneWidth;
    }

    auto minimumVerticalScale = m_scaleYMin;
    if (m_ensureSceneFillViewY && scaleY() > 0 && sceneRect().height() > 0) {
        const auto unscaledSceneHeight = sceneRect().height() / scaleY();
        if (unscaledSceneHeight > 0)
            minimumVerticalScale =
                qMax(minimumVerticalScale, viewport()->height() / unscaledSceneHeight);
    }

    const auto targetHorizontalScale =
        qMin(m_scaleXMax, qMax(minimumHorizontalScale, horizontalScale));
    const auto targetVerticalScale = qMin(m_scaleYMax, qMax(minimumVerticalScale, verticalScale));
    setScaleXY(targetHorizontalScale, targetVerticalScale);
    return true;
}

void TimeGraphicsView::stopViewportAnimations() {
    m_scaleXAnimation.stop();
    m_scaleYAnimation.stop();
    m_hBarAnimation.stop();
    m_vBarAnimation.stop();
    m_logicalHorizontalBarValue.reset();
    m_logicalVerticalBarValue.reset();
}

void TimeGraphicsView::pageAdd() {
    auto start = horizontalScrollBar()->value();
    auto end = start + horizontalScrollBar()->pageStep();
    setHorizontalBarValue(end);
}

double TimeGraphicsView::sceneXToTick(double pos) const {
    return AppGlobal::ticksPerQuarterNote * pos / scaleX() / m_pixelsPerQuarterNote;
}

double TimeGraphicsView::tickToSceneX(double tick) const {
    return tick * scaleX() * m_pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote;
}

QColor TimeGraphicsView::barLineColor() const {
    return m_barLineColor;
}

void TimeGraphicsView::setBarLineColor(const QColor &color) {
    m_barLineColor = color;
    if (m_gridItem)
        m_gridItem->setBarLineColor(m_barLineColor);
}

QColor TimeGraphicsView::beatLineColor() const {
    return m_beatLineColor;
}

void TimeGraphicsView::setBeatLineColor(const QColor &color) {
    m_beatLineColor = color;
    if (m_gridItem)
        m_gridItem->setBeatLineColor(m_beatLineColor);
}

QColor TimeGraphicsView::commonLineColor() const {
    return m_commonLineColor;
}

void TimeGraphicsView::setCommonLineColor(const QColor &color) {
    m_commonLineColor = color;
    if (m_gridItem)
        m_gridItem->setCommonLineColor(m_commonLineColor);
}

QColor TimeGraphicsView::playPosIndicatorColor() const {
    return m_playPosIndicatorColor;
}

void TimeGraphicsView::setPlayPosIndicatorColor(const QColor &color) {
    if (m_playPosIndicatorColor == color)
        return;
    m_playPosIndicatorColor = color;
    if (m_scenePlayPosIndicator) {
        auto pen = m_scenePlayPosIndicator->pen();
        pen.setColor(color);
        m_scenePlayPosIndicator->setPen(pen);
        m_scenePlayPosIndicator->update();
    }
}

QColor TimeGraphicsView::lastPlayPosIndicatorColor() const {
    return m_lastPlayPosIndicatorColor;
}

void TimeGraphicsView::setLastPlayPosIndicatorColor(const QColor &color) {
    if (m_lastPlayPosIndicatorColor == color)
        return;
    m_lastPlayPosIndicatorColor = color;
    if (m_sceneLastPlayPosIndicator) {
        auto pen = m_sceneLastPlayPosIndicator->pen();
        pen.setColor(color);
        m_sceneLastPlayPosIndicator->setPen(pen);
        m_sceneLastPlayPosIndicator->update();
    }
}

QColor TimeGraphicsView::scrollBarHandleColor() const {
    return m_scrollBarHandleColor;
}

void TimeGraphicsView::setScrollBarHandleColor(const QColor &color) {
    if (m_scrollBarHandleColor == color)
        return;
    m_scrollBarHandleColor = color;
    if (m_scene) {
        m_scene->horizontalBar()->setHandleColor(color);
        m_scene->verticalBar()->setHandleColor(color);
    }
}

QColor TimeGraphicsView::rubberBandBorderColor() const {
    return m_rubberBandBorderColor;
}

void TimeGraphicsView::setRubberBandBorderColor(const QColor &color) {
    if (m_rubberBandBorderColor == color)
        return;
    m_rubberBandBorderColor = color;
    m_rubberBand.setBorderColor(color);
}

QColor TimeGraphicsView::rubberBandFillColor() const {
    return m_rubberBandFillColor;
}

void TimeGraphicsView::setRubberBandFillColor(const QColor &color) {
    if (m_rubberBandFillColor == color)
        return;
    m_rubberBandFillColor = color;
    m_rubberBand.setFillColor(color);
}
