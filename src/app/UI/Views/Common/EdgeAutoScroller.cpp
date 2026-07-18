#include "EdgeAutoScroller.h"

#include <QtMath>

EdgeAutoScroller::EdgeAutoScroller(QObject *parent) : QObject(parent) {
    m_timer.setInterval(m_config.intervalMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        const double dtMs = m_elapsed.restart();
        emit frame(dtMs);
    });
}

const EdgeAutoScrollConfig &EdgeAutoScroller::config() const {
    return m_config;
}

void EdgeAutoScroller::setConfig(const EdgeAutoScrollConfig &config) {
    m_config = config;
    m_timer.setInterval(m_config.intervalMs);
}

double EdgeAutoScroller::axisSpeed(const double pos, const double start, const double end,
                                   const int hotZone, const double maxSpeed,
                                   const double baseSpeed) {
    if (hotZone <= 0 || end <= start)
        return 0;

    // Strength ramps from 0 at the inner hot zone boundary to 1 at the
    // viewport edge and saturates beyond it (pointer outside the viewport).
    auto strengthAt = [&](double depth) { return qBound(0.0, depth / hotZone, 1.0); };
    auto curve = [&](double strength) {
        return baseSpeed * strength + (maxSpeed - baseSpeed) * strength * strength;
    };

    double speed = 0;
    const double depthLow = start + hotZone - pos;  // > 0 inside the low-edge zone
    const double depthHigh = pos - (end - hotZone); // > 0 inside the high-edge zone
    if (depthLow > 0)
        speed -= curve(strengthAt(depthLow));
    if (depthHigh > 0)
        speed += curve(strengthAt(depthHigh));
    return speed;
}

QPointF EdgeAutoScroller::velocity(const QPointF &pointerPos, const QRectF &viewportRect,
                                   const Qt::Orientations axes,
                                   const EdgeAutoScrollConfig &config) {
    QPointF v;
    if (axes.testFlag(Qt::Horizontal))
        v.setX(axisSpeed(pointerPos.x(), viewportRect.left(), viewportRect.right(), config.hotZoneH,
                         config.maxSpeedH, config.baseSpeed));
    if (axes.testFlag(Qt::Vertical))
        v.setY(axisSpeed(pointerPos.y(), viewportRect.top(), viewportRect.bottom(), config.hotZoneV,
                         config.maxSpeedV, config.baseSpeed));
    return v;
}

QPointF EdgeAutoScroller::clampToRect(const QPointF &pos, const QRectF &rect) {
    return {qBound(rect.left(), pos.x(), rect.right()), qBound(rect.top(), pos.y(), rect.bottom())};
}

QPoint EdgeAutoScroller::computeStep(const QPointF &pointerPos, const QRectF &viewportRect,
                                     const Qt::Orientations axes, const double dtMs) {
    const auto v = velocity(pointerPos, viewportRect, axes, m_config);
    m_accumulator += v * (dtMs / 1000.0);
    const int dx = static_cast<int>(std::trunc(m_accumulator.x()));
    const int dy = static_cast<int>(std::trunc(m_accumulator.y()));
    m_accumulator -= QPointF(dx, dy);
    return {dx, dy};
}

void EdgeAutoScroller::resetAccumulator() {
    m_accumulator = QPointF();
}

void EdgeAutoScroller::start() {
    if (m_timer.isActive())
        return;
    resetAccumulator();
    m_elapsed.start();
    m_timer.start();
}

void EdgeAutoScroller::stop() {
    m_timer.stop();
}

bool EdgeAutoScroller::isRunning() const {
    return m_timer.isActive();
}
