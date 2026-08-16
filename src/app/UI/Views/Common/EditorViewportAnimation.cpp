#include "EditorViewportAnimation.h"

#include <QEasingCurve>

#include <utility>

EditorViewportAnimation::EditorViewportAnimation(ApplyCallback apply, QObject *parent)
    : QObject(parent), m_apply(std::move(apply)) {
    m_animation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        if (m_apply)
            m_apply(value.toPointF());
    });
    initializeAnimation();
}

void EditorViewportAnimation::moveTo(const QPointF &current, const QPointF &target,
                                     const bool animated) {
    if (animated && isRunning() && target == m_target)
        return;

    m_animation.stop();
    m_target = target;
    if (!animated || m_animation.duration() <= 0 || current == target) {
        if (m_apply && current != target)
            m_apply(target);
        return;
    }

    m_animation.setStartValue(current);
    m_animation.setEndValue(target);
    m_animation.start();
}

void EditorViewportAnimation::stop() {
    m_animation.stop();
}

QPointF EditorViewportAnimation::logicalOffset(const QPointF &current) const {
    return isRunning() ? m_target : current;
}

bool EditorViewportAnimation::isRunning() const {
    return m_animation.state() == QAbstractAnimation::Running;
}

void EditorViewportAnimation::afterSetAnimationLevel(AnimationGlobal::AnimationLevels) {
    updateDuration();
}

void EditorViewportAnimation::afterSetTimeScale(double) {
    updateDuration();
}

void EditorViewportAnimation::updateDuration() {
    constexpr int durationBase = 250;
    const auto target = m_target;
    const auto running = isRunning();
    const auto current = running ? m_animation.currentValue().toPointF() : QPointF();
    m_animation.stop();
    m_animation.setDuration(getEffectiveAnimationTime(durationBase, AnimationGlobal::Full));
    if (!running)
        return;
    if (m_animation.duration() <= 0) {
        if (m_apply)
            m_apply(target);
        return;
    }
    m_animation.setStartValue(current);
    m_animation.setEndValue(target);
    m_animation.start();
}
