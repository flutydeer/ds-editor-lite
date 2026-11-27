#include "OutputSystem.h"

#include <TalcsCore/MixerAudioSource.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsDevice/AudioDriver.h>
#include <TalcsDevice/AudioDriverManager.h>
#ifdef LITE_HAVE_TALCS_ASIO
#    include <TalcsAsio/ASIOAudioDriver.h>
#endif

#include <Model/AppOptions/AppOptions.h>
#include <Modules/Audio/AudioSettings.h>

OutputSystem::OutputSystem(QObject *parent)
    : AbstractOutputSystem(parent), m_outputContext(new talcs::OutputContext) {
    setContext(m_outputContext.get());
#ifdef LITE_HAVE_TALCS_ASIO
    m_outputContext->driverManager()->addAudioDriver(new talcs::ASIOAudioDriver);
#endif
}

OutputSystem::~OutputSystem() = default;

bool OutputSystem::initialize() {
    m_outputContext->setAdoptedBufferSize(AudioSettings::adoptedBufferSize());
    m_outputContext->setAdoptedSampleRate(AudioSettings::adoptedSampleRate());
    m_outputContext->controlMixer()->setGain(static_cast<float>(AudioSettings::deviceGain()));
    m_outputContext->controlMixer()->setPan(static_cast<float>(AudioSettings::devicePan()));
    m_outputContext->setHotPlugNotificationMode(
        static_cast<talcs::OutputContext::HotPlugNotificationMode>(
            AudioSettings::hotPlugNotificationMode()));
    setFileBufferingReadAheadSize(AudioSettings::fileBufferingReadAheadSize());

    if (m_outputContext->initialize(AudioSettings::driverName(), AudioSettings::deviceName())) {
        qDebug() << "Audio::OutputSystem: device initialized" << m_outputContext->device()->name()
                 << m_outputContext->driver()->name() << m_outputContext->adoptedBufferSize()
                 << m_outputContext->adoptedSampleRate();
        return true;
    }

    qWarning() << "Audio::OutputSystem: fatal: cannot initialize";
    return false;
}

bool OutputSystem::setDriver(const QString &driverName) const {
    emit deviceSwitchStarting();
    const bool followsDefaultDevice = AudioSettings::deviceName().isEmpty();
    if (m_outputContext->setDriver(driverName)) {
        postSetDevice();
        if (followsDefaultDevice)
            AudioSettings::setDeviceName(QString{});
        emit deviceSwitchFinished(true);
        qDebug() << "Audio::OutputSystem: driver changed" << m_outputContext->device()->name()
                 << m_outputContext->driver()->name() << m_outputContext->adoptedBufferSize()
                 << m_outputContext->adoptedSampleRate();
        return true;
    }

    qWarning() << "Audio::OutputSystem: fatal: cannot set driver" << driverName;
    emit deviceSwitchFinished(false);
    return false;
}

bool OutputSystem::setDevice(const QString &deviceName) const {
    emit deviceSwitchStarting();
    auto *driver = m_outputContext->driver();
    // An empty name means the driver's default endpoint and stays empty when persisted, so a
    // later startup follows the system default instead of pinning the current endpoint.
    const auto targetName = deviceName.isEmpty() && driver ? driver->defaultDevice() : deviceName;
    if (!targetName.isEmpty() && m_outputContext->setDevice(targetName)) {
        postSetDevice();
        if (deviceName.isEmpty())
            AudioSettings::setDeviceName(QString{});
        emit deviceSwitchFinished(true);
        qDebug() << "Audio::OutputSystem: device changed" << m_outputContext->device()->name()
                 << m_outputContext->driver()->name() << m_outputContext->adoptedBufferSize()
                 << m_outputContext->adoptedSampleRate();
        return true;
    }

    qWarning() << "Audio::OutputSystem: fatal: cannot set device"
               << (driver ? driver->name() : "") << deviceName;
    emit deviceSwitchFinished(false);
    return false;
}

bool OutputSystem::setAdoptedBufferSize(const qint64 bufferSize) const {
    AudioSettings::setAdoptedBufferSize(bufferSize);
    if (m_outputContext->setAdoptedBufferSize(bufferSize)) {
        qDebug() << "Audio::OutputSystem: buffer size changed" << m_outputContext->device()->name()
                 << m_outputContext->driver()->name() << m_outputContext->adoptedBufferSize()
                 << m_outputContext->adoptedSampleRate();
        return true;
    }

    qWarning() << "Audio::OutputSystem: fatal: cannot set buffer size"
               << (m_outputContext->driver() ? m_outputContext->driver()->name() : "")
               << (m_outputContext->device() ? m_outputContext->device()->name() : "")
               << m_outputContext->adoptedBufferSize() << m_outputContext->adoptedSampleRate();
    return false;
}

bool OutputSystem::setAdoptedSampleRate(const double sampleRate) const {
    AudioSettings::setAdoptedSampleRate(sampleRate);
    if (m_outputContext->setAdoptedSampleRate(sampleRate)) {
        qDebug() << "Audio::OutputSystem: sample rate changed" << m_outputContext->device()->name()
                 << m_outputContext->driver()->name() << m_outputContext->adoptedSampleRate()
                 << m_outputContext->adoptedSampleRate();
        return true;
    }

    qWarning() << "Audio::OutputSystem: fatal: cannot set sample rate"
               << (m_outputContext->driver() ? m_outputContext->driver()->name() : "")
               << (m_outputContext->device() ? m_outputContext->device()->name() : "")
               << m_outputContext->adoptedSampleRate() << m_outputContext->adoptedSampleRate();
    return false;
}

void OutputSystem::setHotPlugNotificationMode(
    const talcs::OutputContext::HotPlugNotificationMode mode) const {
    AudioSettings::setHotPlugNotificationMode(mode);
    m_outputContext->setHotPlugNotificationMode(mode);
    qDebug() << "Audio::OutputSystem: hot plug notification mode set to" << mode;
}

void OutputSystem::postSetDevice() const {
    AudioSettings::setDriverName(m_outputContext->driver()->name());
    AudioSettings::setDeviceName(m_outputContext->device()->name());
    AudioSettings::setAdoptedSampleRate(m_outputContext->adoptedSampleRate());
    AudioSettings::setAdoptedBufferSize(m_outputContext->adoptedBufferSize());
}
