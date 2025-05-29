#include "AudioContext.h"

#include "TrackInferenceHandler.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/InferPiece.h"
#include "Model/AppModel/SingingClip.h"

#include <set>

#include <QMessageBox>
#include <QFile>
#include <QBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QDoubleValidator>
#include <QSpinBox>
#include <QPushButton>

#include <TalcsCore/MixerAudioSource.h>
#include <TalcsCore/PositionableMixerAudioSource.h>
#include <TalcsCore/Decibels.h>
#include <TalcsCore/TransportAudioSource.h>
#include <TalcsFormat/FormatEntry.h>
#include <TalcsFormat/FormatManager.h>
#include <TalcsFormat/AudioFormatIO.h>
#include <TalcsDevice/AbstractOutputContext.h>
#include <TalcsDevice/AudioSourcePlayback.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsDspx/DspxTrackContext.h>
#include <TalcsDspx/DspxAudioClipContext.h>
#include <TalcsWidgets/StandardFormatEntry.h>
#include <TalcsWidgets/WavpackFormatEntry.h>

#include <Modules/Audio/AudioSystem.h>
#include <Modules/Audio/subsystem/OutputSystem.h>
#include <Modules/Audio/AudioSettings.h>
#include <Modules/Audio/TrackSynthesizer.h>

#include "Model/AppModel/Track.h"
#include "utils/PseudoSingerConfigNotifier.h"

#include <Model/AppOptions/AppOptions.h>

#define DEVICE_LOCKER                                                                              \
    talcs::AudioDeviceLocker locker(AudioSystem::outputSystem()->context()->device())

class AudioFormatIOObject : public QObject, public talcs::AudioFormatIO {
public:
    explicit AudioFormatIOObject(QIODevice *stream = nullptr, QObject *parent = nullptr)
        : QObject(parent), talcs::AudioFormatIO(stream) {
    }

    ~AudioFormatIOObject() override = default;
};

static AudioContext *m_instance = nullptr;

static AudioExporter *m_exporter = nullptr;

static qint64 tickToSample(double tick) {
    auto sr = m_instance->preMixer()->isOpen() ? m_instance->preMixer()->sampleRate() : 48000.0;
    return qint64(tick * 60.0 * sr / appModel->tempo() / 480.0);
}

static double sampleToTick(qint64 sample) {
    auto sr = m_instance->preMixer()->isOpen() ? m_instance->preMixer()->sampleRate() : 48000.0;
    return double(sample) / sr * appModel->tempo() / 60.0 * 480.0;
}

AudioContext::AudioContext(QObject *parent) : DspxProjectContext(parent) {
    m_instance = this;

    AudioSystem::outputSystem()->context()->preMixer()->addSource(preMixer());

    setTimeConverter(&tickToSample);

    auto formatManager = new talcs::FormatManager(this);
    formatManager->addEntry(new talcs::StandardFormatEntry);
    formatManager->addEntry(new talcs::WavpackFormatEntry);

    setFormatManager(formatManager);

    setBufferingReadAheadSize(AudioSettings::fileBufferingReadAheadSize());

    connect(transport(), &talcs::TransportAudioSource::positionAboutToChange, this,
            [=](qint64 positionSample) {
                m_transportPositionFlag = false;
                playbackController->setPosition(sampleToTick(positionSample));
                m_transportPositionFlag = true;
            });

    connect(transport(), &talcs::TransportAudioSource::playbackStatusChanged, this,
            [=](auto status) {
                if (status == talcs::TransportAudioSource::Paused &&
                    playbackController->playbackStatus() == PlaybackGlobal::Stopped) {
                    if (AudioSettings::playheadBehavior() == ReturnToStart)
                        playbackController->setPosition(playbackController->lastPosition());
                }
            });

    connect(playbackController, &PlaybackController::playbackStatusChanged, this,
            &AudioContext::handlePlaybackStatusChanged);
    connect(playbackController, &PlaybackController::positionChanged, this,
            &AudioContext::handlePlaybackPositionChanged);

    connect(appModel, &AppModel::modelChanged, this, [this] {
        DEVICE_LOCKER;
        handleModelChanged();
        handleMasterControlChanged(appModel->masterControl());
    });
    connect(appModel, &AppModel::trackChanged, this,
            [=](AppModel::TrackChangeType type, int index, Track *track) {
                DEVICE_LOCKER;
                switch (type) {
                    case AppModel::Insert:
                        handleTrackInserted(index, track);
                        break;
                    case AppModel::Remove:
                        handleTrackRemoved(index, track);
                        break;
                }
            });

    connect(appModel, &AppModel::tempoChanged, this, [=] {
        DEVICE_LOCKER;
        handleTimeChanged();
        handlePlaybackPositionChanged(playbackController->position());
    });

    connect(appModel, &AppModel::masterControlChanged, this, [=](const TrackControl &control) {
        DEVICE_LOCKER;
        handleMasterControlChanged(control);
    });

    connect(AudioSystem::outputSystem()->context(),
            &talcs::AbstractOutputContext::bufferSizeChanged, this,
            [=] { playbackController->stop(); });

    connect(AudioSystem::outputSystem()->context(),
            &talcs::AbstractOutputContext::sampleRateChanged, this, [=] {
                DEVICE_LOCKER;
                handleTimeChanged();
                playbackController->stop();
            });

    connect(AudioSystem::outputSystem()->context(), &talcs::AbstractOutputContext::deviceChanged,
            this, [=] { playbackController->stop(); });

    connect(AudioSystem::outputSystem(), &AbstractOutputSystem::fileBufferingReadAheadSizeChanged,
            this, &AudioContext::setBufferingReadAheadSize);

    connect(this, &AudioContext::exporterCausedTimeChanged, this, &AudioContext::handleTimeChanged);

    masterChannel = new Track;
    m_trackLevelMeterValue[masterChannel] = {std::make_shared<talcs::SmoothedFloat>(-96),
                                             std::make_shared<talcs::SmoothedFloat>(-96)};
    m_trackLevelMeterValue[masterChannel].first->setRampLength(
        m_levelMeterRampLength); // TODO make it configurable
    m_trackLevelMeterValue[masterChannel].second->setRampLength(m_levelMeterRampLength);
    auto trackControlMixer = masterControlMixer();
    trackControlMixer->setLevelMeterChannelCount(2);
    connect(trackControlMixer, &talcs::PositionableMixerAudioSource::levelMetered, this,
            [=](QVector<float> values) {
                // if (!m_trackLevelMeterValue.contains(masterChannel))
                //     return;
                if (masterTrackMixer()->isMutedBySoloSetting(trackControlMixer))
                    values = {0, 0};
                updateTrackLevelMeterValue(masterChannel, values);
            });

    m_levelMeterTimer = new QTimer(this);
    m_levelMeterTimer->setInterval(8); // TODO make it configurable
    connect(m_levelMeterTimer, &QTimer::timeout, this, [=] {
        AppModel::LevelMetersUpdatedArgs args;

        auto addChannelLevel = [&](Track *track) {
            if (playbackController->playbackStatus() != Playing &&
                (m_trackLevelMeterValue[track].first->targetValue() > -96 ||
                 m_trackLevelMeterValue[track].second->targetValue() > -96)) {
                m_trackLevelMeterValue[track].first->setTargetValue(-96);
                m_trackLevelMeterValue[track].second->setTargetValue(-96);
            }
            args.trackMeterStates.append({m_trackLevelMeterValue[track].first->nextValue(),
                                          m_trackLevelMeterValue[track].second->nextValue()});
        };
        for (auto track : appModel->tracks()) {
            if (!m_trackLevelMeterValue.contains(track))
                continue;
            addChannelLevel(track);
        }
        // Add master level
        addChannelLevel(masterChannel);
        emit levelMeterUpdated(args);
    });
    m_levelMeterTimer->start();

    new PseudoSingerConfigNotifier(this);

    AudioExporter::registerListener(this);
}

AudioContext::~AudioContext() {
    for (auto trackSynthesizer : m_trackSynthDict.values()) {
        delete trackSynthesizer;
    }
    m_instance = nullptr;
}

AudioContext *AudioContext::instance() {
    return m_instance;
}

Track *AudioContext::getTrackFromContext(talcs::DspxTrackContext *trackContext) const {
    Q_UNUSED(this)
    return trackContext->data().value<Track *>();
}

AudioClip *
    AudioContext::getAudioClipFromContext(talcs::DspxAudioClipContext *audioClipContext) const {
    Q_UNUSED(this)
    return audioClipContext->data().value<AudioClip *>();
}

talcs::DspxTrackContext *AudioContext::getContextFromTrack(Track *trackModel) const {
    return m_trackModelDict.value(trackModel);
}

talcs::DspxAudioClipContext *
    AudioContext::getContextFromAudioClip(AudioClip *audioClipModel) const {
    return m_audioClipModelDict.value(audioClipModel);
}

void AudioContext::handlePanSliderMoved(Track *track, double pan) const {
    auto trackContext = getContextFromTrack(track);
    trackContext->controlMixer()->setPan(static_cast<float>(.01 * pan));
}

void AudioContext::handleGainSliderMoved(Track *track, double gain) const {
    auto trackContext = getContextFromTrack(track);
    trackContext->controlMixer()->setGain(talcs::Decibels::decibelsToGain(gain));
}

void AudioContext::handleMasterGainSliderMoved(double gain) const {
    masterControlMixer()->setGain(talcs::Decibels::decibelsToGain(gain));
}

void AudioContext::handleInferPieceFailed() const {
    if (m_exporter)
        m_exporter->cancel(true, tr("Inference failed"));
}

void AudioContext::handlePlaybackStatusChanged(PlaybackStatus status) {
    auto device = AudioSystem::outputSystem()->context()->device();
    switch (status) {
        case Stopped:
            transport()->pause();
            break;
        case Playing:
            if (!device || !device->isOpen())
                QMessageBox::critical(nullptr, {}, tr("Cannot open audio device!"));
            if (device && !device->isStarted())
                device->start(AudioSystem::outputSystem()->context()->playback());
            if (m_lastStatus == Stopped) {
                if (AudioSettings::playheadBehavior() == KeepAtCurrentButPlayFromStart)
                    playbackController->setPosition(playbackController->lastPosition());
                else
                    playbackController->setLastPosition(playbackController->position());
            }
            transport()->play();
            break;
        case Paused:
            transport()->pause();
            break;
    }
    m_lastStatus = status;
}

void AudioContext::handlePlaybackPositionChanged(double positionTick) {
    if (m_transportPositionFlag)
        transport()->setPosition(tickToSample(positionTick));
}

void AudioContext::handleModelChanged() {
    auto oldTrackContexts = tracks();
    for (int i = static_cast<int>(oldTrackContexts.size()) - 1; i >= 0; i--) {
        handleTrackRemoved(i, getTrackFromContext(oldTrackContexts[i]));
    }
    auto newTracks = appModel->tracks();
    for (int i = 0; i < newTracks.size(); i++) {
        handleTrackInserted(i, newTracks[i]);
    }
}

void AudioContext::handleTrackInserted(int index, Track *track) {
    auto trackContext = addTrack(index);
    trackContext->setData(QVariant::fromValue(track));
    m_trackModelDict.insert(track, trackContext);

    handleTrackControlChanged(track);
    for (auto clip : track->clips()) {
        if (clip->clipType() != Clip::Audio)
            continue;
        handleClipInserted(track, clip->id(), static_cast<AudioClip *>(clip));
    }

    connect(track, &Track::propertyChanged, this, [=] {
        DEVICE_LOCKER;
        handleTrackControlChanged(track);
    });

    connect(track, &Track::clipChanged, this, [=](Track::ClipChangeType type, Clip *clip) {
        if (clip->clipType() != Clip::Audio)
            return;
        DEVICE_LOCKER;
        switch (type) {
            case Track::Inserted:
                handleClipInserted(track, clip->id(), static_cast<AudioClip *>(clip));
                break;
            case Track::Removed:
                handleClipRemoved(track, clip->id(), static_cast<AudioClip *>(clip));
                break;
        }
    });

    m_trackLevelMeterValue[track] = {std::make_shared<talcs::SmoothedFloat>(-96),
                                     std::make_shared<talcs::SmoothedFloat>(-96)};
    m_trackLevelMeterValue[track].first->setRampLength(
        m_levelMeterRampLength); // TODO make it configurable
    m_trackLevelMeterValue[track].second->setRampLength(m_levelMeterRampLength);
    auto trackControlMixer = trackContext->controlMixer();
    trackControlMixer->setLevelMeterChannelCount(2);
    connect(trackControlMixer, &talcs::PositionableMixerAudioSource::levelMetered, this,
            [=](QVector<float> values) {
                if (!m_trackLevelMeterValue.contains(track))
                    return;
                if (masterTrackMixer()->isMutedBySoloSetting(trackControlMixer))
                    values = {0, 0};
                auto dBL = static_cast<float>(talcs::Decibels::gainToDecibels(values[0]));
                auto dBR = static_cast<float>(talcs::Decibels::gainToDecibels(values[1]));
                if (dBL < m_trackLevelMeterValue[track].first->currentValue())
                    m_trackLevelMeterValue[track].first->setTargetValue(dBL);
                else
                    m_trackLevelMeterValue[track].first->setCurrentAndTargetValue(dBL);

                if (dBR < m_trackLevelMeterValue[track].second->currentValue())
                    m_trackLevelMeterValue[track].second->setTargetValue(dBR);
                else
                    m_trackLevelMeterValue[track].second->setCurrentAndTargetValue(dBR);
            });

    // m_trackSynthDict.insert(track, new TrackSynthesizer(trackContext, track));
    m_trackInferDict.insert(track, new TrackInferenceHandler(trackContext, track));
}

void AudioContext::handleTrackRemoved(int index, Track *track) {
    for (auto clip : track->clips()) {
        if (clip->clipType() != Clip::Audio)
            continue;
        handleClipRemoved(track, clip->id(), static_cast<AudioClip *>(clip));
    }
    disconnect(track, nullptr, this, nullptr);
    // delete m_trackSynthDict.take(track);
    removeTrack(index);
    m_trackInferDict.remove(track);
    m_trackModelDict.remove(track);
    m_trackLevelMeterValue.remove(track);
}

void AudioContext::handleMasterControlChanged(const TrackControl &control) {
    masterControlMixer()->setGain(talcs::Decibels::decibelsToGain(control.gain()));
}

void AudioContext::handleTrackControlChanged(Track *track) {
    auto trackContext = getContextFromTrack(track);
    trackContext->controlMixer()->setGain(talcs::Decibels::decibelsToGain(track->control().gain()));
    trackContext->controlMixer()->setPan(static_cast<float>(.01 * track->control().pan()));
    trackContext->controlMixer()->setSilentFlags(track->control().mute() ? -1 : 0);
    masterTrackMixer()->setSourceSolo(trackContext->controlMixer(), track->control().solo());
}

void AudioContext::handleClipInserted(Track *track, int id, AudioClip *audioClip) {
    auto trackContext = getContextFromTrack(track);
    auto audioClipContext = trackContext->addAudioClip(id);
    m_audioClipModelDict.insert(audioClip, audioClipContext);

    handleClipPropertyChanged(audioClip);

    connect(audioClip, &Clip::propertyChanged, this, [=] {
        DEVICE_LOCKER;
        handleClipPropertyChanged(audioClip);
    });
}

void AudioContext::handleClipRemoved(Track *track, int id, AudioClip *audioClip) {
    disconnect(audioClip, nullptr, this, nullptr);
    auto trackContext = getContextFromTrack(track);
    trackContext->removeAudioClip(id);
    m_audioClipModelDict.remove(audioClip);
}

void AudioContext::handleClipPropertyChanged(AudioClip *audioClip) const {
    auto audioClipContext = getContextFromAudioClip(audioClip);

    audioClipContext->setStart(audioClip->start());
    audioClipContext->setClipStart(audioClip->clipStart());
    audioClipContext->setClipLen(audioClip->clipLen());

    audioClipContext->controlMixer()->setGain(talcs::Decibels::decibelsToGain(audioClip->gain()));
    audioClipContext->controlMixer()->setSilentFlags(audioClip->mute() ? -1 : 0);

    auto workspace = audioClip->workspace().value("diffscope.audio.formatData");
    QVariant userData;
    QDataStream o(QByteArray::fromBase64(workspace.value("userData").toString().toLatin1()));
    o >> userData;
    auto entryClassName = workspace.value("entryClassName").toString();

    if (audioClip->path() != audioClipContext->path()) {
        audioClipContext->setPathLoad(audioClip->path(), userData, entryClassName);
    }
}

void AudioContext::handleTimeChanged() {
    for (int i = 0; i < 4; i++)
        PseudoSingerConfigNotifier::notify(i);
    for (auto trackContext : tracks()) {
        for (auto audioClipContext : trackContext->clips()) {
            audioClipContext->updatePosition();
        }
    }
}

bool AudioContext::willStartCallback(AudioExporter *exporter) {
    m_exporter = exporter;
    playbackController->stop();
    setBufferingReadAheadSize(0);
    emit exporterCausedTimeChanged();
    for (auto trackInferenceHandler : m_trackInferDict.values()) {
        trackInferenceHandler->setMode(talcs::DspxTrackInferenceContext::Export);
    }
    bool isOK = [=] {
        for (auto track : m_trackInferDict.keys()) {
            for (auto clip : track->clips()) {
                if (clip->clipType() != Clip::Singing)
                    continue;
                for (auto piece : static_cast<SingingClip *>(clip)->pieces()) {
                    if (piece->acousticInferStatus == Failed)
                        return false;
                }
            }
        }
        return true;
    }();
    if (!isOK)
        exporter->cancel(true, tr("Inference failed"));
    return isOK;
}

void AudioContext::willFinishCallback(AudioExporter *exporter) {
    setBufferingReadAheadSize(AudioSystem::outputSystem()->fileBufferingReadAheadSize());
    emit exporterCausedTimeChanged();
    for (auto trackInferenceHandler : m_trackInferDict.values()) {
        trackInferenceHandler->setMode(talcs::DspxTrackInferenceContext::Default);
    }
    m_exporter = nullptr;
}

void AudioContext::updateTrackLevelMeterValue(Track *track, QList<float> values) {
    auto dBL = static_cast<float>(talcs::Decibels::gainToDecibels(values[0]));
    auto dBR = static_cast<float>(talcs::Decibels::gainToDecibels(values[1]));
    if (dBL < m_trackLevelMeterValue[track].first->currentValue())
        m_trackLevelMeterValue[track].first->setTargetValue(dBL);
    else
        m_trackLevelMeterValue[track].first->setCurrentAndTargetValue(dBL);

    if (dBR < m_trackLevelMeterValue[track].second->currentValue())
        m_trackLevelMeterValue[track].second->setTargetValue(dBR);
    else
        m_trackLevelMeterValue[track].second->setCurrentAndTargetValue(dBR);
}
