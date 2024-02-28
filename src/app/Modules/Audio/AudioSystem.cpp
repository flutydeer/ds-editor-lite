#include "AudioSystem.h"

#include <QApplication>
#include <QTimer>
#include <QPushButton>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>

#include <TalcsCore/TransportAudioSource.h>
#include <TalcsCore/PositionableMixerAudioSource.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsCore/AudioSourceClipSeries.h>
#include <TalcsDevice/AudioDriverManager.h>
#include <TalcsDevice/AudioDriver.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsDevice/AudioSourcePlayback.h>
#include <TalcsRemote/RemoteSocket.h>
#include <TalcsRemote/RemoteAudioDevice.h>
#include <TalcsRemote/RemoteEditor.h>
#include <TalcsRemote/RemoteTransportControllerInterface.h>
#include <TalcsRemote/TransportAudioSourceProcessInfoCallback.h>
#include <TalcsMidi/MidiSineWaveSynthesizer.h>


#include "UI/Dialogs/Audio/AudioSettingsDialog.h"
#include "Modules/Audio/AudioContext.h"
#include "UI/Dialogs/Base/MessageDialog.h"

static AudioSystem *m_instance = nullptr;

AudioSystem::AudioSystem(QObject *parent) : QObject(parent) {
    m_instance = this;
    m_drvMgr = talcs::AudioDriverManager::createBuiltInDriverManager(this);
    m_masterTrack = new talcs::PositionableMixerAudioSource;
    m_tpSrc = new talcs::TransportAudioSource(m_masterTrack, true);
    m_preMixer = new talcs::MixerAudioSource;
    m_preMixer->addSource(m_tpSrc, true);
    m_playback.reset(new talcs::AudioSourcePlayback(m_preMixer, true, false));
    m_audioContext = new AudioContext(this);

    m_masterTrack->addSource(new talcs::AudioSourceClipSeries, true); // TODO dummy source to fill length
}

AudioSystem::~AudioSystem() {
    m_dev.reset();
    m_instance = nullptr;
}

AudioSystem *AudioSystem::instance() {
    return m_instance;
}

bool AudioSystem::findProperDriver() {
    for (int i = -1;; i++) {
        if (i >= m_drvMgr->drivers().size()) {
            m_drv = nullptr;
            return false;
        }
        if (i == -1) {
            auto savedDrvName = m_settings.value("audio/driverName").toString();
            if (savedDrvName.isEmpty())
                continue;
            m_drv = m_drvMgr->driver(savedDrvName);
        } else {
            m_drv = m_drvMgr->driver(m_drvMgr->drivers()[i]);
        }
        if (m_drv && m_drv->initialize()) {
            connect(m_drv, &talcs::AudioDriver::deviceChanged, this, &AudioSystem::handleDeviceHotPlug);
            return true;
        }
    }
}

bool AudioSystem::findProperDevice() {
    for (int i = -2;; i++) {
        std::unique_ptr<talcs::AudioDevice> dev;
        if (i >= m_drv->devices().size())
            return false;
        if (i == -2) {
            auto savedDeviceName = m_settings.value("audio/deviceName").toString();
            if (!savedDeviceName.isEmpty())
                dev.reset(m_drv->createDevice(savedDeviceName));
        } else if (i == -1) {
            if (!m_drv->defaultDevice().isEmpty())
                dev.reset(m_drv->createDevice(m_drv->defaultDevice()));
        } else {
            dev.reset(m_drv->createDevice(m_drv->devices()[i]));
        }
        if (!dev || !dev->isInitialized())
            continue;
        if (i == -2) {
            auto savedBufferSize = m_settings.value("audio/adoptedBufferSize", dev->preferredBufferSize()).value<qint64>();
            auto savedSampleRate = m_settings.value("audio/adoptedSampleRate", dev->preferredSampleRate()).value<double>();
            if (!dev->open(savedBufferSize, savedSampleRate))
                if (!dev->open(dev->preferredBufferSize(), dev->preferredSampleRate()))
                    continue;
        } else {
            if (!dev->open(dev->preferredBufferSize(), dev->preferredSampleRate()))
                continue;
        }

        m_dev = std::move(dev);
        postSetDevice();
        return true;
    }
}

void AudioSystem::VstProcessInfoCallback::onThisBlockProcessInfo(const talcs::RemoteAudioDevice::ProcessInfo &processInfo) {
    if (processInfo.status == talcs::RemoteAudioDevice::ProcessInfo::NotPlaying) {
        if (AudioSystem::instance()->transport()->isPlaying() && !m_isPaused)
            PlaybackController::instance()->stop();
        m_isPaused = true;
    } else {
        if (!AudioSystem::instance()->transport()->isPlaying())
            PlaybackController::instance()->play();
        m_isPaused = false;
        if (AudioSystem::instance()->transport()->position() != processInfo.position)
            AudioSystem::instance()->m_audioContext->handleVstCallbackPositionChange(processInfo.position);
    }
}

bool AudioSystem::initialize(bool isVstMode) {
    auto [vstEditorPort, vstPluginPort] = checkVstConfig();
    if (isVstMode) {
        m_socket = new talcs::RemoteSocket(vstEditorPort, vstPluginPort, this);
        if (!m_socket->startServer(QThread::idealThreadCount()))
            return false;
        m_remoteDev = new talcs::RemoteAudioDevice(m_socket, tr("Host Audio"));
        m_remoteTpCb.reset(new VstProcessInfoCallback);
        m_remoteDev->addProcessInfoCallback(m_remoteTpCb.get());
        // TODO remote editor

        connect(m_remoteDev, &talcs::RemoteAudioDevice::remoteOpened, this, [=](qint64 bufferSize, double sampleRate) {
            m_preMixer->open(bufferSize, sampleRate);
            m_remoteDev->open(bufferSize, sampleRate);
            m_adoptedBufferSize = bufferSize;
            m_adoptedSampleRate = sampleRate;
        });

        m_preMixer->open(1024, 48000); // dummy

        if (!m_socket->startClient())
            return false;
        m_dev.reset(m_remoteDev);
        return true;
    } else {
        if (!findProperDriver())
            return false;
        if (!findProperDevice())
            return false;
        return true;
    }
}
QPair<quint16, quint16> AudioSystem::checkVstConfig() {
    auto vstConfigFilename = QStandardPaths::locate(QStandardPaths::AppDataLocation, "vstconfig.json");
    if (vstConfigFilename.isEmpty()) {
        QDir configDir(QStandardPaths::standardLocations(QStandardPaths::AppDataLocation)[0]);
        if (!configDir.exists())
            configDir.mkpath(configDir.absolutePath());
        QFile configFile(configDir.absoluteFilePath("vstconfig.json"));
        configFile.open(QFile::WriteOnly);
        configFile.write(QJsonDocument({
                                           {"editor",      QApplication::applicationFilePath()},
                                           {"pluginPort",  28082                             },
                                           {"editorPort",  28081                             },
                                           {"threadCount", 2                                 },
        })
                             .toJson());
        return {28081, 28082};
    } else {
        QFile configFile(vstConfigFilename);
        configFile.open(QFile::ReadOnly);
        auto configDoc = QJsonDocument::fromJson(configFile.readAll());
        return {configDoc["editorPort"].toVariant().value<quint16>(), configDoc["pluginPort"].toVariant().value<quint16>()};
    }
}

talcs::AudioDriverManager *AudioSystem::driverManager() const {
    return m_drvMgr;
}

talcs::AudioDevice *AudioSystem::device() const {
    return m_dev.get();
}

talcs::AudioDriver *AudioSystem::driver() const {
    return m_drv;
}

talcs::AudioSourcePlayback *AudioSystem::playback() const {
    return m_playback.get();
}
talcs::MixerAudioSource *AudioSystem::preMixer() const {
    return m_preMixer;
}
talcs::PositionableMixerAudioSource *AudioSystem::masterTrack() const {
    return m_masterTrack;
}

talcs::TransportAudioSource *AudioSystem::transport() const {
    return m_tpSrc;
}

talcs::RemoteSocket *AudioSystem::socket() const {
    return m_socket;
}

bool AudioSystem::isDeviceAutoClosed() const {
    return m_isDeviceAutoClosed;
}

bool AudioSystem::checkStatus() const {
    return m_dev && m_dev->isOpen();
}

bool AudioSystem::setDriver(const QString &driverName) {
    if (driverName == m_drv->name())
        return true;
    m_dev.reset();
    if (m_drv) {
        m_drv->finalize();
        disconnect(m_drv, nullptr, this, nullptr);
    }
    m_drv = m_drvMgr->driver(driverName);
    if (!m_drv->initialize())
        return false;
    connect(m_drv, &talcs::AudioDriver::deviceChanged, this, &AudioSystem::handleDeviceHotPlug);
    return findProperDevice();
}

bool AudioSystem::setDevice(const QString &deviceName) {
    if (!m_drv)
        return false;
    m_dev.reset(m_drv->createDevice(deviceName));
    if (!m_dev || !m_dev->isInitialized())
        return false;
    if (!m_dev->open(m_dev->preferredBufferSize(), m_dev->preferredSampleRate()))
        return false;
    postSetDevice();
    connect(m_dev.get(), &talcs::AudioDevice::closed, this, [=] {
        m_isDeviceAutoClosed = true;
    });
    return true;
}
void AudioSystem::postSetDevice() {
    m_adoptedBufferSize = m_dev->bufferSize();
    if (!qFuzzyCompare(m_adoptedBufferSize, m_dev->sampleRate())) {
        m_adoptedSampleRate = m_dev->sampleRate();
        m_audioContext->rebuildAllClips();
        m_audioContext->handleFileBufferingSizeChange();
    }
    m_isDeviceAutoClosed = false;
    m_preMixer->open(m_adoptedBufferSize, m_adoptedSampleRate);
    m_audioContext->handleDeviceChangeDuringPlayback();
    m_settings.setValue("audio/driverName", m_drv->name());
    m_settings.setValue("audio/deviceName", m_dev->name());
    m_settings.setValue("audio/adoptedBufferSize", m_adoptedBufferSize);
    m_settings.setValue("audio/adoptedSampleRate", m_adoptedSampleRate);
}

qint64 AudioSystem::adoptedBufferSize() const {
    return m_adoptedBufferSize;
}
void AudioSystem::setAdoptedBufferSize(qint64 bufferSize) {
    if (device()->isOpen()) {
        device()->open(bufferSize, device()->sampleRate());
    }
    m_adoptedBufferSize = bufferSize;
    m_settings.setValue("audio/adoptedBufferSize", m_adoptedBufferSize);
    if (m_adoptedBufferSize && m_adoptedSampleRate)
        m_preMixer->open(m_adoptedBufferSize, m_adoptedSampleRate);
    m_audioContext->handleDeviceChangeDuringPlayback();
}
double AudioSystem::adoptedSampleRate() const {
    return m_adoptedSampleRate;
}
void AudioSystem::setAdoptedSampleRate(double sampleRate) {
    if (sampleRate == m_adoptedSampleRate)
        return;
    if (device()->isOpen()) {
        device()->open(device()->bufferSize(), sampleRate);
    }
    m_adoptedSampleRate = sampleRate;
    m_settings.setValue("audio/adoptedSampleRate", m_adoptedSampleRate);
    if (m_adoptedBufferSize && m_adoptedSampleRate)
        m_preMixer->open(m_adoptedBufferSize, m_adoptedSampleRate);
    m_audioContext->rebuildAllClips();
    m_audioContext->handleFileBufferingSizeChange();
    m_audioContext->handleDeviceChangeDuringPlayback();
}

void AudioSystem::testDevice() {
    if (!m_dev)
        return;
    auto testSynth = new talcs::MidiSineWaveSynthesizer;
    m_preMixer->addSource(testSynth);
    if (!m_dev->isOpen()) {
        m_dev->open(m_adoptedBufferSize, m_adoptedSampleRate);
    }
    if (!m_dev->isStarted())
        m_dev->start(m_playback.get());
    testSynth->workCallback(talcs::MidiMessage::noteOn(1, 69, float(0.5)));
    QTimer::singleShot(1000, [=] {
        testSynth->workCallback(talcs::MidiMessage::noteOff(1, 69));
        QTimer::singleShot(100, [=] {
            m_preMixer->removeSource(testSynth);
            delete testSynth;
        });
    });
}

void AudioSystem::handleDeviceHotPlug() {
    auto hotPlugMode = m_settings.value("audio/hotPlugMode", NotifyOnAnyChange).value<HotPlugMode>();
    MessageDialog msgBox;
    msgBox.setText(tr("Audio device change is detected."));
    msgBox.setIcon(QMessageBox::Information);
    msgBox.addButton(QMessageBox::Ok);
    auto openAudioSettingsButton = new QPushButton(tr("Go to audio settings"));
    msgBox.addButton(openAudioSettingsButton, QMessageBox::NoRole);
    connect(openAudioSettingsButton, &QPushButton::clicked, this, [=] {
        AudioSettingsDialog dlg;
        dlg.exec();
    });
    switch(hotPlugMode) {
        case NotifyOnAnyChange:
            msgBox.exec();
            break;
        case NotifyOnCurrentRemoval:
            if (m_dev && !m_drv->devices().contains(m_dev->name()))
                msgBox.exec();
            break;
        case None:
            break;
    }
}

AudioContext *AudioSystem::audioContext() const {
    return m_audioContext;
}

QString AudioSystem::driverDisplayName(const QString &driverName) {
    return driverName;
}
