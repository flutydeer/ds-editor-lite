#include "WheelInputController.h"

#include <QInputDevice>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

namespace WheelInput {

    double axisValue(const QPoint &delta, const Qt::Orientation axis) {
        return axis == Qt::Horizontal ? delta.x() : delta.y();
    }

    bool usesPixelDelta(const QWheelEvent *event) {
        if (event->pixelDelta().isNull())
            return false;
        return event->angleDelta().isNull() ||
               event->deviceType() == QInputDevice::DeviceType::TouchPad;
    }

    double zoomDelta(const QWheelEvent *event, const Qt::Orientation axis) {
        if (usesPixelDelta(event))
            return axisValue(event->pixelDelta(), axis) * 4.0;
        return axisValue(event->angleDelta(), axis);
    }

    Qt::Orientation dominantAxis(const QWheelEvent *event) {
        const auto delta = usesPixelDelta(event) ? event->pixelDelta() : event->angleDelta();
        return qAbs(delta.x()) > qAbs(delta.y()) ? Qt::Horizontal : Qt::Vertical;
    }

    DeviceState::DeviceState() {
#if !defined(Q_OS_MAC) || QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        m_unlockTimer.setInterval(400);
        m_unlockTimer.setSingleShot(true);
        QObject::connect(&m_unlockTimer, &QTimer::timeout,
                         [this] { m_continuousInputLocked = false; });
#endif
    }

    bool DeviceState::isDiscrete(const QWheelEvent *event) {
#if defined(Q_OS_MAC) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (usesPixelDelta(event))
            return false;
        return event->deviceType() == QInputDevice::DeviceType::Mouse;
#else
        const auto deltaX = event->angleDelta().x();
        const auto deltaY = event->angleDelta().y();
        const auto absDx = qAbs(deltaX);
        const auto absDy = qAbs(deltaY);
        if (m_continuousInputLocked) {
            m_unlockTimer.start();
            return false;
        }
        if (usesPixelDelta(event)) {
            m_continuousInputLocked = true;
            m_unlockTimer.start();
            return false;
        }
        const auto isWheelStep = (absDx == 0 && absDy > 0 && absDy % 120 == 0) ||
                                 (absDy == 0 && absDx > 0 && absDx % 120 == 0);
        if (isWheelStep)
            return true;
        m_continuousInputLocked = true;
        m_unlockTimer.start();
        return false;
#endif
    }

} // namespace WheelInput

WheelInputController::WheelInputController(QObject *parent) : QObject(parent) {
    const auto initializeMotion = [this](Motion &motion, const auto &applyValue) {
        motion.animation.setEasingCurve(QEasingCurve::OutCubic);
        connect(&motion.animation, &QVariantAnimation::valueChanged, this, applyValue);
        auto *motionPtr = &motion;
        connect(&motion.animation, &QVariantAnimation::finished, this,
                [motionPtr] { motionPtr->logicalValue.reset(); });
    };

    initializeMotion(m_horizontalScroll, [this](const QVariant &value) {
        const auto &target = m_horizontalScrollTarget;
        if (target.setValue)
            target.setValue(value.toDouble());
    });
    initializeMotion(m_verticalScroll, [this](const QVariant &value) {
        const auto &target = m_verticalScrollTarget;
        if (target.setValue)
            target.setValue(value.toDouble());
    });
    initializeMotion(m_horizontalZoom, [this](const QVariant &value) {
        const auto &target = m_horizontalZoomTarget;
        if (target.setValueAt)
            target.setValueAt(value.toDouble(), m_horizontalZoomAnchor);
    });
    initializeMotion(m_verticalZoom, [this](const QVariant &value) {
        const auto &target = m_verticalZoomTarget;
        if (target.setValueAt)
            target.setValueAt(value.toDouble(), m_verticalZoomAnchor);
    });

    initializeAnimation();
}

void WheelInputController::setScrollTarget(const Qt::Orientation orientation, ScrollTarget target) {
    stopMotion(scrollMotion(orientation));
    scrollTarget(orientation) = std::move(target);
}

void WheelInputController::setZoomTarget(const Qt::Orientation orientation, ZoomTarget target) {
    stopMotion(zoomMotion(orientation));
    zoomTarget(orientation) = std::move(target);
}

void WheelInputController::setContinuousInputMode(const ContinuousInputMode mode) {
    m_continuousInputMode = mode;
}

void WheelInputController::setDiscreteAnimationEnabled(std::function<bool()> enabled) {
    m_discreteAnimationEnabled = enabled ? std::move(enabled) : [] { return true; };
}

bool WheelInputController::handleWheel(QWheelEvent *event, const Action requestedAction,
                                       const std::optional<Qt::Orientation> requestedSourceAxis) {
    if (!event)
        return false;

    const auto action = resolveAction(event, requestedAction);
    if (action == Action::Automatic)
        return false;

    const auto discrete = m_inputState.isDiscrete(event);
    if (!discrete && m_continuousInputMode == ContinuousInputMode::PassThrough) {
        switch (action) {
            case Action::HorizontalScroll:
                stopMotion(m_horizontalScroll);
                break;
            case Action::VerticalScroll:
                stopMotion(m_verticalScroll);
                break;
            case Action::HorizontalZoom:
                stopMotion(m_horizontalZoom);
                break;
            case Action::VerticalZoom:
                stopMotion(m_verticalZoom);
                break;
            case Action::Automatic:
                break;
        }
        return false;
    }

    const auto sourceAxis = resolveSourceAxis(event, action, requestedSourceAxis);
    bool handled = false;
    switch (action) {
        case Action::HorizontalScroll:
            handled = handleScroll(event, Qt::Horizontal, sourceAxis, discrete);
            break;
        case Action::VerticalScroll:
            handled = handleScroll(event, Qt::Vertical, sourceAxis, discrete);
            break;
        case Action::HorizontalZoom:
            handled = handleZoom(event, Qt::Horizontal, sourceAxis, discrete);
            break;
        case Action::VerticalZoom:
            handled = handleZoom(event, Qt::Vertical, sourceAxis, discrete);
            break;
        case Action::Automatic:
            break;
    }
    if (handled)
        event->accept();
    return handled;
}

bool WheelInputController::zoomByFactor(const Qt::Orientation orientation, const double factor,
                                        const double anchor) {
    auto &target = zoomTarget(orientation);
    if (!target.value || !target.setValueAt || !target.boundedValue || !std::isfinite(factor) ||
        factor <= 0.0) {
        return false;
    }

    stopMotion(scrollMotion(orientation));
    stopMotion(zoomMotion(orientation), false);
    const auto endValue = target.boundedValue(target.value() * factor);
    if (orientation == Qt::Horizontal)
        m_horizontalZoomAnchor = anchor;
    else
        m_verticalZoomAnchor = anchor;
    target.setValueAt(endValue, anchor);
    return true;
}

void WheelInputController::stop() {
    stopMotion(m_horizontalScroll);
    stopMotion(m_verticalScroll);
    stopMotion(m_horizontalZoom);
    stopMotion(m_verticalZoom);
}

std::optional<double>
    WheelInputController::logicalScrollValue(const Qt::Orientation orientation) const {
    return scrollMotion(orientation).logicalValue;
}

void WheelInputController::afterSetAnimationLevel(const AnimationGlobal::AnimationLevels level) {
    Q_UNUSED(level)
    updateAnimationDuration();
}

void WheelInputController::afterSetTimeScale(const double scale) {
    Q_UNUSED(scale)
    updateAnimationDuration();
}

WheelInputController::Action WheelInputController::resolveAction(const QWheelEvent *event,
                                                                 const Action action) const {
    if (action != Action::Automatic)
        return action;
    if (event->modifiers() == Qt::ControlModifier)
        return Action::HorizontalZoom;
    if (event->modifiers() == Qt::AltModifier)
        return Action::VerticalZoom;
    if (event->modifiers() == Qt::ShiftModifier)
        return Action::HorizontalScroll;
    if (event->modifiers() != Qt::NoModifier)
        return Action::Automatic;
    return WheelInput::dominantAxis(event) == Qt::Horizontal ? Action::HorizontalScroll
                                                             : Action::VerticalScroll;
}

Qt::Orientation
    WheelInputController::resolveSourceAxis(const QWheelEvent *event, const Action action,
                                            const std::optional<Qt::Orientation> sourceAxis) const {
    if (sourceAxis.has_value())
        return *sourceAxis;

    const auto delta =
        WheelInput::usesPixelDelta(event) ? event->pixelDelta() : event->angleDelta();
    const auto verticalSemanticAction =
        action == Action::HorizontalZoom || action == Action::VerticalZoom ||
        (action == Action::HorizontalScroll && event->modifiers() == Qt::ShiftModifier);
    if (verticalSemanticAction && delta.y() != 0)
        return Qt::Vertical;
    return WheelInput::dominantAxis(event);
}

bool WheelInputController::handleScroll(QWheelEvent *event, const Qt::Orientation targetAxis,
                                        const Qt::Orientation sourceAxis, const bool discrete) {
    auto &target = scrollTarget(targetAxis);
    if (!target.value || !target.setValue || !target.boundedValue || !target.step ||
        (target.canScroll && !target.canScroll())) {
        return false;
    }

    auto &motion = scrollMotion(targetAxis);
    stopMotion(zoomMotion(targetAxis));
    const auto currentValue = target.value();
    double offset = 0.0;
    if (WheelInput::usesPixelDelta(event)) {
        motion.remainder = 0.0;
        offset = -WheelInput::axisValue(event->pixelDelta(), sourceAxis);
    } else {
        const auto rawOffset =
            -target.step() * WheelInput::axisValue(event->angleDelta(), sourceAxis) / 120.0;
        if (discrete) {
            motion.remainder = 0.0;
            offset = rawOffset;
        } else {
            if (event->phase() == Qt::ScrollBegin)
                motion.remainder = 0.0;
            if (!qFuzzyIsNull(motion.remainder) && !qFuzzyIsNull(rawOffset) &&
                std::signbit(motion.remainder) != std::signbit(rawOffset)) {
                motion.remainder = 0.0;
            }
            motion.remainder += rawOffset;
            offset = std::trunc(motion.remainder);
            motion.remainder -= offset;
            if (event->phase() == Qt::ScrollEnd)
                motion.remainder = 0.0;
        }
    }

    const auto baseValue = target.boundedValue(
        discrete && motion.logicalValue.has_value() ? *motion.logicalValue : currentValue);
    const auto endValue = target.boundedValue(baseValue + offset);
    const auto animate = discrete && m_discreteAnimationEnabled() &&
                         getEffectiveAnimationTime(kAnimationDuration, AnimationGlobal::Full) > 0;
    if (!animate) {
        stopMotion(motion, false);
        target.setValue(endValue);
    } else if (!qFuzzyCompare(currentValue, endValue)) {
        startMotion(motion, currentValue, endValue);
    } else {
        motion.logicalValue.reset();
    }
    return true;
}

bool WheelInputController::handleZoom(QWheelEvent *event, const Qt::Orientation targetAxis,
                                      const Qt::Orientation sourceAxis, const bool discrete) {
    auto &target = zoomTarget(targetAxis);
    if (!target.value || !target.setValueAt || !target.boundedValue || target.step <= 0.0)
        return false;

    const auto delta = WheelInput::zoomDelta(event, sourceAxis);
    if (qFuzzyIsNull(delta))
        return true;

    auto &motion = zoomMotion(targetAxis);
    stopMotion(scrollMotion(targetAxis));
    const auto currentValue = target.value();
    const auto baseValue = target.boundedValue(
        discrete && motion.logicalValue.has_value() ? *motion.logicalValue : currentValue);
    const auto factor = delta > 0.0 ? 1.0 + target.step * delta / 120.0
                                    : 1.0 / (1.0 + target.step * -delta / 120.0);
    const auto endValue = target.boundedValue(baseValue * factor);
    if (targetAxis == Qt::Horizontal)
        m_horizontalZoomAnchor = event->position().x();
    else
        m_verticalZoomAnchor = event->position().y();

    const auto animate = discrete && m_discreteAnimationEnabled() &&
                         getEffectiveAnimationTime(kAnimationDuration, AnimationGlobal::Full) > 0;
    if (!animate) {
        stopMotion(motion, false);
        target.setValueAt(endValue, targetAxis == Qt::Horizontal ? m_horizontalZoomAnchor
                                                                 : m_verticalZoomAnchor);
    } else if (!qFuzzyCompare(currentValue, endValue)) {
        startMotion(motion, currentValue, endValue);
    } else {
        motion.logicalValue.reset();
    }
    return true;
}

void WheelInputController::startMotion(Motion &motion, const double currentValue,
                                       const double targetValue) {
    motion.animation.stop();
    motion.animation.setStartValue(currentValue);
    motion.animation.setEndValue(targetValue);
    motion.logicalValue = targetValue;
    motion.animation.start();
}

void WheelInputController::stopMotion(Motion &motion, const bool resetRemainder) {
    motion.animation.stop();
    motion.logicalValue.reset();
    if (resetRemainder)
        motion.remainder = 0.0;
}

void WheelInputController::updateAnimationDuration() {
    const auto duration = getEffectiveAnimationTime(kAnimationDuration, AnimationGlobal::Full);
    const auto update = [duration](Motion &motion, const auto &applyValue) {
        const auto running = motion.animation.state() == QAbstractAnimation::Running;
        const auto endValue = motion.animation.endValue();
        const auto currentValue = motion.animation.currentValue();
        motion.animation.stop();
        motion.animation.setDuration(duration);
        if (!running)
            return;
        if (duration == 0) {
            applyValue(endValue.toDouble());
            motion.logicalValue.reset();
            return;
        }
        motion.animation.setStartValue(currentValue);
        motion.animation.setEndValue(endValue);
        motion.animation.start();
    };

    update(m_horizontalScroll, [this](const double value) {
        if (m_horizontalScrollTarget.setValue)
            m_horizontalScrollTarget.setValue(value);
    });
    update(m_verticalScroll, [this](const double value) {
        if (m_verticalScrollTarget.setValue)
            m_verticalScrollTarget.setValue(value);
    });
    update(m_horizontalZoom, [this](const double value) {
        if (m_horizontalZoomTarget.setValueAt)
            m_horizontalZoomTarget.setValueAt(value, m_horizontalZoomAnchor);
    });
    update(m_verticalZoom, [this](const double value) {
        if (m_verticalZoomTarget.setValueAt)
            m_verticalZoomTarget.setValueAt(value, m_verticalZoomAnchor);
    });
}

WheelInputController::ScrollTarget &
    WheelInputController::scrollTarget(const Qt::Orientation orientation) {
    return orientation == Qt::Horizontal ? m_horizontalScrollTarget : m_verticalScrollTarget;
}

const WheelInputController::ScrollTarget &
    WheelInputController::scrollTarget(const Qt::Orientation orientation) const {
    return orientation == Qt::Horizontal ? m_horizontalScrollTarget : m_verticalScrollTarget;
}

WheelInputController::ZoomTarget &
    WheelInputController::zoomTarget(const Qt::Orientation orientation) {
    return orientation == Qt::Horizontal ? m_horizontalZoomTarget : m_verticalZoomTarget;
}

const WheelInputController::ZoomTarget &
    WheelInputController::zoomTarget(const Qt::Orientation orientation) const {
    return orientation == Qt::Horizontal ? m_horizontalZoomTarget : m_verticalZoomTarget;
}

WheelInputController::Motion &
    WheelInputController::scrollMotion(const Qt::Orientation orientation) {
    return orientation == Qt::Horizontal ? m_horizontalScroll : m_verticalScroll;
}

const WheelInputController::Motion &
    WheelInputController::scrollMotion(const Qt::Orientation orientation) const {
    return orientation == Qt::Horizontal ? m_horizontalScroll : m_verticalScroll;
}

WheelInputController::Motion &WheelInputController::zoomMotion(const Qt::Orientation orientation) {
    return orientation == Qt::Horizontal ? m_horizontalZoom : m_verticalZoom;
}
