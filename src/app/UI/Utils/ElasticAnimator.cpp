//
// Created by FlutyDeer on 2025/4/1.
//

#include "ElasticAnimator.h"

#include <QDebug>

ElasticAnimator::ElasticAnimator(QObject *parent)
    : QObject(parent), m_smoothness(0.9), m_responsiveness(0.08), m_position(0, 0),
      m_velocity(0, 0), m_target(0, 0) {
    m_timer = new QTimer(this);
    m_timer->setInterval(8);
    connect(m_timer, &QTimer::timeout, this, &ElasticAnimator::updatePosition);
    // m_timer->start(8);
}

void ElasticAnimator::setSmoothness(const qreal value) {
    m_smoothness = qBound(0.1, value, 0.9);
}

void ElasticAnimator::setResponsiveness(const qreal value) {
    m_responsiveness = qBound(0.01, value, 0.2);
}

QPointF ElasticAnimator::target() const {
    return m_target;
}

void ElasticAnimator::setTarget(const QPointF &target) {
    m_target = target;
    if (!m_timer->isActive())
        m_timer->start();
}

QPointF ElasticAnimator::position() const {
    return m_position;
}

QPointF ElasticAnimator::velocity() const {
    return m_velocity;
}

void ElasticAnimator::updatePosition() {
    auto distanceBetweenPoints = [](const QPointF &p1, const QPointF &p2) {
        const double dx = p2.x() - p1.x();
        const double dy = p2.y() - p1.y();
        return std::sqrt(dx * dx + dy * dy);
    };
    auto velocityNorm = [](const QPointF &v) { return std::sqrt(v.x() * v.x() + v.y() * v.y()); };

    const qreal dx = m_target.x() - m_position.x();
    const qreal dy = m_target.y() - m_position.y();

    const qreal idealVx = dx * m_responsiveness;
    const qreal idealVy = dy * m_responsiveness;

    m_velocity.setX(m_velocity.x() + (idealVx - m_velocity.x()) * (1 - m_smoothness));
    m_velocity.setY(m_velocity.y() + (idealVy - m_velocity.y()) * (1 - m_smoothness));

    m_position += m_velocity;

    const qreal currentVelocity = velocityNorm(m_velocity);
    const qreal currentDistance = distanceBetweenPoints(m_target, m_position);

    // 定义停止阈值
    constexpr qreal minSpeed = 0.1;
    constexpr qreal minDistance = 0.1;

    if (currentVelocity < minSpeed && currentDistance < minDistance) {
        // 到达目标，停止动画
        m_position = m_target;
        m_velocity = QPointF(0, 0);
        emit positionUpdated(m_position, m_velocity);
        m_timer->stop();
    } else {
        emit positionUpdated(m_position, m_velocity);
    }
}