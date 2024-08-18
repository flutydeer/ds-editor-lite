//
// Created by fluty on 2024/1/23.
//

#include "CommonGraphicsView.h"

#include <QScrollBar>
#include <QWheelEvent>

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)) && defined(Q_OS_MAC)
#  define SUPPORTS_MOUSEWHEEL_DETECT_NATIVE
#endif

CommonGraphicsView::CommonGraphicsView(QWidget *parent) : QGraphicsView(parent) {
    setRenderHint(QPainter::Antialiasing);
    setAttribute(Qt::WA_AcceptTouchEvents);
    // setCacheMode(QGraphicsView::CacheNone);
    // setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setMinimumHeight(150);

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

void CommonGraphicsView::setEnsureSceneFillView(bool on) {
    m_ensureSceneFillView = on;
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
    return QGraphicsView::event(event);
}

void CommonGraphicsView::wheelEvent(QWheelEvent *event) {
    auto cursorPos = event->position().toPoint();
    auto scenePos = mapToScene(cursorPos);

    auto deltaX = event->angleDelta().x();
    auto deltaY = event->angleDelta().y();

    if (event->modifiers() == Qt::ControlModifier) {
        onWheelHorScale(event);
    } else if (event->modifiers() == Qt::AltModifier) {
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
        if (m_ensureSceneFillView && scaledSceneHeight < viewport()->height()) {
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

    } else if (event->modifiers() == Qt::ShiftModifier) {
        auto scrollLength = -1 * viewport()->width() * 0.2 * deltaY / 120;
        auto startValue = hBarValue();
        auto endValue = static_cast<int>(startValue + scrollLength);
        if (!isMouseEventFromWheel(event))
            setHBarValue(endValue);
        else {
            hBarAnimateTo(endValue);
        }
    } else { // No modifier
        if (!isMouseEventFromWheel(event)) {
            QGraphicsView::wheelEvent(event);
        } else {
            auto scrollLength = -1 * viewport()->height() * 0.15 * deltaY / 120;
            auto startValue = vBarValue();
            auto endValue = static_cast<int>(startValue + scrollLength);
            vBarAnimateTo(endValue);
        }
    }

    notifyVisibleRectChanged();
}

void CommonGraphicsView::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    emit sizeChanged(viewport()->size());
    notifyVisibleRectChanged();
}

void CommonGraphicsView::mousePressEvent(QMouseEvent *event) {
    QGraphicsView::mousePressEvent(event);
    event->ignore();
}

void CommonGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    // if (event->pos().x() > rect().width() * 0.8)
    //     horizontalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
    QGraphicsView::mouseMoveEvent(event);
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
    const int animationDurationBase = 150;
    auto duration = animationLevel() == AnimationGlobal::Full
                        ? getScaledAnimationTime(animationDurationBase)
                        : 0;
    m_scaleXAnimation.setDuration(duration);
    m_scaleYAnimation.setDuration(duration);
    m_hBarAnimation.setDuration(duration);
    m_vBarAnimation.setDuration(duration);
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