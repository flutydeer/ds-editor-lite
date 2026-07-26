//
// Created by fluty on 24-9-18.
//

#include <lite/ProjectModel/AppModel/AudioClip.h>

#include <lite/MusicBase/Timeline.h>

#include <QtGlobal>

AudioClip::AudioClipProperties::AudioClipProperties(const AudioClip &clip) {
    applyPropertiesFromClip(*this, clip);
    path = clip.path();
}

AudioClip::AudioClipProperties::AudioClipProperties(const IClip &clip) {
    applyPropertiesFromClip(*this, clip);
}

IClip::ClipType AudioClip::clipType() const {
    return Audio;
}

QString AudioClip::path() const {
    return m_path;
}

void AudioClip::setPath(const QString &path) {
    const bool changed = m_path != path;
    m_path = path;
    if (changed)
        emit pathChanged();
    emit propertyChanged();
}

AudioPathInfo AudioClip::pathInfo() const {
    return m_pathInfo;
}

void AudioClip::setPathInfo(const AudioPathInfo &pathInfo) {
    m_pathInfo = pathInfo;
}

AudioClip::PathStatus AudioClip::pathStatus() const {
    return m_pathStatus;
}

void AudioClip::setPathStatus(const PathStatus status) {
    if (m_pathStatus == status)
        return;
    m_pathStatus = status;
    emit pathStatusChanged(status);
}

const AudioInfoModel &AudioClip::audioInfo() const {
    return m_info;
}

void AudioClip::setAudioInfo(const AudioInfoModel &audioInfo) {
    m_info = audioInfo;
    if (m_info.sampleRate > 0 && m_info.frames > 0)
        m_materialLengthMs = static_cast<double>(m_info.frames) * 1000.0 / m_info.sampleRate;
    emit propertyChanged();
}

double AudioClip::trimStartMs() const {
    return m_trimStartMs;
}

double AudioClip::playLengthMs() const {
    return m_playLengthMs;
}

double AudioClip::materialLengthMs() const {
    return m_materialLengthMs;
}

bool AudioClip::hasRealTimeAnchor() const {
    return m_playLengthMs >= 0;
}

void AudioClip::setRealTimeAnchor(const double trimStartMs, const double playLengthMs,
                                  const double materialLengthMs) {
    m_trimStartMs = qMax(0.0, trimStartMs);
    m_playLengthMs = qMax(0.0, playLengthMs);
    m_materialLengthMs = qMax(m_playLengthMs, materialLengthMs);
}

void AudioClip::syncTruthFromTicks(const Timeline &timeline) {
    const int visibleStart = m_start + m_clipStart;
    const double originMs = timeline.tickToMs(m_start);
    const double visibleMs = timeline.tickToMs(visibleStart);
    m_trimStartMs = qMax(0.0, visibleMs - originMs);
    m_playLengthMs = qMax(0.0, timeline.tickToMs(visibleStart + m_clipLen) - visibleMs);
    const double materialMs = timeline.tickToMs(m_start + m_length) - originMs;
    // Prefer the decoded file duration when available; ticks are a fallback
    if (m_info.sampleRate > 0 && m_info.frames > 0)
        m_materialLengthMs = static_cast<double>(m_info.frames) * 1000.0 / m_info.sampleRate;
    else
        m_materialLengthMs = qMax(m_playLengthMs, materialMs);
}

bool AudioClip::updateTicksFromTruth(const Timeline &timeline) {
    if (!hasRealTimeAnchor())
        return false;
    const int visibleStart = m_start + m_clipStart;
    const double visibleMs = timeline.tickToMs(visibleStart);
    const int newStart = qRound(timeline.msToTick(visibleMs - m_trimStartMs));
    const int newClipStart = visibleStart - newStart;
    const int newClipLen =
        qMax(1, qRound(timeline.msToTick(visibleMs + m_playLengthMs)) - visibleStart);
    const int newLength =
        qMax(newClipStart + newClipLen,
             qRound(timeline.msToTick(visibleMs - m_trimStartMs + m_materialLengthMs)) - newStart);
    if (newStart == m_start && newClipStart == m_clipStart && newClipLen == m_clipLen &&
        newLength == m_length)
        return false;
    m_start = newStart;
    m_clipStart = newClipStart;
    m_clipLen = newClipLen;
    m_length = newLength;
    return true;
}

void AudioClip::deriveTruthForProperties(ClipCommonProperties &args, const Timeline &timeline) {
    const int visibleStart = args.start + args.clipStart;
    const double originMs = timeline.tickToMs(args.start);
    const double visibleMs = timeline.tickToMs(visibleStart);
    args.trimStartMs = qMax(0.0, visibleMs - originMs);
    args.playLengthMs = qMax(0.0, timeline.tickToMs(visibleStart + args.clipLen) - visibleMs);
    args.materialLengthMs =
        qMax(args.playLengthMs, timeline.tickToMs(args.start + args.length) - originMs);
}

void AudioClip::preserveUnchangedTruth(ClipCommonProperties &newArgs,
                                       const ClipCommonProperties &oldArgs) {
    if (oldArgs.playLengthMs < 0)
        return;
    if (newArgs.clipStart == oldArgs.clipStart)
        newArgs.trimStartMs = oldArgs.trimStartMs;
    if (newArgs.clipLen == oldArgs.clipLen)
        newArgs.playLengthMs = oldArgs.playLengthMs;
    newArgs.materialLengthMs = qMax(newArgs.playLengthMs, oldArgs.materialLengthMs);
}

void AudioClip::applyRealTimeAnchorFromProperties(const ClipCommonProperties &args,
                                                  const Timeline &timeline) {
    if (args.playLengthMs >= 0)
        setRealTimeAnchor(args.trimStartMs, args.playLengthMs, args.materialLengthMs);
    else
        syncTruthFromTicks(timeline);
    updateTicksFromTruth(timeline);
}