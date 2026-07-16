//
// Created by fluty on 24-9-18.
//

#include "AudioClip.h"

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
    emit propertyChanged();
}