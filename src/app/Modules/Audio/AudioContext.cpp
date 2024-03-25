//
// Created by Crs_1 on 2024/2/4.
//

#include "AudioContext.h"

#include "AudioSystem.h"

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

#include "Model/Track.h"
#include "Model/Clip.h"
#include "Model/AppOptions/AppOptions.h"

static qint64 tickToSample(double tick) {
    return qint64(tick * 60.0 * AudioSystem::instance()->adoptedSampleRate() /
                  PlaybackController::instance()->tempo() / 480.0);
}

static double sampleToTick(qint64 sample) {
    return double(sample) / AudioSystem::instance()->adoptedSampleRate() *
           PlaybackController::instance()->tempo() / 60.0 * 480.0;
}

template <typename Iterator, typename Func, typename... Args>
static inline void applyListener(Iterator first, Iterator last, Func func, Args &&...args) {
    for (auto it = first; it != last; it++) {
        ((*it)->*func)(std::forward<Args>(args)...);
    }
}


AudioContext::AudioContext(QObject *parent) : QObject(parent), m_levelMeterTimer(new QTimer(this)) {
    connect(AudioSystem::instance()->transport(),
            &talcs::TransportAudioSource::positionAboutToChange, this, [=](qint64 positionSample) {
                if (AudioSystem::instance()->adoptedSampleRate())
                    PlaybackController::instance()->setPosition(sampleToTick(positionSample));
            });
    connect(PlaybackController::instance(), &PlaybackController::playbackStatusChanged, this,
            &AudioContext::handlePlaybackStatusChange);
    connect(PlaybackController::instance(), &PlaybackController::lastPositionChanged, this,
            &AudioContext::handlePlaybackPositionChange);

    connect(AppModel::instance(), &AppModel::modelChanged, this, &AudioContext::handleModelChange);

    connect(AppModel::instance(), &AppModel::tracksChanged, this,
            [=](AppModel::TrackChangeType type, int index, Track *track) {
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
            if (PlaybackController::instance()->playbackStatus() != PlaybackController::Playing &&
                (m_trackLevelMeterValue[track].first->targetValue() > -96 ||
                 m_trackLevelMeterValue[track].second->targetValue() > -96)) {
                m_trackLevelMeterValue[track].first->setTargetValue(-96);
                m_trackLevelMeterValue[track].second->setTargetValue(-96);
            }
            args.trackMeterStates.append({m_trackLevelMeterValue[track].first->nextValue(),
                                          m_trackLevelMeterValue[track].second->nextValue()});
        }
        emit levelMeterUpdated(args);
    });
    m_levelMeterTimer->start();
}

AudioContext::~AudioContext() {
}

talcs::FutureAudioSourceClipSeries *AudioContext::trackSynthesisClipSeries(const Track *track) {
    return m_trackSynthesisClipSeriesDict[track];
}

void AudioContext::handlePlaybackStatusChange(PlaybackController::PlaybackStatus status) {
    auto options = AppOptions::instance()->audio();
    switch (status) {
        case PlaybackController::Stopped:
            AudioSystem::instance()->transport()->pause();
            if (options->closeDeviceOnPlaybackStop && AudioSystem::instance()->device())
                AudioSystem::instance()->device()->close();
            AudioSystem::instance()->transport()->setPosition(
                tickToSample(PlaybackController::instance()->lastPosition()));
            break;
        case PlaybackController::Playing:
            if (AudioSystem::instance()->device() && !AudioSystem::instance()->device()->isOpen())
                AudioSystem::instance()->device()->open(
                    AudioSystem::instance()->adoptedBufferSize(),
                    AudioSystem::instance()->adoptedSampleRate());
            if (!AudioSystem::instance()->device() || !AudioSystem::instance()->device()->isOpen())
                QMessageBox::critical(nullptr, {}, tr("Cannot open audio device!"));
            if (AudioSystem::instance()->device() &&
                !AudioSystem::instance()->device()->isStarted())
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
    for (auto track : m_trackItDict.keys()) {
        handleTrackRemoval(track);
    }

    for (auto track : AppModel::instance()->tracks()) {
        handleTrackInsertion(track);
    }
}

void AudioContext::handleTrackInsertion(const Track *track) {
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
    m_trackLevelMeterValue[track] = {std::make_shared<talcs::SmoothedFloat>(-96),
                                     std::make_shared<talcs::SmoothedFloat>(-96)};
    m_trackLevelMeterValue[track].first->setRampLength(8); // TODO make it configurable
    m_trackLevelMeterValue[track].second->setRampLength(8);
    connect(trackSrc, &talcs::PositionableMixerAudioSource::levelMetered, this,
            [=](QVector<float> values) {
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

    for (auto clip : track->clips()) {
        handleClipInsertion(track, clip);
    }

    connect(track, &Track::clipChanged, this,
            [=](Track::ClipChangeType type, int index, Clip *clip) {
                Q_UNUSED(index)
                switch (type) {
                    case Track::Inserted:
                        handleClipInsertion(track, clip);
                        break;
                    case Track::Removed:
                        handleClipRemoval(track, clip);
                        break;
                    case Track::PropertyChanged:
                        handleClipPropertyChange(track, clip);
                        break;
                }
            });

    applyListener(m_synthesisListeners.cbegin(), m_synthesisListeners.cend(),
                  &SynthesisListener::trackInsertedCallback, track, trackSynthesisClipSeries);
}

void AudioContext::handleTrackRemoval(const Track *track) {
    applyListener(m_synthesisListeners.cbegin(), m_synthesisListeners.cend(),
                  &SynthesisListener::trackWillRemoveCallback, track,
                  m_trackSynthesisClipSeriesDict[track]);
    for (auto clip : track->clips()) {
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

void AudioContext::handleTrackControlChange(const Track *track) {
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

void AudioContext::handleClipInsertion(const Track *track, const Clip *clip) {
    auto options = AppOptions::instance()->audio();
    if (clip->type() != Clip::Audio)
        return;
    auto audioClip = static_cast<const AudioClip *>(clip);
    auto f = std::make_unique<QFile>(audioClip->path());
    auto fileSrc = new talcs::AudioFormatInputSource(new talcs::AudioFormatIO(f.get()), true);
    auto bufSrc = new talcs::BufferingAudioSource(fileSrc, true, 2,
                                                  options->fileBufferingSizeMsec / 1000.0 *
                                                      AudioSystem::instance()->adoptedSampleRate());
    if (!fileSrc->audioFormatIo()->open(QIODevice::ReadOnly) ||
        !fileSrc->open(1024, fileSrc->audioFormatIo()->sampleRate())) {
        QMessageBox::critical(nullptr, {},
                              tr("Cannot read audio file:\n\n%1").arg(audioClip->path()));
        return;
    }
    auto fileMixer = new talcs::PositionableMixerAudioSource;
    fileMixer->addSource(bufSrc, true);
    auto trackClipSeries = m_trackAudioClipSeriesDict[track];
    auto clipView = trackClipSeries->insertClip(
        fileMixer, tickToSample(clip->start() + clip->clipStart()), tickToSample(clip->clipStart()),
        qMax(1, tickToSample(clip->clipLen())));
    m_audioFiles[clip] = std::move(f);
    m_audioClips[clip] = clipView;
    m_audioClipMixers[clip] = fileMixer;
    m_audioClipBufferingSources[clip] = bufSrc;
}

void AudioContext::handleClipRemoval(const Track *track, const Clip *clip) {
    if (!m_trackAudioClipSeriesDict.contains(track))
        return;
    if (clip->type() != Clip::Audio)
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

void AudioContext::handleClipPropertyChange(const Track *track, const Clip *clip) {
    if (clip->type() != Clip::Audio)
        return;
    auto filePath = static_cast<const AudioClip *>(clip)->path();
    if (m_audioFiles.contains(clip) && filePath != m_audioFiles[clip]->fileName()) {
        handleClipRemoval(track, clip);
        handleClipInsertion(track, clip);
        return;
    }
    auto trackClipSeries = m_trackAudioClipSeriesDict[track];
    auto &clipView = m_audioClips[clip];
    if (clipView.isNull()) {
        if (m_audioClipMixers.contains(clip))
            clipView = trackClipSeries->insertClip(
                m_audioClipMixers[clip], tickToSample(clip->start() + clip->clipStart()),
                tickToSample(clip->clipStart()), qMax(1, tickToSample(clip->clipLen())));
    } else {
        trackClipSeries->setClipStartPos(clipView, tickToSample(clip->clipStart()));
        trackClipSeries->setClipRange(clipView, tickToSample(clip->start() + clip->clipStart()),
                                      qMax(1, tickToSample(clip->clipLen())));
    }
}

void AudioContext::rebuildAllClips() {
    for (auto track : AppModel::instance()->tracks()) {
        auto trackClipSeries = m_trackAudioClipSeriesDict[track];
        for (auto clip : track->clips()) {
            if (clip->type() != Clip::Audio)
                continue;
            auto clipView = m_audioClips[clip];
            if (!clipView.isNull())
                trackClipSeries->removeClip(clipView);
        }
        for (auto clip : track->clips()) {
            if (clip->type() != Clip::Audio)
                continue;
            if (m_audioClipMixers.contains(clip))
                m_audioClips[clip] = trackClipSeries->insertClip(
                    m_audioClipMixers[clip], tickToSample(clip->start() + clip->clipStart()),
                    tickToSample(clip->clipStart()), qMax(1, tickToSample(clip->clipLen())));
        }
    }
    applyListener(m_synthesisListeners.cbegin(), m_synthesisListeners.cend(),
                  &SynthesisListener::clipRebuildCallback);
}

void AudioContext::handleFileBufferingSizeChange() {
    auto options = AppOptions::instance()->audio();
    for (auto bufSrc : m_audioClipBufferingSources) {
        bufSrc->setReadAheadSize(options->fileBufferingSizeMsec / 1000.0 *
                                 AudioSystem::instance()->adoptedSampleRate());
    }
    applyListener(m_synthesisListeners.cbegin(), m_synthesisListeners.cend(),
                  &SynthesisListener::fileBufferingSizeChangeCallback);
}

void AudioContext::handleDeviceChangeDuringPlayback() {
    if (AudioSystem::instance()->adoptedSampleRate())
        AudioSystem::instance()->transport()->setPosition(
            tickToSample(PlaybackController::instance()->position()));
    if (PlaybackController::instance()->playbackStatus() == PlaybackController::Playing) {
        if (AudioSystem::instance()->device() && !AudioSystem::instance()->device()->isStarted())
            AudioSystem::instance()->device()->start(AudioSystem::instance()->playback());
        AudioSystem::instance()->transport()->play();
    }
}
