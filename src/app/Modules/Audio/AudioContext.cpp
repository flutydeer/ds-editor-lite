#include "AudioContext.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"

#include "TrackInferenceHandler.h"
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QBoxLayout>
#include <QFormLayout>
#include <QComboBox>

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

#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/AppModel/LoopSettings.h>
#include "Model/AppStatus/AppStatus.h"
#include <lite/GUI/Controls/LevelMeterViewModel.h>
#include "utils/PseudoSingerConfigNotifier.h"

#include <Model/AppOptions/AppOptions.h>
#include "Global/AppGlobal.h"
#include "AppContext.h"
#include "UI/Controls/LevelMeterManager.h"

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

static qint64 tickToSample(const double tick) {
    const auto sr =
        m_instance->preMixer()->isOpen() ? m_instance->preMixer()->sampleRate() : 48000.0;
    // Piecewise mapping over the tempo map: anchor on the governing tempo
    // point and extrapolate linearly inside the segment. With a single tempo
    // point this reduces exactly to the old constant-tempo formula.
    const auto &timeline = appModel->timeline();
    const int refTick =
        timeline.nearestTickWithTempoTo(static_cast<int>(std::floor(qMax(0.0, tick))));
    const auto tempo = timeline.tempoAt(refTick);
    const auto refSamples = timeline.tickToMs(refTick) / 1000.0 * sr;
    return static_cast<qint64>(refSamples + (tick - refTick) * 60.0 * sr / tempo /
                                                AppGlobal::ticksPerQuarterNote);
}

static double sampleToTick(const qint64 sample) {
    const auto sr =
        m_instance->preMixer()->isOpen() ? m_instance->preMixer()->sampleRate() : 48000.0;
    return appModel->timeline().msToTick(static_cast<double>(sample) / sr * 1000.0);
}

AudioContext::AudioContext(QObject *parent) : DspxProjectContext(parent) {
    m_instance = this;

    AudioSystem::outputSystem()->context()->preMixer()->addSource(preMixer());

    setTimeConverter(&tickToSample);

    const auto formatManager = new talcs::FormatManager(this);
    formatManager->addEntry(new talcs::StandardFormatEntry);
    formatManager->addEntry(new talcs::WavpackFormatEntry);

    setFormatManager(formatManager);

    setBufferingReadAheadSize(AudioSettings::fileBufferingReadAheadSize());

    connect(transport(), &talcs::TransportAudioSource::positionAboutToChange, this,
            [this](const qint64 positionSample) {
                m_transportPositionFlag = false;
                playbackController->setPosition(sampleToTick(positionSample));
                m_transportPositionFlag = true;
            });

    connect(transport(), &talcs::TransportAudioSource::playbackStatusChanged, this,
            [this](auto status) {
                if (status != talcs::TransportAudioSource::Paused)
                    return;
                if (playbackController->playbackStatus() == PlaybackGlobal::Playing) {
                    playbackController->pause();
                } else if (playbackController->playbackStatus() == PlaybackGlobal::Stopped &&
                           AudioSettings::playheadBehavior() == ReturnToStart) {
                    playbackController->setPosition(playbackController->lastPosition());
                }
            });

    connect(playbackController, &PlaybackController::playbackStatusChanged, this,
            &AudioContext::handlePlaybackStatusChanged);
    connect(playbackController, &PlaybackController::positionChanged, this,
            &AudioContext::handlePlaybackPositionChanged);
    playbackController->setPlaybackStartGuard([this] { return ensurePlaybackDeviceStarted(); });

    connect(appModel, &AppModel::modelChanged, this, [this] {
        DEVICE_LOCKER;
        handleModelChanged();
        handleMasterControlChanged(appModel->masterControl());
        // Reapply loop settings after model change
        updateLoopingRange();
    });
    connect(appModel, &AppModel::trackChanged, this,
            [this](const AppModel::TrackChangeType type, const int index, Track *track) {
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
    connect(appModel, &AppModel::trackMoved, this,
            [this](const qsizetype from, const qsizetype to) {
                DEVICE_LOCKER;
                handleTrackMoved(static_cast<int>(from), static_cast<int>(to));
            });

    connect(appModel, &AppModel::timelineChanged, this, [this] {
        DEVICE_LOCKER;
        handleTimeChanged();
        handlePlaybackPositionChanged(playbackController->position());
        // The loop sample range depends on the tempo map
        updateLoopingRange();
    });

    connect(appModel, &AppModel::masterControlChanged, this, [this](const TrackControl &control) {
        DEVICE_LOCKER;
        handleMasterControlChanged(control);
    });

    connect(AudioSystem::outputSystem()->context(),
            &talcs::AbstractOutputContext::bufferSizeChanged, this,
            [this] { playbackController->stop(); });

    connect(AudioSystem::outputSystem()->context(),
            &talcs::AbstractOutputContext::sampleRateChanged, this, [this] {
                DEVICE_LOCKER;
                handleTimeChanged();
                // The loop sample range depends on the sample rate
                updateLoopingRange();
                playbackController->stop();
            });

    connect(AudioSystem::outputSystem()->context(), &talcs::AbstractOutputContext::deviceChanged,
            this, [this] { playbackController->stop(); });

    connect(AudioSystem::outputSystem(), &AbstractOutputSystem::fileBufferingReadAheadSizeChanged,
            this, &AudioContext::setBufferingReadAheadSize);

    connect(this, &AudioContext::exporterCausedTimeChanged, this, &AudioContext::handleTimeChanged);

    // Connect loop settings changes to transport
    connect(appStatus, &AppStatus::loopSettingsChanged, this, [this](const LoopSettings &settings) {
        updateLoopingRange();
        if (settings.enabled) {
            auto currentPos = playbackController->position();
            if (currentPos < settings.start || currentPos >= settings.end()) {
                playbackController->setPosition(settings.start);
            }
        }
    });

    m_masterLevelMeterValueL = std::make_shared<talcs::SmoothedFloat>(-96);
    m_masterLevelMeterValueR = std::make_shared<talcs::SmoothedFloat>(-96);
    m_masterLevelMeterValueL->setRampLength(m_levelMeterRampLength);
    m_masterLevelMeterValueR->setRampLength(m_levelMeterRampLength);
    auto trackControlMixer = masterControlMixer();
    trackControlMixer->setLevelMeterChannelCount(2);
    connect(trackControlMixer, &talcs::PositionableMixerAudioSource::levelMetered, this,
            [trackControlMixer, this](QVector<float> values) {
                if (masterTrackMixer()->isMutedBySoloSetting(trackControlMixer))
                    values = {0, 0};
                auto dBL = static_cast<float>(talcs::Decibels::gainToDecibels(values[0]));
                auto dBR = static_cast<float>(talcs::Decibels::gainToDecibels(values[1]));
                updateSmoothedValue(m_masterLevelMeterValueL, dBL);
                updateSmoothedValue(m_masterLevelMeterValueR, dBR);
            });

    m_levelMeterTimer = new QTimer(this);
    m_levelMeterTimer->setSingleShot(true);
    connect(m_levelMeterTimer, &QTimer::timeout, this, &AudioContext::tickLevelMeters);

    new PseudoSingerConfigNotifier(this);

    AudioExporter::registerListener(this);
}

AudioContext::~AudioContext() {
    playbackController->setPlaybackStartGuard({});
    for (const auto trackSynthesizer : m_trackSynthDict.values()) {
        delete trackSynthesizer;
    }
    m_instance = nullptr;
}

AudioContext *AudioContext::instance() {
    return m_instance;
}

AudioContext::ExportInferenceStatus AudioContext::exportInferenceStatus() const {
    return exportInferenceStatus(m_trackInferDict.keys());
}

AudioContext::ExportInferenceStatus
    AudioContext::exportInferenceStatus(const QList<Track *> &tracks) const {
    auto result = ExportInferenceStatus::Ready;
    for (const auto track : tracks) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() != Clip::Singing)
                continue;
            for (const auto piece : static_cast<SingingClip *>(clip)->pieces()) {
                if (piece->acousticInferStatus == Failed)
                    return ExportInferenceStatus::Failed;
                if (piece->acousticInferStatus != Success)
                    result = ExportInferenceStatus::Pending;
            }
        }
    }
    return result;
}

Track *AudioContext::getTrackFromContext(const talcs::DspxTrackContext *trackContext) const {
    Q_UNUSED(this)
    return trackContext->data().value<Track *>();
}

auto AudioContext::getAudioClipFromContext(
    const talcs::DspxAudioClipContext *audioClipContext) const -> AudioClip * {
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

void AudioContext::handlePanSliderMoved(Track *track, const double pan) const {
    const auto trackContext = getContextFromTrack(track);
    trackContext->controlMixer()->setPan(static_cast<float>(pan));
}

void AudioContext::handleGainSliderMoved(Track *track, const double gain) const {
    const auto trackContext = getContextFromTrack(track);
    trackContext->controlMixer()->setGain(talcs::Decibels::decibelsToGain(gain));
}

void AudioContext::handleMasterPanSliderMoved(const double pan) const {
    masterControlMixer()->setPan(static_cast<float>(pan));
}

void AudioContext::handleMasterGainSliderMoved(const double gain) const {
    masterControlMixer()->setGain(talcs::Decibels::decibelsToGain(gain));
}

void AudioContext::handleInferPieceFailed() {
    if (m_exporter)
        m_exporter->cancel(true, tr("Inference failed"));
}

void AudioContext::handlePlaybackStatusChanged(const PlaybackStatus status) {
    switch (status) {
        case Stopped:
            transport()->pause();
            break;
        case Playing:
            if (!m_levelMeterActive) {
                m_levelMeterActive = true;
                m_levelMeterTickTime.start();
                tickLevelMeters();
            }
            if (m_lastStatus == Stopped) {
                if (AudioSettings::playheadBehavior() == KeepAtCurrentButPlayFromStart)
                    playbackController->setPosition(playbackController->lastPosition());
                else
                    playbackController->setLastPosition(playbackController->position());
            }
            {
                auto loopSettings = appStatus->loopSettings.get();
                if (loopSettings.enabled) {
                    auto pos = playbackController->position();
                    if (pos < loopSettings.start || pos >= loopSettings.end())
                        playbackController->setPosition(loopSettings.start);
                }
            }
            transport()->play();
            break;
        case Paused:
            transport()->pause();
            break;
    }
    m_lastStatus = status;
}

bool AudioContext::ensurePlaybackDeviceStarted() const {
    const auto outputContext = AudioSystem::outputSystem()->context();
    const auto device = outputContext->device();
    if (device && device->isOpen() &&
        (device->isStarted() || device->start(outputContext->playback()))) {
        return true;
    }
    QMessageBox::critical(nullptr, {}, tr("Cannot open audio device!"));
    return false;
}

void AudioContext::handlePlaybackPositionChanged(const double positionTick) const {
    if (m_transportPositionFlag)
        transport()->setPosition(tickToSample(positionTick));
    // Stop playback once the playhead passes the end of the project (the last
    // clip's end). Only act while actually playing so that scrubbing the
    // playhead never fires a spurious stop.
    if (playbackController->playbackStatus() != PlaybackGlobal::Playing)
        return;
    const int projectEndTick = appModel->projectLengthInTicks();
    if (projectEndTick > 0 && positionTick >= projectEndTick)
        playbackController->stop();
}

void AudioContext::tickLevelMeters() {
    const auto status = playbackController->playbackStatus();
    const bool notPlaying = status != Playing;

    auto addTrackLevels = [&](const Track *track) {
        if (!m_trackLevelMeterValue.contains(track))
            return;
        auto &pair = m_trackLevelMeterValue[track];
        if (notPlaying && (pair.first->targetValue() > -96 || pair.second->targetValue() > -96)) {
            pair.first->setTargetValue(-96);
            pair.second->setTargetValue(-96);
        }
        auto dBL = static_cast<double>(pair.first->nextValue());
        auto dBR = static_cast<double>(pair.second->nextValue());
        if (auto mgr = AppContext::instance<LevelMeterManager>())
            if (auto vm = mgr->viewModelForTrack(track))
                vm->setLevels(dBL, dBR);
    };

    for (const auto track : appModel->tracks())
        addTrackLevels(track);

    if (notPlaying && m_masterLevelMeterValueL) {
        if (m_masterLevelMeterValueL->targetValue() > -96) {
            m_masterLevelMeterValueL->setTargetValue(-96);
            m_masterLevelMeterValueR->setTargetValue(-96);
        }
    }
    auto masterDBL = static_cast<double>(m_masterLevelMeterValueL->nextValue());
    auto masterDBR = static_cast<double>(m_masterLevelMeterValueR->nextValue());
    if (auto mgr = AppContext::instance<LevelMeterManager>())
        if (auto vm = mgr->masterViewModel())
            vm->setLevels(masterDBL, masterDBR);

    if (notPlaying) {
        bool allAtFloor = true;
        for (auto it = m_trackLevelMeterValue.constBegin(); it != m_trackLevelMeterValue.constEnd();
             ++it) {
            if (it->first->currentValue() > -96 || it->second->currentValue() > -96) {
                allAtFloor = false;
                break;
            }
        }
        if (allAtFloor && m_masterLevelMeterValueL &&
            m_masterLevelMeterValueL->currentValue() <= -96) {
            m_levelMeterActive = false;
            return;
        }
    }

    qint64 elapsed = m_levelMeterTickTime.elapsed();
    m_levelMeterTickTime.start();
    int delay = qMax(0, 8 - static_cast<int>(elapsed));
    m_levelMeterTimer->start(delay);
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

void AudioContext::handleTrackInserted(const int index, Track *track) {
    const auto trackContext = addTrack(index);
    trackContext->setData(QVariant::fromValue(track));
    m_trackModelDict.insert(track, trackContext);

    handleTrackControlChanged(track);
    for (const auto clip : track->clips()) {
        if (clip->clipType() != Clip::Audio)
            continue;
        handleClipInserted(track, clip->id(), dynamic_cast<AudioClip *>(clip));
    }

    connect(track, &Track::propertyChanged, this, [track, this] {
        DEVICE_LOCKER;
        handleTrackControlChanged(track);
    });

    connect(track, &Track::clipChanged, this,
            [track, this](const Track::ClipChangeType type, Clip *clip) {
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
    m_trackLevelMeterValue[track].first->setRampLength(m_levelMeterRampLength);
    m_trackLevelMeterValue[track].second->setRampLength(m_levelMeterRampLength);
    auto trackControlMixer = trackContext->controlMixer();
    trackControlMixer->setLevelMeterChannelCount(2);
    connect(trackControlMixer, &talcs::PositionableMixerAudioSource::levelMetered, this,
            [track, trackControlMixer, this](QVector<float> values) {
                if (!m_trackLevelMeterValue.contains(track))
                    return;
                if (masterTrackMixer()->isMutedBySoloSetting(trackControlMixer))
                    values = {0, 0};
                const auto dBL = static_cast<float>(talcs::Decibels::gainToDecibels(values[0]));
                const auto dBR = static_cast<float>(talcs::Decibels::gainToDecibels(values[1]));
                updateSmoothedValue(m_trackLevelMeterValue[track].first, dBL);
                updateSmoothedValue(m_trackLevelMeterValue[track].second, dBR);
            });

    m_trackInferDict.insert(track, new TrackInferenceHandler(trackContext, track));
}

void AudioContext::handleTrackRemoved(const int index, Track *track) {
    for (const auto clip : track->clips()) {
        if (clip->clipType() != Clip::Audio)
            continue;
        handleClipRemoved(track, clip->id(), static_cast<AudioClip *>(clip));
    }
    disconnect(track, nullptr, this, nullptr);
    removeTrack(index);
    m_trackInferDict.remove(track);
    m_trackModelDict.remove(track);
    m_trackLevelMeterValue.remove(track);
}

void AudioContext::handleTrackMoved(const int from, const int to) {
    const auto trackCount = tracks().size();
    if (from == to || from < 0 || from >= trackCount || to < 0 || to >= trackCount)
        return;

    // talcs 的 dest 是从原列表移除前的插入位置；AppModel 的 to 是最终下标。
    const auto destination = to > from ? to + 1 : to;
    talcs::DspxProjectContext::moveTrack(from, 1, destination);
}

void AudioContext::handleMasterControlChanged(const TrackControl &control) const {
    masterControlMixer()->setGain(talcs::Decibels::decibelsToGain(control.gain()));
}

void AudioContext::handleTrackControlChanged(Track *track) const {
    const auto trackContext = getContextFromTrack(track);
    trackContext->controlMixer()->setGain(talcs::Decibels::decibelsToGain(track->control().gain()));
    trackContext->controlMixer()->setPan(static_cast<float>(track->control().pan()));
    trackContext->controlMixer()->setSilentFlags(track->control().mute() ? -1 : 0);
    masterTrackMixer()->setSourceSolo(trackContext->controlMixer(), track->control().solo());
}

void AudioContext::handleClipInserted(Track *track, const int id, AudioClip *audioClip) {
    const auto trackContext = getContextFromTrack(track);
    const auto audioClipContext = trackContext->addAudioClip(id);
    m_audioClipModelDict.insert(audioClip, audioClipContext);

    handleClipPropertyChanged(audioClip);

    connect(audioClip, &Clip::propertyChanged, this, [audioClip, this] {
        DEVICE_LOCKER;
        handleClipPropertyChanged(audioClip);
    });
    connect(audioClip, &AudioClip::sourceChanged, this, [audioClip, this] {
        DEVICE_LOCKER;
        handleClipPropertyChanged(audioClip, true);
    });
    const QPointer<AudioClip> guardedAudioClip(audioClip);
    connect(
        audioClip, &AudioClip::pathStatusChanged, this,
        [guardedAudioClip, this] {
            if (!guardedAudioClip)
                return;
            DEVICE_LOCKER;
            const auto context = getContextFromAudioClip(guardedAudioClip.data());
            if (!context)
                return;
            context->controlMixer()->setSilentFlags(
                shouldSilenceAudioClip(guardedAudioClip.data()) ? -1 : 0);
        },
        Qt::QueuedConnection);
}

void AudioContext::handleClipRemoved(Track *track, const int id, AudioClip *audioClip) {
    disconnect(audioClip, nullptr, this, nullptr);
    const auto trackContext = getContextFromTrack(track);
    trackContext->removeAudioClip(id);
    m_audioClipModelDict.remove(audioClip);
    m_unloadableAudioClips.remove(audioClip);
}

void AudioContext::handleClipPropertyChanged(AudioClip *audioClip, const bool forceSourceReload) {
    const auto audioClipContext = getContextFromAudioClip(audioClip);
    if (!audioClipContext)
        return;

    feedCompensatedPosition(audioClip, audioClipContext);

    audioClipContext->controlMixer()->setGain(talcs::Decibels::decibelsToGain(audioClip->gain()));
    audioClipContext->controlMixer()->setSilentFlags(shouldSilenceAudioClip(audioClip) ? -1 : 0);

    const auto workspace = audioClip->workspace().value("diffscope.audio.formatData");
    QVariant userData;
    QDataStream o(QByteArray::fromBase64(workspace.value("userData").toString().toLatin1()));
    o >> userData;
    const auto entryClassName = workspace.value("entryClassName").toString();

    const bool shouldReloadSource =
        forceSourceReload || (!m_unloadableAudioClips.contains(audioClip) &&
                              audioClip->path() != audioClipContext->path());
    if (shouldReloadSource) {
        const QFileInfo audioFile(audioClip->path());
        if (!audioFile.isAbsolute() || !audioFile.isFile()) {
            m_unloadableAudioClips.insert(audioClip);
            audioClipContext->controlMixer()->setSilentFlags(-1);
            return;
        }
        if (!audioClipContext->setPathLoad(audioClip->path(), userData, entryClassName)) {
            m_unloadableAudioClips.insert(audioClip);
            audioClipContext->controlMixer()->setSilentFlags(-1);
            if (auto *runtime = AppContext::instance<Automation::CoreRuntime>()) {
                runtime->project().setAudioClipPathStatus(
                    {.expected = runtime->documentVersion(),
                     .source = Automation::InvocationSource::TrustedGui},
                    Automation::ClipId(audioClip->id()),
                    Automation::audioAssetSnapshotDto(*audioClip), AudioClip::PathStatus::Missing);
            }
        } else {
            m_unloadableAudioClips.remove(audioClip);
            audioClipContext->controlMixer()->setSilentFlags(shouldSilenceAudioClip(audioClip) ? -1
                                                                                               : 0);
        }
    }
}

bool AudioContext::shouldSilenceAudioClip(const AudioClip *audioClip) const {
    return !audioClip || audioClip->mute() ||
           audioClip->pathStatus() == AudioClip::PathStatus::Missing ||
           m_unloadableAudioClips.contains(audioClip);
}

void AudioContext::feedCompensatedPosition(const AudioClip *audioClip,
                                           talcs::DspxAudioClipContext *audioClipContext) {
    // talcs converts (clipStart, start + clipStart, start + clipStart + clipLen)
    // through the project tempo map; under multi-tempo the raw model ticks would
    // shift the material read offset. Feed a compensation triplet instead:
    //   clipStart' = f⁻¹(trim)  — the tick whose absolute conversion equals the
    //                             material trim offset
    //   start'     = P − clipStart'  (never converted alone by talcs; may be
    //                                 negative, stored without validation)
    //   clipLen'   = f⁻¹(f(P) + playLength) − P
    // where P is the visible start tick and f the absolute tick→time mapping.
    // NOTE: this compensation must never be applied to singing clips —
    // DspxNoteContext converts the singing clip's start tick ALONE.
    const auto &timeline = appModel->timeline();
    const int visibleStart = audioClip->start() + audioClip->clipStart();
    if (!audioClip->hasRealTimeAnchor()) {
        // Truth not established yet; raw ticks are still correct in this state
        audioClipContext->setStart(audioClip->start());
        audioClipContext->setClipStart(audioClip->clipStart());
        audioClipContext->setClipLen(audioClip->clipLen());
        audioClipContext->updatePosition();
        return;
    }
    const int compClipStart = qMax(0, qRound(timeline.msToTick(audioClip->trimStartMs())));
    const int compStart = visibleStart - compClipStart;
    const double visibleMs = timeline.tickToMs(visibleStart);
    const int compClipLen =
        qMax(1, qRound(timeline.msToTick(visibleMs + audioClip->playLengthMs())) - visibleStart);
    audioClipContext->setStart(compStart);
    audioClipContext->setClipStart(compClipStart);
    audioClipContext->setClipLen(compClipLen);
    // The setters no-op on equal ticks even though the tempo map may have
    // changed; recompute unconditionally
    audioClipContext->updatePosition();
}

void AudioContext::handleTimeChanged() const {
    for (int i = 0; i < 4; i++)
        PseudoSingerConfigNotifier::notify(i);
    // The compensation triplet depends on the tempo map, so re-derive it
    // instead of just re-converting the stale ticks stored inside talcs
    for (auto it = m_audioClipModelDict.constBegin(); it != m_audioClipModelDict.constEnd(); ++it)
        feedCompensatedPosition(it.key(), it.value());
}

void AudioContext::updateLoopingRange() const {
    const auto &settings = appStatus->loopSettings.get();
    if (settings.enabled)
        transport()->setLoopingRange(tickToSample(settings.start), tickToSample(settings.end()));
    else
        transport()->setLoopingRange(-1, -1);
}

void AudioContext::updateSmoothedValue(std::shared_ptr<talcs::SmoothedFloat> &sm, float dBL) {
    if (dBL < sm->currentValue())
        sm->setTargetValue(dBL);
    else
        sm->setCurrentAndTargetValue(dBL);
}

bool AudioContext::willStartCallback(AudioExporter *exporter) {
    m_exporter = exporter;
    playbackController->stop();
    setBufferingReadAheadSize(0);
    emit exporterCausedTimeChanged();
    for (const auto trackInferenceHandler : m_trackInferDict.values()) {
        trackInferenceHandler->setMode(talcs::DspxTrackInferenceContext::Export);
    }
    const bool isOK = exportInferenceStatus() != ExportInferenceStatus::Failed;
    if (!isOK)
        exporter->cancel(true, tr("Inference failed"));
    return isOK;
}

void AudioContext::willFinishCallback(AudioExporter *exporter) {
    setBufferingReadAheadSize(AudioSystem::outputSystem()->fileBufferingReadAheadSize());
    emit exporterCausedTimeChanged();
    for (const auto trackInferenceHandler : m_trackInferDict.values()) {
        trackInferenceHandler->setMode(talcs::DspxTrackInferenceContext::Default);
    }
    m_exporter = nullptr;
}
