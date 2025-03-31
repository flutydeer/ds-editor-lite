//
// Created by FlutyDeer on 2025/4/1.
//

#include "ElasticAnimator.h"

ElasticAnimator::ElasticAnimator(QObject *parent)
    : QObject(parent), m_smoothness(0.5), m_responsiveness(0.08), m_position(0, 0),
      m_velocity(0, 0), m_target(0, 0) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ElasticAnimator::updatePosition);
    m_timer->start(8); // ~60 FPS
}

void ElasticAnimator::setSmoothness(qreal value) {
    m_smoothness = qBound(0.1, value, 0.9);
}

void ElasticAnimator::setResponsiveness(qreal value) {
    m_responsiveness = qBound(0.01, value, 0.2);
}

void ElasticAnimator::setTarget(const QPointF &target) {
    m_target = target;
}

QPointF ElasticAnimator::position() const {
    return m_position;
}

QPointF ElasticAnimator::velocity() const {
    return m_velocity;
}

void ElasticAnimator::updatePosition() {
    // 弹性跟随算法
    const qreal dx = m_target.x() - m_position.x();
    const qreal dy = m_target.y() - m_position.y();

    const qreal idealVx = dx * m_responsiveness;
    const qreal idealVy = dy * m_responsiveness;

    m_velocity.setX(m_velocity.x() + (idealVx - m_velocity.x()) * (1 - m_smoothness));
    m_velocity.setY(m_velocity.y() + (idealVy - m_velocity.y()) * (1 - m_smoothness));

    m_position += m_velocity;

    emit positionUpdated(m_position, m_velocity);
}