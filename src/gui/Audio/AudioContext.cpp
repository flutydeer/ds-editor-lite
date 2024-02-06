//
// Created by Crs_1 on 2024/2/4.
//

#include "AudioContext.h"

#include "AudioSystem.h"
#include "Views/TracksView.h"

#include <TalcsCore/TransportAudioSource.h>
#include <TalcsCore/AudioSourceClipSeries.h>
#include <TalcsCore/Decibels.h>
#include <TalcsCore/BufferingAudioSource.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsDevice/AudioSourcePlayback.h>
#include <TalcsSynthesis/FutureAudioSourceClipSeries.h>

#include <TalcsFormat/AudioFormatIO.h>
#include <TalcsFormat/AudioFormatInputSource.h>

#include <QFileDialog>
#include <QFile>
#include <QMessageBox>

static qint64 tickToSample(double tick) {
    return qint64(tick * 60.0 * AudioSystem::instance()->adoptedSampleRate() / PlaybackController::instance()->tempo() / 480.0);
}

static double sampleToTick(qint64 sample) {
    return double(sample) / AudioSystem::instance()->adoptedSampleRate()  * PlaybackController::instance()->tempo() / 60.0 * 480.0;
}



AudioContext::AudioContext(QObject *parent) : QObject(parent), m_levelMeterTimer(new QTimer(this)) {
    connect(AudioSystem::instance()->transport(), &talcs::TransportAudioSource::positionAboutToChange, this, [=](qint64 positionSample) {
        if (AudioSystem::instance()->adoptedSampleRate())
            PlaybackController::instance()->setPosition(sampleToTick(positionSample));
    });
    connect(PlaybackController::instance(), &PlaybackController::playbackStatusChanged, this, &AudioContext::handlePlaybackStatusChange);
    connect(PlaybackController::instance(), &PlaybackController::lastPositionChanged, this, &AudioContext::handlePlaybackPositionChange);

    connect(AppModel::instance(), &AppModel::modelChanged, this, &AudioContext::handleModelChange);

    connect(AppModel::instance(), &AppModel::tracksChanged, this, [=](AppModel::TrackChangeType type, int index, DsTrack *track) {
        Q_UNUSED(index)
        switch (type) {
            case AppModel::Insert:
                handleTrackInsertion(track);
                break;
            case AppModel::PropertyUpdate:
                handleTrackControlChange(track);
                break;
            case AppModel::Remove:
                handleTrackRemoval(track);
                break;
        }
    });

    connect(AppModel::instance(), &AppModel::tempoChanged, this, &AudioContext::rebuildAllClips);

    m_levelMeterTimer->setInterval(50); // TODO make it configurable
    connect(m_levelMeterTimer, &QTimer::timeout, this, [=] {
        AppModel::LevelMetersUpdatedArgs args;
        for (auto track : AppModel::instance()->tracks()) {
            if (!m_trackLevelMeterValue.contains(track))
                continue;
            if (PlaybackController::instance()->playbackStatus() != PlaybackController::Playing && (m_trackLevelMeterValue[track].first->targetValue() > -96 || m_trackLevelMeterValue[track].second->targetValue() > -96)) {
                m_trackLevelMeterValue[track].first->setTargetValue(-96);
                m_trackLevelMeterValue[track].second->setTargetValue(-96);
            }
            args.trackMeterStates.append({m_trackLevelMeterValue[track].first->nextValue(), m_trackLevelMeterValue[track].second->nextValue()});
        }
        emit levelMeterUpdated(args);
    });
    m_levelMeterTimer->start();
}

AudioContext::~AudioContext() {

}

talcs::FutureAudioSourceClipSeries *AudioContext::trackSynthesisClipSeries(const DsTrack *track) {
    return m_trackSynthesisClipSeriesDict[track];
}

void AudioContext::handlePlaybackStatusChange(PlaybackController::PlaybackStatus status) {
    QSettings settings;
    switch (status) {
        case PlaybackController::Stopped:
            AudioSystem::instance()->transport()->pause();
            if (settings.value("audio/closeDeviceOnPlaybackStop", false).toBool() && AudioSystem::instance()->device())
                AudioSystem::instance()->device()->close();
            AudioSystem::instance()->transport()->setPosition(tickToSample(PlaybackController::instance()->lastPosition()));
            break;
        case PlaybackController::Playing:
            if (AudioSystem::instance()->device() && !AudioSystem::instance()->device()->isOpen())
                AudioSystem::instance()->device()->open(AudioSystem::instance()->adoptedBufferSize(), AudioSystem::instance()->adoptedSampleRate());
            if (!AudioSystem::instance()->device() || !AudioSystem::instance()->device()->isOpen())
                QMessageBox::critical(nullptr, {}, tr("Cannot open audio device!"));
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

void AudioContext::handleModelChange() {
    for (auto track: m_trackItDict.keys()) {
        handleTrackRemoval(track);
    }

    for (auto track: AppModel::instance()->tracks()) {
        handleTrackInsertion(track);
    }
}

void AudioContext::handleTrackInsertion(const DsTrack *track) {
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
    m_trackLevelMeterValue[track] = {std::make_shared<talcs::SmoothedFloat>(-96), std::make_shared<talcs::SmoothedFloat>(-96)};
    m_trackLevelMeterValue[track].first->setRampLength(8); // TODO make it configurable
    m_trackLevelMeterValue[track].second->setRampLength(8);
    connect(trackSrc, &talcs::PositionableMixerAudioSource::levelMetered, this, [=](QVector<float> values) {
        if (!m_trackLevelMeterValue.contains(track))
            return;
        if (AudioSystem::instance()->masterTrack()->isMutedBySoloSetting(trackSrc))
            values = {0, 0};
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

    for (auto clip: track->clips()) {
        handleClipInsertion(track, clip);
    }

    connect(track, &DsTrack::clipChanged, this, [=](DsTrack::ClipChangeType type, int index, DsClip *clip) {
        Q_UNUSED(index)
        switch (type) {
            case DsTrack::Insert:
                handleClipInsertion(track, clip);
                break;
            case DsTrack::Remove:
                handleClipRemoval(track, clip);
                break;
            case DsTrack::PropertyChanged:
                handleClipPropertyChange(track, clip);
                break;
        }
    });
}

void AudioContext::handleTrackRemoval(const DsTrack *track) {
    for (auto clip: track->clips()) {
        handleClipRemoval(track, clip);
    }
    AudioSystem::instance()->masterTrack()->eraseSource(m_trackItDict[track]);
    m_trackItDict.remove(track);
    m_trackAudioClipSeriesDict.remove(track);
    m_trackSynthesisClipSeriesDict.remove(track);
    delete m_trackSourceDict[track];
    m_trackSourceDict.remove(track);
    m_trackLevelMeterValue.remove(track);
    disconnect(track, nullptr, this, nullptr);
}

void AudioContext::handleTrackControlChange(const DsTrack *track) {
    auto trackSource = m_trackSourceDict[track];
    auto dev = AudioSystem::instance()->device();
    if (dev)
        dev->lock();
    AudioSystem::instance()->masterTrack()->setSourceSolo(trackSource, track->control().solo());
    trackSource->setSilentFlags(track->control().mute() ? -1 : 0);
    trackSource->setGain(talcs::Decibels::decibelsToGain(track->control().gain()));
    trackSource->setPan(0.01f * track->control().pan());
    if (dev)
        dev->unlock();
}

void AudioContext::handleClipInsertion(const DsTrack *track, const DsClip *clip) {
    QSettings settings;
    if (clip->type() != DsClip::Audio)
        return;
    auto audioClip = static_cast<const DsAudioClip *>(clip);
    auto f = std::make_unique<QFile>(audioClip->path());
    auto fileSrc = new talcs::AudioFormatInputSource(new talcs::AudioFormatIO(f.get()), true);
    auto bufSrc = new talcs::BufferingAudioSource(
        fileSrc,
        true,
        2,
        settings.value("audio/fileBufferingSizeMsec", 1000.0).toDouble() / 1000.0 * AudioSystem::instance()->adoptedSampleRate());
    if (!fileSrc->audioFormatIo()->open(QIODevice::ReadOnly) || !fileSrc->open(1024, fileSrc->audioFormatIo()->sampleRate())) {
        QMessageBox::critical(nullptr, {}, tr("Cannot read audio file:\n\n%1").arg(audioClip->path()));
        return;
    }
    auto fileMixer = new talcs::PositionableMixerAudioSource;
    fileMixer->addSource(bufSrc, true);
    auto trackClipSeries = m_trackAudioClipSeriesDict[track];
    auto clipView = trackClipSeries->insertClip(fileMixer, tickToSample(clip->start() + clip->clipStart()), tickToSample(clip->clipStart()), qMax(1, tickToSample(clip->clipLen())));
    m_audioFiles[clip] = std::move(f);
    m_audioClips[clip] = clipView;
    m_audioClipMixers[clip] = fileMixer;
    m_audioClipBufferingSources[clip] = bufSrc;
}

void AudioContext::handleClipRemoval(const DsTrack *track, const DsClip *clip) {
    if (!m_trackAudioClipSeriesDict.contains(track))
        return;
    if (clip->type() != DsClip::Audio)
        return;
    auto clipView = m_audioClips[clip];
    if (!clipView.isNull()) {
        auto trackClipSeries = m_trackAudioClipSeriesDict[track];
        trackClipSeries->removeClip(clipView);
    }
    delete m_audioClipMixers[clip];
    m_audioClipMixers.remove(clip);
    m_audioFiles.remove(clip);
    m_audioClipBufferingSources.remove(clip);
}

void AudioContext::handleClipPropertyChange(const DsTrack *track, const DsClip *clip) {
    if (clip->type() != DsClip::Audio)
        return;
    auto filePath = static_cast<const DsAudioClip *>(clip)->path();
    if (filePath != m_audioFiles[clip]->fileName()) {
        handleClipRemoval(track, clip);
        handleClipInsertion(track, clip);
        return;
    }
    auto trackClipSeries = m_trackAudioClipSeriesDict[track];
    auto &clipView = m_audioClips[clip];
    if (clipView.isNull()) {
        if (m_audioClipMixers.contains(clip))
            clipView = trackClipSeries->insertClip(m_audioClipMixers[clip], tickToSample(clip->start() + clip->clipStart()), tickToSample(clip->clipStart()), qMax(1, tickToSample(clip->clipLen())));
    } else {
        trackClipSeries->setClipStartPos(clipView, tickToSample(clip->clipStart()));
        trackClipSeries->setClipRange(clipView, tickToSample(clip->start() + clip->clipStart()), qMax(1, tickToSample(clip->clipLen())));
    }
}

void AudioContext::rebuildAllClips() {
    for (auto track : AppModel::instance()->tracks()) {
        auto trackClipSeries = m_trackAudioClipSeriesDict[track];
        for (auto clip: track->clips()) {
            if (clip->type() != DsClip::Audio)
                continue;
            auto clipView = m_audioClips[clip];
            if (!clipView.isNull())
                trackClipSeries->removeClip(clipView);
        }
        for (auto clip: track->clips()) {
            if (clip->type() != DsClip::Audio)
                continue;
            if (m_audioClipMixers.contains(clip))
                m_audioClips[clip] = trackClipSeries->insertClip(m_audioClipMixers[clip], tickToSample(clip->start() + clip->clipStart()), tickToSample(clip->clipStart()), qMax(1, tickToSample(clip->clipLen())));
        }
    }
}

void AudioContext::handleFileBufferingSizeChange() {
    QSettings settings;
    for (auto bufSrc: m_audioClipBufferingSources) {
        bufSrc->setReadAheadSize(settings.value("audio/fileBufferingSizeMsec", 1000.0).toDouble() / 1000.0 * AudioSystem::instance()->adoptedSampleRate());
    }
}

void AudioContext::handleDeviceChangeDuringPlayback() {
    if (PlaybackController::instance()->playbackStatus() == PlaybackController::Playing) {
        if (AudioSystem::instance()->device() && !AudioSystem::instance()->device()->isStarted())
            AudioSystem::instance()->device()->start(AudioSystem::instance()->playback());
        AudioSystem::instance()->transport()->play();
    }
    if (AudioSystem::instance()->adoptedSampleRate())
        AudioSystem::instance()->transport()->setPosition(tickToSample(PlaybackController::instance()->position()));
}
