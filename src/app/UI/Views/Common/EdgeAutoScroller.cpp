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
                                   const double lowHotZone, const double highHotZone,
                                   const double maxSpeed, const double baseSpeed) {
    if (end <= start || (lowHotZone <= 0 && highHotZone <= 0))
        return 0;

    // Strength ramps from 0 at the inner hot zone boundary to 1 at the
    // viewport edge and saturates beyond it (pointer outside the viewport).
    auto strengthAt = [](const double depth, const double zone) {
        return zone > 0 ? qBound(0.0, depth / zone, 1.0) : 0.0;
    };
    auto curve = [&](double strength) {
        return baseSpeed * strength + (maxSpeed - baseSpeed) * strength * strength;
    };

    double speed = 0;
    const double depthLow = start + lowHotZone - pos;    // > 0 inside the low-edge zone
    const double depthHigh = pos - (end - highHotZone);  // > 0 inside the high-edge zone
    if (depthLow > 0)
        speed -= curve(strengthAt(depthLow, lowHotZone));
    if (depthHigh > 0)
        speed += curve(strengthAt(depthHigh, highHotZone));
    return speed;
}

// Distance of the press position from one edge (0 = exactly on the edge,
// grows toward the viewport center). lowEdge is true for left/top edges.
static double pressDistance(const double pressPos, const double edgePos, const bool lowEdge) {
    return lowEdge ? pressPos - edgePos : edgePos - pressPos;
}

// Effective hot zone for one edge given where the drag started. When the
// pointer was already inside the configured zone at press time, the zone
// shrinks to the press distance so that scrolling only begins once the
// pointer moves closer to the edge than it was at press time.
static double effectiveHotZone(const double pressPos, const double edgePos, const double hotZone,
                               const bool lowEdge) {
    const double dist = pressDistance(pressPos, edgePos, lowEdge);
    if (dist >= hotZone)
        return hotZone; // pressed outside the zone: keep the configured zone
    return qMax(dist, 1.0); // pressed inside: narrow the zone to the press distance
}

QPointF EdgeAutoScroller::velocity(const QPointF &pointerPos, const QRectF &viewportRect,
                                   const Qt::Orientations axes,
                                   const EdgeAutoScrollConfig &config) {
    QPointF v;
    if (axes.testFlag(Qt::Horizontal))
        v.setX(axisSpeed(pointerPos.x(), viewportRect.left(), viewportRect.right(),
                         config.hotZoneH, config.hotZoneH, config.maxSpeedH, config.baseSpeed));
    if (axes.testFlag(Qt::Vertical))
        v.setY(axisSpeed(pointerPos.y(), viewportRect.top(), viewportRect.bottom(), config.hotZoneV,
                         config.hotZoneV, config.maxSpeedV, config.baseSpeed));
    return v;
}

QPointF EdgeAutoScroller::velocity(const QPointF &pointerPos, const QPointF &pressPos,
                                   const QRectF &viewportRect, const Qt::Orientations axes,
                                   const EdgeAutoScrollConfig &config) {
    QPointF v;
    if (axes.testFlag(Qt::Horizontal))
        v.setX(axisSpeed(pointerPos.x(), viewportRect.left(), viewportRect.right(),
                         effectiveHotZone(pressPos.x(), viewportRect.left(), config.hotZoneH, true),
                         effectiveHotZone(pressPos.x(), viewportRect.right(), config.hotZoneH,
                                          false),
                         config.maxSpeedH, config.baseSpeed));
    if (axes.testFlag(Qt::Vertical))
        v.setY(axisSpeed(pointerPos.y(), viewportRect.top(), viewportRect.bottom(),
                         effectiveHotZone(pressPos.y(), viewportRect.top(), config.hotZoneV, true),
                         effectiveHotZone(pressPos.y(), viewportRect.bottom(), config.hotZoneV,
                                          false),
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

QPoint EdgeAutoScroller::computeStep(const QPointF &pointerPos, const QPointF &pressPos,
                                     const QRectF &viewportRect, const Qt::Orientations axes,
                                     const double dtMs) {
    const auto v = velocity(pointerPos, pressPos, viewportRect, axes, m_config);
    m_accumulator += v * (dtMs / 1000.0);
    const int dx = static_cast<int>(std::trunc(m_accumulator.x()));
    const int dy = static_cast<int>(std::trunc(m_accumulator.y()));
    m_accumulator -= QPointF(dx, dy);
    return {dx, dy};
}

void EdgeAutoScroller::resetAccumulator() {
    m_accumulator = QPointF();
}

void EdgeAutoScroller::prepareDrag(const QPointF &pressPos, const Qt::Orientations axes) {
    stopDrag();
    m_dragPressPos = pressPos;
    setDragAxes(axes);
}

void EdgeAutoScroller::setDragAxes(const Qt::Orientations axes) {
    m_dragAxes = axes;
    m_dragArmed = !!axes;
    if (!m_dragArmed) {
        m_dragDistanceReached = false;
        stop();
    }
}

void EdgeAutoScroller::updateDragState(const QPointF &pointerPos, const QRectF &viewportRect,
                                       const int startDragDistance) {
    if (!m_dragArmed)
        return;
    if (!m_dragDistanceReached) {
        if ((pointerPos - m_dragPressPos).manhattanLength() < startDragDistance)
            return;
        m_dragDistanceReached = true;
    }

    const auto dragVelocity =
        velocity(pointerPos, m_dragPressPos, viewportRect, m_dragAxes, m_config);
    if (!dragVelocity.isNull())
        start();
    else
        stop();
}

QPoint EdgeAutoScroller::computeDragStep(const QPointF &pointerPos, const QRectF &viewportRect,
                                         const double dtMs) {
    if (!m_dragArmed)
        return {};
    return computeStep(pointerPos, m_dragPressPos, viewportRect, m_dragAxes, dtMs);
}

void EdgeAutoScroller::stopDrag() {
    m_dragArmed = false;
    m_dragDistanceReached = false;
    m_dragAxes = {};
    stop();
}

bool EdgeAutoScroller::isDragArmed() const {
    return m_dragArmed;
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
