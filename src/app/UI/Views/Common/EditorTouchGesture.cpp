#include "EditorTouchGesture.h"

#include <algorithm>
#include <cmath>

namespace {
    double distance(const QPointF &a, const QPointF &b) {
        const auto d = a - b;
        return std::hypot(d.x(), d.y());
    }
}

EditorTouchGesture::EditorTouchGesture() = default;

EditorTouchGesture::EditorTouchGesture(const Config &config) : m_config(config) {
}

void EditorTouchGesture::setConfig(const Config &config) {
    m_config = config;
}

const EditorTouchGesture::Config &EditorTouchGesture::config() const {
    return m_config;
}

EditorTouchGesture::Phase EditorTouchGesture::phase() const {
    return m_phase;
}

int EditorTouchGesture::activePointCount() const {
    return static_cast<int>(m_points.size());
}

bool EditorTouchGesture::hasSingleStream() const {
    return m_phase == Phase::Single;
}

QPointF EditorTouchGesture::lastPosition() const {
    return m_lastPosition;
}

qint64 EditorTouchGesture::longPressDeadline() const {
    if (m_phase != Phase::Pending || !m_longPressEligible)
        return 0;
    return m_pressTimestamp + m_config.longPressMs;
}

int EditorTouchGesture::indexOf(const int id) const {
    for (qsizetype i = 0; i < m_points.size(); ++i) {
        if (m_points.at(i).id == id)
            return static_cast<int>(i);
    }
    return -1;
}

void EditorTouchGesture::resetToIdle() {
    m_phase = m_points.isEmpty() ? Phase::Idle : Phase::Settling;
    m_primaryId = -1;
    m_fromLongPress = false;
    m_pendingDoubleTap = false;
    m_longPressEligible = false;
    m_navigationIds[0] = -1;
    m_navigationIds[1] = -1;
    m_axisLockDecided = false;
    m_horizontalZoomLocked = false;
    m_verticalZoomLocked = false;
    m_navigationVelocity = {};
    m_navigationDirty = false;
}

EditorTouchGesture::Events EditorTouchGesture::pressed(const int id, const QPointF &position,
                                                       const qint64 timestampMs) {
    if (indexOf(id) < 0)
        m_points.append({id, position});
    else
        m_points[indexOf(id)].position = position;

    Events events;
    if (m_phase == Phase::Settling || m_phase == Phase::Navigation)
        return events; // extra fingers never restart a gesture

    if (m_points.size() == 1) {
        m_phase = Phase::Pending;
        m_primaryId = id;
        m_pressPosition = position;
        m_lastPosition = position;
        m_pressTimestamp = timestampMs;
        m_longPressEligible = true;
        m_fromLongPress = false;
        m_pendingDoubleTap = m_lastTapTimestamp != 0 &&
                             timestampMs - m_lastTapTimestamp <= m_config.doubleTapMaxMs &&
                             distance(position, m_lastTapPosition) <= m_config.doubleTapSlopPx;
        return events;
    }

    // A second finger always wins over whatever the first one was doing.
    if (m_phase == Phase::Single) {
        Event cancel;
        cancel.type = Event::Type::SingleCancel;
        cancel.position = m_lastPosition;
        events.append(cancel);
    }
    beginNavigation(timestampMs);
    Event begin;
    begin.type = Event::Type::NavigationBegin;
    begin.anchor = m_navigationCentroid;
    events.append(begin);
    return events;
}

EditorTouchGesture::Events EditorTouchGesture::moved(const int id, const QPointF &position,
                                                     const qint64 timestampMs) {
    const auto index = indexOf(id);
    if (index < 0)
        return {};
    m_points[index].position = position;

    Events events;
    switch (m_phase) {
        case Phase::Pending: {
            if (id != m_primaryId)
                break;
            m_lastPosition = position;
            const auto travelled = distance(position, m_pressPosition);
            if (travelled > m_config.longPressSlopPx)
                m_longPressEligible = false;
            if (travelled <= m_config.tapSlopPx)
                break;
            m_phase = Phase::Single;
            Event begin;
            begin.type = Event::Type::SingleBegin;
            begin.position = m_pressPosition;
            begin.doubleTap = m_pendingDoubleTap;
            events.append(begin);
            Event move;
            move.type = Event::Type::SingleMove;
            move.position = position;
            events.append(move);
            break;
        }
        case Phase::LongPressPending:
            if (id == m_primaryId)
                m_lastPosition = position;
            break;
        case Phase::Single: {
            if (id != m_primaryId)
                break;
            m_lastPosition = position;
            Event move;
            move.type = Event::Type::SingleMove;
            move.position = position;
            move.fromLongPress = m_fromLongPress;
            events.append(move);
            break;
        }
        case Phase::Navigation:
            // Only the position is recorded here; the delta is emitted from
            // flushNavigation() once the whole touch event has been applied.
            m_navigationDirty = true;
            break;
        case Phase::Idle:
        case Phase::Settling:
            break;
    }
    return events;
}

EditorTouchGesture::Events EditorTouchGesture::flushNavigation(const qint64 timestampMs) {
    if (m_phase != Phase::Navigation || !m_navigationDirty)
        return {};
    m_navigationDirty = false;
    return updateNavigation(timestampMs);
}

EditorTouchGesture::Events EditorTouchGesture::released(const int id, const QPointF &position,
                                                        const qint64 timestampMs) {
    const auto index = indexOf(id);
    if (index >= 0)
        m_points.removeAt(index);

    Events events;
    switch (m_phase) {
        case Phase::Pending: {
            if (id != m_primaryId)
                break;
            const auto elapsed = timestampMs - m_pressTimestamp;
            const auto travelled = distance(position, m_pressPosition);
            if (travelled <= m_config.tapSlopPx && elapsed <= m_config.tapMaxMs) {
                Event begin;
                begin.type = Event::Type::SingleBegin;
                begin.position = m_pressPosition;
                begin.doubleTap = m_pendingDoubleTap;
                begin.tap = true;
                events.append(begin);
                Event end;
                end.type = Event::Type::SingleEnd;
                end.position = position;
                end.doubleTap = m_pendingDoubleTap;
                end.tap = true;
                events.append(end);
                // A double tap closes the chain so a third tap starts over.
                m_lastTapTimestamp = m_pendingDoubleTap ? 0 : timestampMs;
                m_lastTapPosition = position;
            }
            resetToIdle();
            break;
        }
        case Phase::LongPressPending:
            if (id == m_primaryId)
                resetToIdle();
            break;
        case Phase::Single: {
            if (id != m_primaryId)
                break;
            Event end;
            end.type = Event::Type::SingleEnd;
            end.position = position;
            end.fromLongPress = m_fromLongPress;
            events.append(end);
            resetToIdle();
            break;
        }
        case Phase::Navigation: {
            if (id != m_navigationIds[0] && id != m_navigationIds[1])
                break;
            Event end;
            end.type = Event::Type::NavigationEnd;
            end.anchor = m_navigationCentroid;
            const auto speed = std::hypot(m_navigationVelocity.x(), m_navigationVelocity.y());
            if (speed >= m_config.inertiaMinVelocityPxPerSec)
                end.velocity = m_navigationVelocity;
            events.append(end);
            resetToIdle();
            break;
        }
        case Phase::Idle:
        case Phase::Settling:
            if (m_points.isEmpty())
                m_phase = Phase::Idle;
            break;
    }
    if (m_points.isEmpty() && m_phase == Phase::Settling)
        m_phase = Phase::Idle;
    return events;
}

EditorTouchGesture::Events EditorTouchGesture::cancelled() {
    Events events;
    if (m_phase == Phase::Single) {
        Event cancel;
        cancel.type = Event::Type::SingleCancel;
        cancel.position = m_lastPosition;
        events.append(cancel);
    } else if (m_phase == Phase::Navigation) {
        Event end;
        end.type = Event::Type::NavigationEnd;
        end.anchor = m_navigationCentroid;
        events.append(end);
    }
    m_points.clear();
    resetToIdle();
    m_phase = Phase::Idle;
    return events;
}

EditorTouchGesture::Events EditorTouchGesture::longPressTimeout(const qint64 timestampMs) {
    Q_UNUSED(timestampMs)
    Events events;
    if (m_phase != Phase::Pending || !m_longPressEligible)
        return events;
    m_phase = Phase::LongPressPending;
    Event event;
    event.type = Event::Type::LongPress;
    event.position = m_pressPosition;
    events.append(event);
    return events;
}

EditorTouchGesture::Events EditorTouchGesture::confirmLongPress(const bool asDrag,
                                                                const qint64 timestampMs) {
    Q_UNUSED(timestampMs)
    Events events;
    if (m_phase != Phase::LongPressPending)
        return events;
    if (!asDrag) {
        // The caller opened a context menu: the stream is spent, and the finger
        // still on the glass must not fall through to an edit.
        resetToIdle();
        m_phase = m_points.isEmpty() ? Phase::Idle : Phase::Settling;
        return events;
    }
    m_phase = Phase::Single;
    m_fromLongPress = true;
    Event begin;
    begin.type = Event::Type::SingleBegin;
    begin.position = m_pressPosition;
    begin.fromLongPress = true;
    events.append(begin);
    if (distance(m_lastPosition, m_pressPosition) > 0.5) {
        Event move;
        move.type = Event::Type::SingleMove;
        move.position = m_lastPosition;
        move.fromLongPress = true;
        events.append(move);
    }
    return events;
}

void EditorTouchGesture::beginNavigation(const qint64 timestampMs) {
    m_phase = Phase::Navigation;
    m_navigationIds[0] = m_points.at(0).id;
    m_navigationIds[1] = m_points.at(1).id;
    const auto a = m_points.at(0).position;
    const auto b = m_points.at(1).position;
    m_navigationCentroid = (a + b) / 2.0;
    m_navigationSpanX = std::abs(a.x() - b.x());
    m_navigationSpanY = std::abs(a.y() - b.y());
    m_navigationLogX = 0.0;
    m_navigationLogY = 0.0;
    m_axisLockDecided = false;
    m_horizontalZoomLocked = false;
    m_verticalZoomLocked = false;
    m_navigationVelocity = {};
    m_navigationTimestamp = timestampMs;
    m_navigationDirty = false;
}

void EditorTouchGesture::updateAxisLock(const double logX, const double logY) {
    if (m_axisLockDecided)
        return;
    const auto ax = std::abs(logX);
    const auto ay = std::abs(logY);
    if (std::max(ax, ay) < m_config.axisLockThreshold)
        return;
    if (ax > ay * m_config.axisLockRatio) {
        m_horizontalZoomLocked = true;
        m_verticalZoomLocked = false;
    } else if (ay > ax * m_config.axisLockRatio) {
        m_horizontalZoomLocked = false;
        m_verticalZoomLocked = true;
    } else {
        m_horizontalZoomLocked = true;
        m_verticalZoomLocked = true;
    }
    m_axisLockDecided = true;
}

EditorTouchGesture::Events EditorTouchGesture::updateNavigation(const qint64 timestampMs) {
    const auto first = indexOf(m_navigationIds[0]);
    const auto second = indexOf(m_navigationIds[1]);
    if (first < 0 || second < 0)
        return {};

    const auto a = m_points.at(first).position;
    const auto b = m_points.at(second).position;
    const auto centroid = (a + b) / 2.0;
    const auto spanX = std::abs(a.x() - b.x());
    const auto spanY = std::abs(a.y() - b.y());

    const auto panDelta = centroid - m_navigationCentroid;
    const auto dt = static_cast<double>(timestampMs - m_navigationTimestamp) / 1000.0;
    if (dt > 0.0) {
        const QPointF instant(panDelta.x() / dt, panDelta.y() / dt);
        const auto keep = m_config.velocitySmoothing;
        m_navigationVelocity = m_navigationVelocity * keep + instant * (1.0 - keep);
    }

    const auto axisFactor = [this](const double previousSpan, const double span) {
        if (previousSpan < m_config.minPinchSpanPx || span < m_config.minPinchSpanPx)
            return 1.0;
        return span / previousSpan;
    };
    auto factorX = axisFactor(m_navigationSpanX, spanX);
    auto factorY = axisFactor(m_navigationSpanY, spanY);
    m_navigationLogX += std::log(factorX);
    m_navigationLogY += std::log(factorY);

    const auto wasDecided = m_axisLockDecided;
    updateAxisLock(m_navigationLogX, m_navigationLogY);
    if (!wasDecided && m_axisLockDecided) {
        // Replay everything accumulated before the lock so the pinch does not
        // silently lose the motion that decided the dominant axis.
        factorX = std::exp(m_navigationLogX);
        factorY = std::exp(m_navigationLogY);
    }
    if (!m_horizontalZoomLocked)
        factorX = 1.0;
    if (!m_verticalZoomLocked)
        factorY = 1.0;

    m_navigationCentroid = centroid;
    m_navigationSpanX = spanX;
    m_navigationSpanY = spanY;
    m_navigationTimestamp = timestampMs;

    Events events;
    Event event;
    event.type = Event::Type::NavigationUpdate;
    event.panDelta = panDelta;
    event.anchor = centroid;
    event.horizontalFactor = factorX;
    event.verticalFactor = factorY;
    events.append(event);
    return events;
}
