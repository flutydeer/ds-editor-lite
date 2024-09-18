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
    m_path = path;
    emit propertyChanged();
}

const AudioInfoModel &AudioClip::audioInfo() const {
    return m_info;
}

void AudioClip::setAudioInfo(const AudioInfoModel &audioInfo) {
    m_info = audioInfo;
    emit propertyChanged();
}