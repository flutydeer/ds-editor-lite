#include "AudioSystem.h"

#include <TalcsCore/TransportAudioSource.h>
#include <TalcsCore/PositionableMixerAudioSource.h>
#include <TalcsDevice/AudioDriverManager.h>
#include <TalcsDevice/AudioDriver.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsDevice/AudioSourcePlayback.h>

static AudioSystem *m_instance = nullptr;

AudioSystem::AudioSystem(QObject *parent) : QObject(parent) {
    m_drvMgr = talcs::AudioDriverManager::createBuiltInDriverManager(this);
    m_masterTrack = new talcs::PositionableMixerAudioSource;
    m_tpSrc = new talcs::TransportAudioSource(m_masterTrack, true);
    m_playback.reset(new talcs::AudioSourcePlayback(m_tpSrc, true, false));
    m_instance = this;
}

AudioSystem::~AudioSystem() {
    m_dev.reset();
}

AudioSystem *AudioSystem::instance() {
    return m_instance;
}

bool AudioSystem::findProperDevice() {
    for (int i = -1;; i++) {
        std::unique_ptr<talcs::AudioDevice> dev;
        if (i >= m_drv->devices().size())
            return false;
        if (i == -1) {
            if (!m_drv->defaultDevice().isEmpty())
                dev.reset(m_drv->createDevice(m_drv->defaultDevice()));
        } else {
            dev.reset(m_drv->createDevice(m_drv->devices()[i]));
        }
        if (!dev || !dev->isInitialized())
            continue;
        if (!dev->open(dev->preferredBufferSize(), dev->preferredSampleRate()))
            continue;
        m_dev = std::move(dev);
        break;
    }
    return true;
}

bool AudioSystem::initialize(bool isVstMode) {
    if (isVstMode) {
        return false;
    } else {
        for (int i = 0;; i++) {
            if (i >= m_drvMgr->drivers().size()) {
                m_drv = nullptr;
                return false;
            }
            m_drv = m_drvMgr->driver(m_drvMgr->drivers()[i]);
            if (m_drv->initialize())
                break;
        }
        return findProperDevice();
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

talcs::TransportAudioSource *AudioSystem::transport() const {
    return m_tpSrc;
}

bool AudioSystem::checkStatus() const {
    return m_drv && m_dev && m_dev->isOpen();
}

bool AudioSystem::setDriver(const QString &driverName) {
    if (driverName == m_drv->name())
        return true;
    m_dev.reset();
    m_drv = m_drvMgr->driver(driverName);
    if (!m_drv->initialize())
        return false;
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
    return true;
}

void AudioSystem::openAudioSettings() {
    if (m_dev)
        m_dev->stop();

}
