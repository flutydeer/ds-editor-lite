//
// Created by Crs_1 on 2024/2/4.
//

#include "AudioContext.h"

#include "AudioSystem.h"

#include <TalcsCore/TransportAudioSource.h>
#include <TalcsCore/AudioSourceClipSeries.h>
#include <TalcsCore/Decibels.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsDevice/AudioSourcePlayback.h>
#include <TalcsSynthesis/FutureAudioSourceClipSeries.h>

#include <TalcsFormat/AudioFormatIO.h>
#include <TalcsFormat/AudioFormatInputSource.h>

#include <QFileDialog>
#include <QFile>

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

    connect(AppModel::instance(), &AppModel::tracksChanged, this, [=](AppModel::TrackChangeType type, int index, DsTrack *track) {
        Q_UNUSED(index);
        switch (type) {
            case AppModel::Insert:
                handleTrackInsertion(track);
                break;
            case AppModel::PropertyUpdate:
                handleTrackControlChange(track, track->control().gain(), track->control().pan(), track->control().mute(), track->control().solo());
                break;
            case AppModel::Remove:
                handleTrackRemoval(track);
                break;
        }
    });
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

void AudioContext::handleTrackInsertion(DsTrack *track) {
    auto trackSrc = new talcs::PositionableMixerAudioSource;
    auto trackAudioClipSeries = new talcs::AudioSourceClipSeries;
    auto trackSynthesisClipSeries = new talcs::FutureAudioSourceClipSeries;
    trackSrc->addSource(trackAudioClipSeries, true);
    trackSrc->addSource(trackSynthesisClipSeries, true);
    trackSrc->setLevelMeterChannelCount(2);
    auto trackIt = AudioSystem::instance()->masterTrack()->appendSource(trackSrc, true);
    m_trackItDict[track] = trackIt;
    m_trackSourceDict[track] = trackSrc;
    m_trackAudioClipSeriesDict[track] = trackAudioClipSeries;
    m_trackSynthesisClipSeriesDict[track] = trackSynthesisClipSeries;
    m_trackLevelMeterValue[track] = {new talcs::SmoothedFloat, new talcs::SmoothedFloat};
    connect(trackSrc, &talcs::PositionableMixerAudioSource::levelMetered, this, [=](const QVector<float> &values) {
        if (!m_trackLevelMeterValue.contains(track))
            return;
        auto dBL = talcs::Decibels::gainToDecibels(values[0]);
        auto dBR = talcs::Decibels::gainToDecibels(values[1]);
        if (dBL < m_trackLevelMeterValue[track].first->currentValue())
            m_trackLevelMeterValue[track].first->setTargetValue(dBL);
        else
            m_trackLevelMeterValue[track].first->setCurrentAndTargetValue(dBL);

        if (dBR < m_trackLevelMeterValue[track].second->currentValue())
            m_trackLevelMeterValue[track].second->setTargetValue(dBR);
        else
            m_trackLevelMeterValue[track].second->setCurrentAndTargetValue(dBR);
    });

    auto s = QFileDialog::getOpenFileName();
    if (s.isEmpty())
        return;
    auto f = new QFile(s);
    auto fileSrc = new talcs::AudioFormatInputSource(new talcs::AudioFormatIO(f), true);
    fileSrc->audioFormatIo()->open(QIODevice::ReadOnly);
    if (!fileSrc->open(1024, fileSrc->audioFormatIo()->sampleRate()))
        return;
    trackSrc->appendSource(fileSrc, true);
    QObject::connect(trackSrc, &QObject::destroyed, f, &QObject::deleteLater);
}

void AudioContext::handleTrackRemoval(DsTrack *track) {
    AudioSystem::instance()->masterTrack()->eraseSource(m_trackItDict[track]);
    m_trackItDict.remove(track);
    m_trackAudioClipSeriesDict.remove(track);
    m_trackSynthesisClipSeriesDict.remove(track);
    delete m_trackSourceDict[track];
    m_trackSourceDict.remove(track);
    delete m_trackLevelMeterValue[track].first;
    delete m_trackLevelMeterValue[track].second;
    m_trackLevelMeterValue.remove(track);
}

void AudioContext::handleTrackControlChange(DsTrack *track, float gainDb, float pan100x, bool mute, bool solo) {
    auto trackSource = m_trackSourceDict[track];
    auto dev = AudioSystem::instance()->device();
    if (dev)
        dev->lock();
    AudioSystem::instance()->masterTrack()->setSourceSolo(trackSource, solo);
    trackSource->setSilentFlags(mute ? -1 : 0);
    trackSource->setGain(talcs::Decibels::decibelsToGain(gainDb));
    trackSource->setPan(0.01f * pan100x);
    if (dev)
        dev->unlock();
}
