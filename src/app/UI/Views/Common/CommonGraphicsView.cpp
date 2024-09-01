//
// Created by fluty on 2024/1/23.
//

#include "CommonGraphicsView.h"

#include "CommonGraphicsScene.h"
#include "ScrollBarGraphicsItem.h"
#include "Utils/MathUtils.h"

#include <QScrollBar>
#include <QWheelEvent>

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)) && defined(Q_OS_MAC)
#  define SUPPORTS_MOUSEWHEEL_DETECT_NATIVE
#endif

CommonGraphicsView::CommonGraphicsView(QWidget *parent) : QGraphicsView(parent) {
    setRenderHint(QPainter::Antialiasing);
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_Hover);
    // setCacheMode(QGraphicsView::CacheNone);
    // setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setMinimumHeight(150);
    setAcceptDrops(true);

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
            &CommonGraphicsView::notifyVisibleRectChanged);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
            &CommonGraphicsView::notifyVisibleRectChanged);

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
}

qreal CommonGraphicsView::scaleXMax() const {
    return m_scaleXMax;
}

void CommonGraphicsView::setScaleXMax(qreal max) {
    m_scaleXMax = max;
}

double CommonGraphicsView::scaleYMin() const {
    return m_scaleYMin;
}

void CommonGraphicsView::setScaleYMin(double min) {
    m_scaleYMin = min;
}

int CommonGraphicsView::hBarValue() const {
    return horizontalScrollBar()->value();
}

void CommonGraphicsView::setHBarValue(const int value) {
    horizontalScrollBar()->setValue(value);
}

int CommonGraphicsView::vBarValue() const {
    return verticalScrollBar()->value();
}

void CommonGraphicsView::setVBarValue(const int value) {
    verticalScrollBar()->setValue(value);
}

void CommonGraphicsView::hBarAnimateTo(int value) {
    m_hBarAnimation.stop();
    m_hBarAnimation.setStartValue(hBarValue());
    m_hBarAnimation.setEndValue(value);
    m_hBarAnimation.start();
}

void CommonGraphicsView::vBarAnimateTo(int value) {
    m_vBarAnimation.stop();
    m_vBarAnimation.setStartValue(vBarValue());
    m_vBarAnimation.setEndValue(value);
    m_vBarAnimation.start();
}

void CommonGraphicsView::hBarVBarAnimateTo(int hValue, int vValue) {
    m_hBarAnimation.stop();
    m_vBarAnimation.stop();

    m_hBarAnimation.setStartValue(hBarValue());
    m_hBarAnimation.setEndValue(hValue);
    m_vBarAnimation.setStartValue(vBarValue());
    m_vBarAnimation.setEndValue(vValue);

    m_hBarAnimation.start();
    m_vBarAnimation.start();
}

QRectF CommonGraphicsView::visibleRect() const {
    auto viewportRect = viewport()->rect();
    auto leftTop = mapToScene(viewportRect.left(), viewportRect.top());
    auto rightBottom = mapToScene(viewportRect.width(), viewportRect.height());
    auto rect = QRectF(leftTop, rightBottom);
    return rect;
}

void CommonGraphicsView::setEnsureSceneFillViewX(bool on) {
    m_ensureSceneFillViewX = on;
}

void CommonGraphicsView::setEnsureSceneFillViewY(bool on) {
    m_ensureSceneFillViewY = on;
}

CommonGraphicsView::DragBehavior CommonGraphicsView::dragBehavior() const {
    return m_dragBehavior;
}

void CommonGraphicsView::setDragBehavior(DragBehavior dragBehaviour) {
    m_dragBehavior = dragBehaviour;
    // TODO: 实现手型拖动视图
}

void CommonGraphicsView::notifyVisibleRectChanged() {
    emit visibleRectChanged(visibleRect());
}

void CommonGraphicsView::onWheelHorScale(QWheelEvent *event) {
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
        setHBarValue(targetValue);
    } else {
        m_scaleXAnimation.stop();
        m_scaleXAnimation.setStartValue(scaleX());
        m_scaleXAnimation.setEndValue(targetScaleX);
        m_scaleXAnimation.start();

        hBarAnimateTo(targetValue);
    }
}

void CommonGraphicsView::onWheelVerScale(QWheelEvent *event) {
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
        setVBarValue(targetValue);
    } else {
        m_scaleYAnimation.stop();
        m_scaleYAnimation.setStartValue(scaleY());
        m_scaleYAnimation.setEndValue(targetScaleY);
        m_scaleYAnimation.start();

        vBarAnimateTo(targetValue);
    }
}

void CommonGraphicsView::onWheelHorScroll(QWheelEvent *event) {
    auto deltaY = event->angleDelta().y();
    auto scrollLength = -1 * viewport()->width() * 0.2 * deltaY / 120;
    auto startValue = hBarValue();
    auto endValue = static_cast<int>(startValue + scrollLength);
    if (!isMouseEventFromWheel(event))
        setHBarValue(endValue);
    else {
        hBarAnimateTo(endValue);
    }
}

void CommonGraphicsView::onWheelVerScroll(QWheelEvent *event) {
    auto deltaY = event->angleDelta().y();
    if (!isMouseEventFromWheel(event)) {
        QGraphicsView::wheelEvent(event);
    } else {
        auto scrollLength = -1 * viewport()->height() * 0.15 * deltaY / 120;
        auto startValue = vBarValue();
        auto endValue = static_cast<int>(startValue + scrollLength);
        vBarAnimateTo(endValue);
    }
}

void CommonGraphicsView::adjustScaleXToFillView() {
    if (sceneRect().width() < viewport()->width()) {
        auto targetSceneWidth = viewport()->width();
        auto targetScaleX = targetSceneWidth / (sceneRect().width() / scaleX());
        setScaleX(targetScaleX);
        qDebug() << "Scene width < viewport width, adjust scaleX to" << targetScaleX;
    }
}

void CommonGraphicsView::adjustScaleYToFillView() {
    if (sceneRect().height() < viewport()->height()) {
        auto targetSceneHeight = viewport()->height();
        auto targetScaleY = targetSceneHeight / (sceneRect().height() / scaleY());
        setScaleY(targetScaleY);
        qDebug() << "Scene height < viewport height, adjust scaleY to" << targetScaleY;
    }
}

void CommonGraphicsView::dragEnterEvent(QDragEnterEvent *event) {
    QGraphicsView::dragEnterEvent(event);
    event->ignore();
}

void CommonGraphicsView::dragMoveEvent(QDragMoveEvent *event) {
    QGraphicsView::dragMoveEvent(event);
    event->ignore();
}

void CommonGraphicsView::dragLeaveEvent(QDragLeaveEvent *event) {
    QGraphicsView::dragLeaveEvent(event);
    event->ignore();
}

bool CommonGraphicsView::event(QEvent *event) {
#ifdef Q_OS_MAC
    // Mac Trackpad smooth zooming
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
            setHBarValue(targetValue);

            return true;
        }
    }
#endif
    if (event->type() == QEvent::HoverEnter)
        handleHoverEnterEvent(dynamic_cast<QHoverEvent *>(event)->position());
    else if (event->type() == QEvent::HoverLeave)
        handleHoverLeaveEvent(dynamic_cast<QHoverEvent *>(event));
    else if (event->type() == QEvent::HoverMove)
        handleHoverMoveEvent(dynamic_cast<QHoverEvent *>(event));
    return QGraphicsView::event(event);
}

void CommonGraphicsView::wheelEvent(QWheelEvent *event) {
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

void CommonGraphicsView::resizeEvent(QResizeEvent *event) {
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

void CommonGraphicsView::mousePressEvent(QMouseEvent *event) {
    if (scene()) {
        if (auto scrollBar =
                dynamic_cast<ScrollBarGraphicsItem *>(itemAt(event->position().toPoint()))) {
            // auto oriStr = scrollBar->orientation() == Qt::Horizontal ? "horizontal" : "vertical";
            // qDebug() << "mouse down on" << oriStr << "scrollbar";
            m_isDraggingScrollBar = true;
            m_draggingScrollbarType = scrollBar->orientation();
            m_mouseDownPos = event->position().toPoint();
            auto bar = scrollBar->orientation() == Qt::Horizontal ? horizontalScrollBar()
                                                                  : verticalScrollBar();
            scrollBar->moveToPressedState();
            m_scrollBarPressed = true;
            m_mouseDownBarValue = bar->value();
            m_mouseDownBarMax = bar->maximum();
            event->ignore();
            return;
        }
    }

    const auto isSelect = m_dragBehavior == DragBehavior::RectSelect || m_dragBehavior == DragBehavior::IntervalSelect;
    if (isSelect && event->button() == Qt::LeftButton) {
        if (scene()) {
            m_isDraggingContent = true;
            if (m_dragBehavior == DragBehavior::RectSelect)
                m_rubberBand.setSelectMode(RubberBandGraphicsItem::SelectMode::RectSelect);
            else
                m_rubberBand.setSelectMode(RubberBandGraphicsItem::SelectMode::BeamSelect);
            m_rubberBand.mouseDown(mapToScene(event->pos()));
            m_rubberBandAdded = false;
        }
    }
    QGraphicsView::mousePressEvent(event);
    event->ignore();
}

void CommonGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDraggingScrollBar) {
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
            horizontalScrollBar()->setValue(qRound(value));
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
            verticalScrollBar()->setValue(qRound(value));
        }
        QGraphicsView::mouseMoveEvent(event);
        return;
    }
    if (m_isDraggingContent) {
        if (!m_rubberBandAdded) {
            scene()->addItem(&m_rubberBand);
            m_rubberBandAdded = true;
        }
        m_rubberBand.mouseMove(mapToScene(event->pos()));
        QPainterPath path;
        path.addRect(QRectF(m_rubberBand.pos(), m_rubberBand.boundingRect().size()));
        scene()->setSelectionArea(path);
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CommonGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    // 模拟从按下切换到悬停状态
    m_scrollBarPressed = false;
    if (m_isDraggingScrollBar)
        handleHoverEnterEvent(event->position());
    m_isDraggingScrollBar = false;

    if (m_isDraggingContent) {
        if (m_rubberBandAdded)
            scene()->removeItem(&m_rubberBand);
        m_isDraggingContent = false;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

bool CommonGraphicsView::isMouseEventFromWheel(QWheelEvent *event) {
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

void CommonGraphicsView::updateAnimationDuration() {
    const int animationDurationBase = 250;
    auto duration = animationLevel() == AnimationGlobal::Full
                        ? getScaledAnimationTime(animationDurationBase)
                        : 0;
    m_scaleXAnimation.setDuration(duration);
    m_scaleYAnimation.setDuration(duration);
    m_hBarAnimation.setDuration(duration);
    m_vBarAnimation.setDuration(duration);
}

void CommonGraphicsView::handleHoverEnterEvent(const QPointF &pos) {
    if (!scene() || m_scrollBarPressed)
        return;

    if (auto scrollBar = scrollBarAt(pos.toPoint())) {
        m_prevHoveredItem = scrollBar->orientation() == Qt::Horizontal ? ItemType::HorizontalBar
                                                                       : ItemType::VerticalBar;
        if (scrollBar->mouseOnHandle(pos))
            scrollBar->moveToHoverState();
        else
            scrollBar->moveToNormalState();
    } else {
        if (auto commonScene = dynamic_cast<CommonGraphicsScene *>(scene())) {
            commonScene->hBar()->moveToNormalState();
            commonScene->vBar()->moveToNormalState();
        }
        m_prevHoveredItem = ItemType::Content;
    }
}

void CommonGraphicsView::handleHoverLeaveEvent(QHoverEvent *event) const {
    if (!scene() || m_scrollBarPressed)
        return;

    if (auto commonScene = dynamic_cast<CommonGraphicsScene *>(scene())) {
        commonScene->hBar()->moveToNormalState();
        commonScene->vBar()->moveToNormalState();
    }
}

void CommonGraphicsView::handleHoverMoveEvent(QHoverEvent *event) {
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
            if (auto commonScene = dynamic_cast<CommonGraphicsScene *>(scene())) {
                commonScene->hBar()->moveToNormalState();
                commonScene->vBar()->moveToNormalState();
            }
        }
        m_prevHoveredItem =
            orientation == Qt::Horizontal ? ItemType::HorizontalBar : ItemType::VerticalBar;
    } else {
        if (auto commonScene = dynamic_cast<CommonGraphicsScene *>(scene())) {
            commonScene->hBar()->moveToNormalState();
            commonScene->vBar()->moveToNormalState();
        }
        m_prevHoveredItem = ItemType::Content;
    }
}

ScrollBarGraphicsItem *CommonGraphicsView::scrollBarAt(const QPoint &pos) const {
    if (!scene() || !dynamic_cast<CommonGraphicsScene *>(scene()))
        return nullptr;

    for (const auto item : items(pos))
        if (auto bar = dynamic_cast<ScrollBarGraphicsItem *>(item))
            return bar;
    return nullptr;
}

void CommonGraphicsView::afterSetScale() {
    emit scaleChanged(scaleX(), scaleY());
}

void CommonGraphicsView::afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) {
    updateAnimationDuration();
}

void CommonGraphicsView::afterSetTimeScale(double scale) {
    updateAnimationDuration();
}