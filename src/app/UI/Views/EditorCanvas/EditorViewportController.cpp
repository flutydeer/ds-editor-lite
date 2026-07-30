#include "EditorViewportController.h"

#include <QtMath>

EditorViewportController::EditorViewportController(QObject *parent) : QObject(parent) {
}

const EditorViewportState &EditorViewportController::state() const {
    return m_state;
}

void EditorViewportController::restore(const EditorViewportState &state) {
    if (!validScale(state.horizontalScale) || !validScale(state.verticalScale))
        return;
    m_state = state;
    notify(EditorDirtyDomain::All);
}

void EditorViewportController::setCenter(const double tick, const double secondaryValue) {
    if (!qIsFinite(tick) || !qIsFinite(secondaryValue))
        return;
    if (qFuzzyCompare(m_state.centerTick, tick) &&
        qFuzzyCompare(m_state.centerTrack, secondaryValue) &&
        qFuzzyCompare(m_state.centerKey, secondaryValue))
        return;
    m_state.centerTick = tick;
    m_state.centerTrack = secondaryValue;
    m_state.centerKey = secondaryValue;
    notify(EditorDirtyDomain::Camera);
}

bool EditorViewportController::setScale(const double horizontalScale, const double verticalScale) {
    if (!validScale(horizontalScale) || !validScale(verticalScale))
        return false;
    if (qFuzzyCompare(m_state.horizontalScale, horizontalScale) &&
        qFuzzyCompare(m_state.verticalScale, verticalScale))
        return true;
    m_state.horizontalScale = horizontalScale;
    m_state.verticalScale = verticalScale;
    notify(EditorDirtyDomain::Camera | EditorDirtyDomain::Geometry | EditorDirtyDomain::Text);
    return true;
}

void EditorViewportController::setVisibleRange(const double startTick, const double endTick,
                                               const double topValue, const double bottomValue) {
    if (!qIsFinite(startTick) || !qIsFinite(endTick) || !qIsFinite(topValue) ||
        !qIsFinite(bottomValue))
        return;
    m_state.startTick = startTick;
    m_state.endTick = endTick;
    m_state.topValue = topValue;
    m_state.bottomValue = bottomValue;
    notify(EditorDirtyDomain::Camera);
}

void EditorViewportController::setViewportMetrics(const QSize &size,
                                                  const double devicePixelRatio) {
    if (size == m_state.viewportSize && qFuzzyCompare(devicePixelRatio, m_state.devicePixelRatio))
        return;
    m_state.viewportSize = size;
    m_state.devicePixelRatio = qMax(1.0, devicePixelRatio);
    notify(EditorDirtyDomain::Camera | EditorDirtyDomain::Geometry | EditorDirtyDomain::Text);
}

void EditorViewportController::setPlaybackPositions(const double playbackPosition,
                                                    const double lastPlaybackPosition) {
    if (!qIsFinite(playbackPosition) || !qIsFinite(lastPlaybackPosition))
        return;
    m_state.playbackPosition = playbackPosition;
    m_state.lastPlaybackPosition = lastPlaybackPosition;
    notify(EditorDirtyDomain::Overlay);
}

bool EditorViewportController::validScale(const double value) {
    return qIsFinite(value) && value > 0.0;
}

void EditorViewportController::notify(const EditorDirtyDomains domains) {
    emit stateChanged(m_state, domains);
}
