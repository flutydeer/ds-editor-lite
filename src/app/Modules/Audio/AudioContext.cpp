#include "AudioContext.h"

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

#include <Modules/Audio/AudioSystem.h>
#include <Modules/Audio/subsystem/AbstractOutputSystem.h>
#include <Modules/Audio/AudioSettings.h>

#include "Model/Track.h"
#include <Model/AppOptions/AppOptions.h>

#define DEVICE_LOCKER talcs::AudioDeviceLocker locker(AudioSystem::sessionOutputSystem()->context()->device())

class AudioFormatIOObject : public QObject, public talcs::AudioFormatIO {
public:
    explicit AudioFormatIOObject(QIODevice *stream = nullptr, QObject *parent = nullptr) : QObject(parent), talcs::AudioFormatIO(stream) {

    }
    ~AudioFormatIOObject() override = default;
};

class BuiltInFormatEntry : public talcs::FormatEntry {
public:
    explicit BuiltInFormatEntry(QObject *parent = nullptr) : FormatEntry(parent) {
        std::set<QString> extensionHintSet;
        for (const auto &fmtInfo : talcs::AudioFormatIO::availableFormats()) {
            QStringList fmtExtensions;
            fmtExtensions.append(fmtInfo.extension);
            if (fmtInfo.extension == "raw") {
                for (const auto &subtypeInfo : fmtInfo.subtypes)
                    m_rawSubtypes.append({subtypeInfo.name, subtypeInfo.subtype});
            }
            for (const auto &subtypeInfo : fmtInfo.subtypes) {
                fmtExtensions += subtypeInfo.extensions;
            }
            extensionHintSet.insert(fmtExtensions.cbegin(), fmtExtensions.cend());
            std::transform(fmtExtensions.cbegin(), fmtExtensions.cend(), fmtExtensions.begin(), [](const QString &extension) {
                return "*." + extension;
            });
            m_filters.append(QString("%1 (%2)").arg(fmtInfo.name, fmtExtensions.join(" ")));
        }
        m_extensionHints = QStringList(extensionHintSet.cbegin(), extensionHintSet.cend());
    }

    ~BuiltInFormatEntry() override = default;

    QStringList filters() const override {
        return m_filters;
    }
    QStringList extensionHints() const override {
        return m_extensionHints;
    }

    talcs::AbstractAudioFormatIO *getFormatOpen(const QString &filename, QVariant &userData, QWidget *win) {
        std::unique_ptr<AudioFormatIOObject> io = std::make_unique<AudioFormatIOObject>();
        std::unique_ptr<QFile> f = std::make_unique<QFile>(filename, io.get());
        if (!f->open(QIODevice::ReadOnly))
            return nullptr;
        io->setStream(f.release());
        if (filename.endsWith(".raw")) {
            QDialog dlg(win);
            auto mainLayout = new QVBoxLayout;
            auto optionsLayout = new QFormLayout;

            auto subtypeComboBox = new QComboBox;
            for (const auto &[name, subtype] : m_rawSubtypes) {
                subtypeComboBox->addItem(name, subtype);
            }
            optionsLayout->addRow(tr("Option"), subtypeComboBox);

            auto channelCountSpinBox = new QSpinBox;
            channelCountSpinBox->setMinimum(1);
            optionsLayout->addRow(tr("Channel"), channelCountSpinBox);

            auto sampleRateComboBox = new QComboBox;
            sampleRateComboBox->addItems({"8000", "11025", "12000", "16000", "22050", "24000",
                                          "32000", "44100", "48000", "64000", "88200", "96000",
                                          "128000", "176400", "192000", "256000", "352800",
                                          "384000"});
            sampleRateComboBox->setEditable(true);
            sampleRateComboBox->setValidator(new QDoubleValidator(0.01, std::numeric_limits<double>::max(), 2));
            optionsLayout->addRow(tr("Sample rate"), sampleRateComboBox);

            auto byteOrderComboBox = new QComboBox;
            byteOrderComboBox->addItem(tr("System"), talcs::AudioFormatIO::SystemOrder);
            byteOrderComboBox->addItem(tr("Little-endian"), talcs::AudioFormatIO::LittleEndian);
            byteOrderComboBox->addItem(tr("Big-endian"), talcs::AudioFormatIO::BigEndian);
            optionsLayout->addRow(tr("Byte order"), byteOrderComboBox);

            mainLayout->addLayout(optionsLayout);

            auto buttonLayout = new QHBoxLayout;
            buttonLayout->addStretch();
            auto okButton = new QPushButton(tr("OK"));
            buttonLayout->addWidget(okButton);
            connect(okButton, &QAbstractButton::clicked, &dlg, &QDialog::accept);
            mainLayout->addLayout(buttonLayout);

            dlg.setLayout(mainLayout);
            dlg.setWindowTitle(tr("Configure Raw Data"));
            dlg.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
            if (dlg.exec() != QDialog::Accepted)
                return nullptr;
            userData = QVariantMap({
                {"subtype", subtypeComboBox->currentData()},
                {"channelCount", channelCountSpinBox->value()},
                {"sampleRate", QLocale().toDouble(sampleRateComboBox->currentText())},
                {"byteOrder", byteOrderComboBox->currentData()}
            });
            io->setFormat(talcs::AudioFormatIO::RAW | subtypeComboBox->currentData().toInt() | byteOrderComboBox->currentData().toInt());
            io->setChannelCount(channelCountSpinBox->value());
            io->setSampleRate(QLocale().toDouble(sampleRateComboBox->currentText()));

        }
        if (!io->open(talcs::AbstractAudioFormatIO::Read))
            return nullptr;
        io->close();
        return io.release();
    }

    talcs::AbstractAudioFormatIO *getFormatLoad(const QString &filename, const QVariant &userData) override {
        std::unique_ptr<AudioFormatIOObject> io = std::make_unique<AudioFormatIOObject>();
        std::unique_ptr<QFile> f = std::make_unique<QFile>(filename, io.get());
        if (!f->open(QIODevice::ReadOnly))
            return nullptr;
        io->setStream(f.release());
        if (filename.endsWith(".raw")) {
            auto rawOptions = userData.toMap();
            io->setFormat(talcs::AudioFormatIO::RAW | rawOptions.value("subtype").toInt() | rawOptions.value("byteOrder").toInt());
            io->setChannelCount(rawOptions.value("channelCount").toInt());
            io->setSampleRate(rawOptions.value("sampleRate").toDouble());
        }
        if (!io->open(talcs::AbstractAudioFormatIO::Read))
            return nullptr;
        io->close();
        return io.release();
    }

private:
    QStringList m_filters;
    QStringList m_extensionHints;
    QList<QPair<QString, int>> m_rawSubtypes;
};

static AudioContext *m_instance = nullptr;

static qint64 tickToSample(double tick) {
    auto dev = AudioSystem::sessionOutputSystem()->context()->device();
    auto sr = dev ? !qFuzzyIsNull(dev->sampleRate()) ? dev->sampleRate() : 48000.0 : 48000.0;
    return qint64(tick * 60.0 * sr / playbackController->tempo() / 480.0);
}

static double sampleToTick(qint64 sample) {
    auto dev = AudioSystem::sessionOutputSystem()->context()->device();
    auto sr = dev ? !qFuzzyIsNull(dev->sampleRate()) ? dev->sampleRate() : 48000.0 : 48000.0;
    return double(sample) / sr * playbackController->tempo() / 60.0 * 480.0;
}

AudioContext::AudioContext(QObject *parent) : DspxProjectContext(parent) {
    m_instance = this;

    AudioSystem::sessionOutputSystem()->context()->preMixer()->addSource(preMixer());

    setTimeConverter(&tickToSample);

    auto formatManager = new talcs::FormatManager(this);
    formatManager->addEntry(new BuiltInFormatEntry);

    setFormatManager(formatManager);

    setBufferingReadAheadSize(AudioSettings::fileBufferingReadAheadSize());

    connect(transport(), &talcs::TransportAudioSource::positionAboutToChange, this, [=](qint64 positionSample) {
        m_transportPositionFlag = false;
        playbackController->setPosition(sampleToTick(positionSample));
        m_transportPositionFlag = true;
    });

    connect(transport(), &talcs::TransportAudioSource::playbackStatusChanged, this, [=](auto status) {
        if (status == talcs::TransportAudioSource::Paused && playbackController->playbackStatus() == PlaybackGlobal::Stopped)
            playbackController->setPosition(playbackController->lastPosition());
    });

    connect(playbackController, &PlaybackController::playbackStatusChanged, this, &AudioContext::handlePlaybackStatusChanged);
    connect(playbackController, &PlaybackController::positionChanged, this, &AudioContext::handlePlaybackPositionChanged);

    connect(appModel, &AppModel::modelChanged, this, [this] {
        DEVICE_LOCKER;
        handleModelChanged();
    });
    connect(appModel, &AppModel::trackChanged, this, [=](AppModel::TrackChangeType type, int index, Track *track) {
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
    });

    connect(AudioSystem::sessionOutputSystem()->context(), &talcs::AbstractOutputContext::bufferSizeChanged, this, [=] {
        playbackController->stop();
    });

    connect(AudioSystem::sessionOutputSystem()->context(), &talcs::AbstractOutputContext::sampleRateChanged, this, [=] {
        DEVICE_LOCKER;
        handleTimeChanged();
        playbackController->stop();
    });

    connect(AudioSystem::sessionOutputSystem()->context(), &talcs::AbstractOutputContext::deviceChanged, this, [=] {
        playbackController->stop();
    });

    m_levelMeterTimer = new QTimer(this);
    m_levelMeterTimer->setInterval(50);  // TODO make it configurable
    connect(m_levelMeterTimer, &QTimer::timeout, this, [=] {
        AppModel::LevelMetersUpdatedArgs args;
        for (auto track : appModel->tracks()) {
            if (!m_trackLevelMeterValue.contains(track))
                continue;
            if (playbackController->playbackStatus() != Playing &&
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
    m_instance = nullptr;
}

AudioContext *AudioContext::instance() {
    return m_instance;
}

Track *AudioContext::getTrackFromContext(talcs::DspxTrackContext *trackContext) const {
    Q_UNUSED(this)
    return trackContext->data().value<Track *>();
}
AudioClip *AudioContext::getAudioClipFromContext(talcs::DspxAudioClipContext *audioClipContext) const {
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

void AudioContext::handlePlaybackStatusChanged(PlaybackStatus status) {
    auto device = AudioSystem::sessionOutputSystem()->context()->device();
    switch (status) {
        case Stopped:
            transport()->pause();
            break;
        case Playing:
            if (!device || !device->isOpen())
                QMessageBox::critical(nullptr, {}, tr("Cannot open audio device!"));
            if (device && !device->isStarted())
                device->start(AudioSystem::sessionOutputSystem()->context()->playback());
            transport()->play();
            break;
        case Paused:
            transport()->pause();
            break;
    }
}
void AudioContext::handlePlaybackPositionChanged(double positionTick) {
    if (m_transportPositionFlag)
        transport()->setPosition(tickToSample(positionTick));
}

void AudioContext::handleModelChanged() {
    auto oldTrackContexts = tracks();
    for (int i = static_cast<int>(oldTrackContexts.size()) -1; i >= 0; i--) {
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
        switch(type) {
            case Track::Inserted:
                handleClipInserted(track, clip->id(), static_cast<AudioClip *>(clip));
                break;
            case Track::Removed:
                handleClipRemoved(track, clip->id(), static_cast<AudioClip *>(clip));
                break;
        }
    });

    m_trackLevelMeterValue[track] = {std::make_shared<talcs::SmoothedFloat>(-96), std::make_shared<talcs::SmoothedFloat>(-96)};
    m_trackLevelMeterValue[track].first->setRampLength(8); // TODO make it configurable
    m_trackLevelMeterValue[track].second->setRampLength(8);
    trackContext->controlMixer()->setLevelMeterChannelCount(2);
    connect(trackContext->controlMixer(), &talcs::PositionableMixerAudioSource::levelMetered, this, [=](QVector<float> values) {
        if (!m_trackLevelMeterValue.contains(track))
            return;
        if (masterTrackMixer()->isMutedBySoloSetting(trackContext->controlMixer()))
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
}
void AudioContext::handleTrackRemoved(int index, Track *track) {
    for (auto clip : track->clips()) {
        if (clip->clipType() != Clip::Audio)
            continue;
        handleClipRemoved(track, clip->id(), static_cast<AudioClip *>(clip));
    }
    removeTrack(index);
    m_trackModelDict.remove(track);
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

    if (audioClip->path() != audioClipContext->path())
        audioClipContext->setPathLoad(audioClip->path());
}

void AudioContext::handleTimeChanged() {
    for (auto trackContext : tracks()) {
        for (auto audioClipContext : trackContext->clips()) {
            audioClipContext->updatePosition();
        }
    }
}
