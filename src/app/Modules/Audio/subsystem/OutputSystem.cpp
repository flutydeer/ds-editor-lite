#include "OutputSystem.h"

#include <QDebug>

#include <TalcsCore/MixerAudioSource.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsDevice/AudioDriver.h>
#include <TalcsDevice/AudioDriverManager.h>
#include <TalcsDevice/AudioSourcePlayback.h>

#include <Model/AppOptions/AppOptions.h>

OutputSystem::OutputSystem(QObject *parent) : AbstractOutputSystem(parent), m_outputContext(new talcs::OutputContext) {
    setContext(m_outputContext.get());
}

OutputSystem::~OutputSystem() = default;

bool OutputSystem::initialize() {
    m_outputContext->setAdoptedBufferSize(appOptions->audio()->adoptedBufferSize);
    m_outputContext->setAdoptedSampleRate(appOptions->audio()->adoptedSampleRate);
    m_outputContext->controlMixer()->setGain(appOptions->audio()->deviceGain);
    m_outputContext->controlMixer()->setPan(appOptions->audio()->devicePan);
    m_outputContext->setHotPlugNotificationMode(appOptions->audio()->hotPlugNotificationMode);
    setFileBufferingReadAheadSize(appOptions->audio()->fileBufferingReadAheadSize);

    if (m_outputContext->initialize(appOptions->audio()->driverName, appOptions->audio()->deviceName)) {
        qDebug() << "Audio::OutputSystem: device initialized"
                 << m_outputContext->device()->name()
                 << m_outputContext->driver()->name()
                 << m_outputContext->adoptedBufferSize()
                 << m_outputContext->adoptedSampleRate();
        return true;
    } else {
        qWarning() << "Audio::OutputSystem: fatal: cannot initialize";
        return false;
    }

}
bool OutputSystem::setDriver(const QString &driverName) {
    if (m_outputContext->setDriver(driverName)) {
        postSetDevice();
        qDebug() << "Audio::OutputSystem: driver changed"
                 << m_outputContext->device()->name()
                 << m_outputContext->driver()->name()
                 << m_outputContext->adoptedBufferSize()
                 << m_outputContext->adoptedSampleRate();
        return true;
    } else {
        qWarning() << "Audio::OutputSystem: fatal: cannot set driver" << driverName;
        return false;
    }
}
bool OutputSystem::setDevice(const QString &deviceName) {
    if (m_outputContext->setDevice(deviceName)) {
        postSetDevice();
        qDebug() << "Audio::OutputSystem: device changed"
                 << m_outputContext->device()->name()
                 << m_outputContext->driver()->name()
                 << m_outputContext->adoptedBufferSize()
                 << m_outputContext->adoptedSampleRate();
        return true;
    } else {
        qWarning() << "Audio::OutputSystem: fatal: cannot set device"
                   << (m_outputContext->driver() ? m_outputContext->driver()->name() : "")
                   << deviceName;
        return false;
    }
}

bool OutputSystem::setAdoptedBufferSize(qint64 bufferSize) {
    appOptions->audio()->adoptedBufferSize = bufferSize;
    if (m_outputContext->setAdoptedBufferSize(bufferSize)) {
        qDebug() << "Audio::OutputSystem: buffer size changed"
                 << m_outputContext->device()->name()
                 << m_outputContext->driver()->name()
                 << m_outputContext->adoptedBufferSize()
                 << m_outputContext->adoptedSampleRate();
        return true;
    } else {
        qWarning() << "Audio::OutputSystem: fatal: cannot set buffer size"
                   << (m_outputContext->driver() ? m_outputContext->driver()->name() : "")
                   << (m_outputContext->device() ? m_outputContext->device()->name() : "")
                   << m_outputContext->adoptedBufferSize()
                   << m_outputContext->adoptedSampleRate();
        return false;
    }
}

bool OutputSystem::setAdoptedSampleRate(double sampleRate) {
    appOptions->audio()->adoptedSampleRate = sampleRate;
    if (m_outputContext->setAdoptedSampleRate(sampleRate)) {
        qDebug() << "Audio::OutputSystem: sample rate changed"
                 << m_outputContext->device()->name()
                 << m_outputContext->driver()->name()
                 << m_outputContext->adoptedSampleRate()
                 << m_outputContext->adoptedSampleRate();
        return true;
    } else {
        qWarning() << "Audio::OutputSystem: fatal: cannot set sample rate"
                   << (m_outputContext->driver() ? m_outputContext->driver()->name() : "")
                   << (m_outputContext->device() ? m_outputContext->device()->name() : "")
                   << m_outputContext->adoptedSampleRate()
                   << m_outputContext->adoptedSampleRate();
        return false;
    }
}

void OutputSystem::setHotPlugNotificationMode(talcs::OutputContext::HotPlugNotificationMode mode) {
    appOptions->audio()->hotPlugNotificationMode = mode;
    m_outputContext->setHotPlugNotificationMode(mode);
    qDebug() << "Audio::OutputSystem: hot plug notification mode set to" << mode;
}

void OutputSystem::postSetDevice() const {
    appOptions->audio()->driverName = m_outputContext->driver()->name();
    appOptions->audio()->deviceName = m_outputContext->device()->name();
    appOptions->audio()->adoptedSampleRate = m_outputContext->adoptedSampleRate();
    appOptions->audio()->adoptedBufferSize = m_outputContext->adoptedBufferSize();
}