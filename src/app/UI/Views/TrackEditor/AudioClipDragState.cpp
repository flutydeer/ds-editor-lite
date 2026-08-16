#include "AudioClipDragState.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>

#include <QtGlobal>

AudioClipDragState AudioClipDragState::begin(const double trimStartMs,
                                             const double playLengthMs,
                                             const double materialLengthMs,
                                             const int visibleStartTick, const double grabTick,
                                             const Timeline &timeline) {
    AudioClipDragState state;
    state.m_trimStartMs = trimStartMs;
    state.m_playLengthMs = playLengthMs;
    state.m_materialLengthMs = materialLengthMs;
    const double visibleStartMs = timeline.tickToMs(visibleStartTick);
    state.m_grabOffsetMs = timeline.tickToMs(grabTick) - visibleStartMs;
    state.m_materialStartMs = visibleStartMs - trimStartMs;
    state.m_visibleEndMs = visibleStartMs + playLengthMs;
    return state;
}

int AudioClipDragState::visibleStartForCursor(const double cursorTick,
                                              const Timeline &timeline) const {
    return qRound(timeline.msToTick(timeline.tickToMs(cursorTick) - m_grabOffsetMs));
}

void AudioClipDragState::moveTo(const int visibleStartTick,
                                Clip::ClipCommonProperties &properties,
                                const Timeline &timeline) const {
    project(visibleStartTick, properties, timeline);
}

bool AudioClipDragState::resizeLeftTo(const int leftTick, const int originalRightTick,
                                      const int minimumVisibleLength,
                                      Clip::ClipCommonProperties &properties,
                                      const Timeline &timeline) {
    int visibleStartTick =
        qMin(leftTick, originalRightTick - qMax(1, minimumVisibleLength));
    double trimStartMs = timeline.tickToMs(visibleStartTick) - m_materialStartMs;
    if (trimStartMs < 0.0) {
        trimStartMs = 0.0;
        visibleStartTick = qRound(timeline.msToTick(m_materialStartMs));
    }
    const double playLengthMs = m_visibleEndMs - timeline.tickToMs(visibleStartTick);
    if (playLengthMs <= 0.0)
        return false;

    m_trimStartMs = trimStartMs;
    m_playLengthMs = playLengthMs;
    project(visibleStartTick, properties, timeline);
    return true;
}

bool AudioClipDragState::resizeRightTo(const int rightTick, const int visibleStartTick,
                                       const int minimumVisibleLength,
                                       Clip::ClipCommonProperties &properties,
                                       const Timeline &timeline) {
    const auto visibleEndTick = qMax(rightTick, visibleStartTick + qMax(1, minimumVisibleLength));
    const double visibleStartMs = m_materialStartMs + m_trimStartMs;
    const double availableLengthMs = m_materialLengthMs - m_trimStartMs;
    const double playLengthMs =
        qMin(timeline.tickToMs(visibleEndTick) - visibleStartMs, availableLengthMs);
    if (playLengthMs <= 0.0)
        return false;

    m_playLengthMs = playLengthMs;
    project(visibleStartTick, properties, timeline);
    return true;
}

void AudioClipDragState::writeTruth(Clip::ClipCommonProperties &properties) const {
    properties.trimStartMs = m_trimStartMs;
    properties.playLengthMs = m_playLengthMs;
    properties.materialLengthMs = m_materialLengthMs;
}

void AudioClipDragState::project(const int visibleStartTick,
                                 Clip::ClipCommonProperties &properties,
                                 const Timeline &timeline) const {
    const auto caches = AudioClip::deriveTickCaches(m_trimStartMs, m_playLengthMs,
                                                   m_materialLengthMs, visibleStartTick, timeline);
    properties.start = caches.start;
    properties.clipStart = caches.clipStart;
    properties.clipLen = caches.clipLen;
    properties.length = caches.length;
}
