//
// Created by Crs_1 on 2024/2/4.
//

#include "AudioContext.h"

#include "AudioSystem.h"

#include <TalcsCore/TransportAudioSource.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsDevice/AudioSourcePlayback.h>

static qint64 tickToSample(double tick) {
    return qint64(tick * PlaybackController::instance()->tempo() * AudioSystem::instance()->adoptedSampleRate() / 60.0 / 480.0);
}

static double sampleToTick(qint64 sample) {
    return double(sample) / AudioSystem::instance()->adoptedSampleRate()  * 60 / PlaybackController::instance()->tempo() * 480;
}



AudioContext::AudioContext(QObject *parent) : QObject(parent) {
    connect(AudioSystem::instance()->transport(), &talcs::TransportAudioSource::positionAboutToChange, this, [=](qint64 positionSample) {
        if (AudioSystem::instance()->adoptedSampleRate())
            PlaybackController::instance()->setPosition(sampleToTick(positionSample));
    });
    connect(PlaybackController::instance(), &PlaybackController::playbackStatusChanged, this, &AudioContext::handlePlaybackStatusChange);
    connect(PlaybackController::instance(), &PlaybackController::lastPositionChanged, this, &AudioContext::handlePlaybackPositionChange);
}

void AudioContext::handlePlaybackStatusChange(PlaybackController::PlaybackStatus status) {
    QSettings settings;
    switch (status) {
        case PlaybackController::Stopped:
            AudioSystem::instance()->transport()->pause();
            if (settings.value("audio/closeDeviceOnPlaybackStop", false).toBool() && AudioSystem::instance()->device())
                AudioSystem::instance()->device()->stop();
            AudioSystem::instance()->transport()->setPosition(tickToSample(PlaybackController::instance()->lastPosition()));
            break;
        case PlaybackController::Playing:
            if (AudioSystem::instance()->device() && !AudioSystem::instance()->device()->isStarted())
                AudioSystem::instance()->device()->start(AudioSystem::instance()->playback());
            AudioSystem::instance()->transport()->play();
            break;
        case PlaybackController::Paused:
            AudioSystem::instance()->transport()->pause();
            break;
    }
}
void AudioContext::handlePlaybackPositionChange(double positionTick) {
    if (AudioSystem::instance()->adoptedSampleRate())
        AudioSystem::instance()->transport()->setPosition(tickToSample(positionTick));
}

void AudioContext::handleVstCallbackPositionChange(qint64 positionSample) {
    PlaybackController::instance()->setLastPosition(sampleToTick(positionSample));
}
