//
// Created by fluty on 2024/2/10.
//

#include "TimeGraphicsView.h"

#include <QScrollBar>

#include "TimeGraphicsScene.h"
#include "TimeGridView.h"
#include "TimeIndicatorView.h"
#include "Model/AppStatus/AppStatus.h"

#include <QWheelEvent>

TimeGraphicsView::TimeGraphicsView(TimeGraphicsScene *scene, bool showLastPlaybackPosition,
                                   QWidget *parent)
    : QGraphicsView(parent), m_scene(scene) {
    setRenderHint(QPainter::Antialiasing);
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_Hover);
    // setCacheMode(QGraphicsView::CacheNone);
    // setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
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

    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &TimeGraphicsView::notifyVisibleRectChanged);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
            &TimeGraphicsView::notifyVisibleRectChanged);

    initializeAnimation();
    updateAnimationDuration();

#ifndef SUPPORTS_MOUSEWHEEL_DETECT_NATIVE
    m_timer.setInterval(400);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [=]() {
        m_touchPadLock = false;
        // qDebug() << "touchpad lock off";
    });
#endif

    connect(this, &TimeGraphicsView::visibleRectChanged,
            [=](const QRectF &rect) { m_scene->setVisibleRect(rect); });
    connect(this, &TimeGraphicsView::scaleChanged,
            [=](double sx, double sy) { m_scene->setScaleXY(sx, sy); });
    connect(m_scene, &TimeGraphicsScene::baseSizeChanged, this,
            &TimeGraphicsView::adjustScaleXToFillView);

    m_scenePlayPosIndicator = new TimeIndicatorView;
    m_scenePlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
    QPen curPlayPosPen;
    curPlayPosPen.setWidth(1);
    curPlayPosPen.setColor(QColor(200, 200, 200));
    m_scenePlayPosIndicator->setPen(curPlayPosPen);
    m_scene->addTimeIndicator(m_scenePlayPosIndicator);

    m_sceneLastPlayPosIndicator = new TimeIndicatorView;
    m_sceneLastPlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
    QPen lastPlayPosPen;
    lastPlayPosPen.setWidth(1);
    lastPlayPosPen.setColor(QColor(160, 160, 160));
    lastPlayPosPen.setStyle(Qt::DashLine);
    m_sceneLastPlayPosIndicator->setPen(lastPlayPosPen);
    if (showLastPlaybackPosition)
        m_scene->addTimeIndicator(m_sceneLastPlayPosIndicator);

    setScene(m_scene);
    setEnsureSceneFillViewX(true);

    connect(this, &TimeGraphicsView::scaleChanged, this,
            [=] { emit timeRangeChanged(startTick(), endTick()); });
    connect(this, &TimeGraphicsView::visibleRectChanged, this,
            [=] { emit timeRangeChanged(startTick(), endTick()); });
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
    setScene(on ? m_scene : nullptr);
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
    horizontalScrollBar()->setValue(value);
}

int TimeGraphicsView::verticalBarValue() const {
    return verticalScrollBar()->value();
}

void TimeGraphicsView::setVerticalBarValue(const int value) {
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

    // auto deltaX = event->angleDelta().x();
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
    if (!isMouseEventFromWheel(event)) {
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
    if (!isMouseEventFromWheel(event)) {
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
    if (!isMouseEventFromWheel(event))
        setHorizontalBarValue(endValue);
    else {
        horizontalBarAnimateTo(endValue);
    }
}

void TimeGraphicsView::onWheelVerScroll(QWheelEvent *event) {
    auto deltaY = event->angleDelta().y();
    if (!isMouseEventFromWheel(event)) {
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
    // else if (event->type() == QEvent::WindowActivate)
    //     qDebug() << "Window activated";
    // else if (event->type() == QEvent::WindowDeactivate)
    //     qDebug() << "Window deactivated";
    return QGraphicsView::event(event);
}

void TimeGraphicsView::wheelEvent(QWheelEvent *event) {
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
    if (scene()) {
        if (auto scrollBar = scrollBarAt(event->position().toPoint())) {
            // auto oriStr = scrollBar->orientation() == Qt::Horizontal ? "horizontal" : "vertical";
            // qDebug() << "mouse down on" << oriStr << "scrollbar";
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
            if (m_dragBehavior == DragBehavior::RectSelect)
                m_rubberBand.setSelectMode(RubberBandView::SelectMode::RectSelect);
            else
                m_rubberBand.setSelectMode(RubberBandView::SelectMode::BeamSelect);
            m_rubberBand.mouseDown(mapToScene(event->pos()));
            m_rubberBandAdded = false;
        }
    }
    QGraphicsView::mousePressEvent(event);
    event->ignore();
}

void TimeGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    // qDebug() << "m_isDraggingScrollBar" << m_isDraggingScrollBar << "m_mouseOnScrollBarHandle"
    //          << m_mouseOnScrollBarHandle << "m_isDraggingContent" << m_isDraggingContent;
    if (m_isDraggingScrollBar && m_mouseOnScrollBarHandle) {
        int barWidth = 14;
        auto value0 = m_mouseDownBarValue;
        auto max = m_mouseDownBarMax;
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
            // qDebug() << "Move horizontal bar: " << horizontalScrollBar()->value();
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
        if (!m_rubberBandAdded) {
            scene()->addCommonItem(&m_rubberBand);
            m_rubberBandAdded = true;
        }
        m_rubberBand.mouseMove(mapToScene(event->pos()));
        QPainterPath path;
        path.addRect(QRectF(m_rubberBand.pos(), m_rubberBand.boundingRect().size()));
        scene()->setSelectionArea(path);
    }
    QGraphicsView::mouseMoveEvent(event);
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
    // qDebug() << pos;
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
        // qDebug() << scrollBar->orientation();
        auto orientation = scrollBar->orientation();

        if (scrollBar->mouseOnHandle(pos)) {
            // qDebug() << "mouseOnHandle true";
            scrollBar->moveToHoverState();
        } else {
            // qDebug() << "mouseOnHandle false";
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

void TimeGraphicsView::setSceneLength(int tick) const {
    m_scene->setSceneLength(tick);
}

void TimeGraphicsView::setPlaybackPosition(double tick) {
    m_playbackPosition = tick;
    if (m_scenePlayPosIndicator != nullptr)
        m_scenePlayPosIndicator->setPosition(tick);

    if (!m_autoTurnPage || appStatus->currentEditObject != AppStatus::EditObjectType::None)
        return;

    if (m_playbackPosition > endTick()) {
        pageAdd();
    } else if (m_playbackPosition < startTick())
        setViewportStartTick(m_playbackPosition);
}

void TimeGraphicsView::setLastPlaybackPosition(double tick) {
    m_lastPlaybackPosition = tick;
    if (m_sceneLastPlayPosIndicator != nullptr)
        m_sceneLastPlayPosIndicator->setPosition(tick);
}

void TimeGraphicsView::setViewportStartTick(double tick) {
    auto sceneX = qRound(tickToSceneX(tick - m_offset));
    // horizontalScrollBar()->setValue(sceneX);
    horizontalBarAnimateTo(sceneX);
}

void TimeGraphicsView::setViewportCenterAtTick(double tick) {
    auto tickRange = endTick() - startTick();
    auto targetStart = tick - tickRange / 2;
    // qDebug() << "tickRange" << tickRange << "tick" << tick << "targetStart" << targetStart;
    setViewportStartTick(targetStart);
}

void TimeGraphicsView::pageAdd() {
    // horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    auto start = horizontalScrollBar()->value();
    auto end = start + horizontalScrollBar()->pageStep();
    horizontalBarAnimateTo(end);
}

double TimeGraphicsView::sceneXToTick(double pos) const {
    return 480 * pos / scaleX() / m_pixelsPerQuarterNote;
}

double TimeGraphicsView::tickToSceneX(double tick) const {
    return tick * scaleX() * m_pixelsPerQuarterNote / 480;
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