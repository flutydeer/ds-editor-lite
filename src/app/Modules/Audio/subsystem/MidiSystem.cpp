#include "MidiSystem.h"

#include <TalcsCore/NoteSynthesizer.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsCore/Decibels.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsMidi/MidiInputDevice.h>
#include <TalcsMidi/MidiMessageIntegrator.h>
#include <TalcsMidi/MidiNoteSynthesizer.h>

#include <QMetaObject>
#include <QTimer>

#ifdef Q_OS_WIN
#  include <Windows.h>
#  include <mmsystem.h>
#endif

#include <Modules/Audio/AudioSystem.h>
#include <Modules/Audio/subsystem/OutputSystem.h>
#include <Modules/Audio/AudioSettings.h>
#include <Modules/Audio/utils/AudioHelpers.h>

#include <Model/AppOptions/AppOptions.h>

#include <exception>

namespace {
    std::unique_ptr<talcs::MidiInputDevice> openMidiInput(const int index, QString *error) {
        try {
            auto device = std::make_unique<talcs::MidiInputDevice>(index);
            if (device->open())
                return device;
            if (error)
                *error = device->errorString();
        } catch (const std::exception &e) {
            if (error)
                *error = QString::fromUtf8(e.what());
        }
        return {};
    }
}

static qint64 msecToSample(const int msec, double sampleRate = {}) {
    const auto audioDevice = AudioSystem::outputSystem()->outputContext()->device();
    sampleRate = qFuzzyIsNull(sampleRate)
                     ? audioDevice && audioDevice->isOpen() ? audioDevice->sampleRate() : 48000.0
                     : sampleRate;
    return AudioHelpers::msecToSample(msec, sampleRate);
}

MidiSystem::MidiSystem(QObject *parent) : QObject(parent) {
    m_integrator = std::make_unique<talcs::MidiMessageIntegrator>();
    m_synthesizer = new talcs::MidiNoteSynthesizer;
    m_integrator->setStream(m_synthesizer, true);
    m_synthesizerMixer = std::make_unique<talcs::MixerAudioSource>();
    m_synthesizerMixer->addSource(m_integrator.get());
    m_errorListener = std::make_unique<ErrorListener>(this);
}

MidiSystem::~MidiSystem() {
    if (m_refreshTimer)
        m_refreshTimer->stop();
    m_device.reset(); // due to delete order issue
}

bool MidiSystem::initialize() {
    AudioSystem::outputSystem()->outputContext()->preMixer()->addSource(m_synthesizerMixer.get());
    connect(AudioSystem::outputSystem()->outputContext(), &talcs::OutputContext::sampleRateChanged,
            this, &MidiSystem::updateRateOnSampleRateChange);
    m_synthesizer->noteSynthesizer()->setGenerator(
        static_cast<talcs::NoteSynthesizer::Generator>(AudioSettings::midiSynthesizerGenerator()));

    m_synthesizer->noteSynthesizer()->setAttackTime(
        msecToSample(AudioSettings::midiSynthesizerAttackMsec()));
    m_synthesizer->noteSynthesizer()->setDecayTime(
        msecToSample(AudioSettings::midiSynthesizerDecayMsec()));
    m_synthesizer->noteSynthesizer()->setDecayRatio(AudioSettings::midiSynthesizerDecayRatio());
    m_synthesizer->noteSynthesizer()->setReleaseTime(
        msecToSample(AudioSettings::midiSynthesizerReleaseMsec()));
    m_synthesizerMixer->setGain(
        talcs::Decibels::decibelsToGain(AudioSettings::midiSynthesizerAmplitude()));
    if (qFuzzyIsNull(AudioSettings::midiSynthesizerFrequencyOfA())) {
        // TODO
    } else {
        m_synthesizer->setFrequencyOfA(AudioSettings::midiSynthesizerFrequencyOfA());
    }

    m_deviceId = AudioSettings::midiDeviceId();
    qDebug() << "Audio::MidiSystem: saved device id" << m_deviceId;
    refreshDevices();
    if (m_ports.isEmpty())
        qWarning() << "Audio::MidiSystem: no MIDI input devices available";

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(refreshIntervalMsec);
    connect(m_refreshTimer, &QTimer::timeout, this, &MidiSystem::refreshDevices);
    m_refreshTimer->start();
    return isDeviceReady();
}

std::optional<QStringList> MidiSystem::availableDevices() {
#ifdef Q_OS_WIN
    // WinMM reports the empty device list through RtMidi's stderr warning, so
    // skip the enumeration that would repeat it on every poll.
    if (midiInGetNumDevs() == 0)
        return QStringList();
#endif
    try {
        return talcs::MidiInputDevice::devices();
    } catch (const std::exception &error) {
        qWarning() << "Audio::MidiSystem: cannot enumerate MIDI inputs:" << error.what();
        return std::nullopt;
    }
}

std::optional<QStringList> MidiSystem::enumeratePorts() {
    return availableDevices();
}

talcs::MidiInputDevice *MidiSystem::device() const {
    return m_device.get();
}

bool MidiSystem::isDeviceReady() const {
    return m_device && m_device->isOpen() && m_device->errorString().isEmpty();
}

QString MidiSystem::deviceId() const {
    return m_deviceId;
}

QString MidiSystem::deviceErrorString() const {
    return m_error;
}

MidiPortList MidiSystem::ports() const {
    return m_ports;
}

bool MidiSystem::setDeviceId(const QString &deviceId) {
    if (m_deviceId == deviceId && isDeviceReady())
        return true;
    // The cached list may be one poll old, and an index taken from a stale list
    // opens whichever port now sits at that position.
    refreshPorts();
    const auto index = m_ports.ids().indexOf(deviceId);
    if (index < 0) {
        m_error = tr("The MIDI input device is unavailable.");
        emit deviceChanged();
        return false;
    }

    QString error;
    auto dev = openMidiInput(index, &error);
    if (!dev) {
        qWarning() << "Audio::MidiSystem: cannot open MIDI input" << index << error;
        m_error = error;
        emit deviceChanged();
        return false;
    }
    if (dev->name() != MidiPortList::nameOf(deviceId)) {
        // The list changed between the refresh above and the open.
        qWarning() << "Audio::MidiSystem: MIDI input changed while opening" << index << dev->name()
                   << "instead of" << MidiPortList::nameOf(deviceId);
        m_error = tr("The MIDI input device is unavailable.");
        emit deviceChanged();
        return false;
    }
    qDebug() << "Audio::MidiSystem: MIDI device opened" << dev->name();
    m_device = std::move(dev);
    m_deviceId = deviceId;
    m_error.clear();
    attachDevice();
    if (AudioSettings::midiDeviceId() != deviceId)
        AudioSettings::setMidiDeviceId(deviceId);
    emit deviceChanged();
    return true;
}

void MidiSystem::refreshDevices() {
    const MidiPortList previous = m_ports;
    refreshPorts();

    // A fresh profile follows the first device that can be opened. Keeping a
    // copy of the ids: opening a device re-enumerates the ports.
    const auto ids = m_ports.ids();
    if (m_deviceId.isEmpty() && !ids.isEmpty()) {
        for (const auto &id : ids) {
            if (setDeviceId(id))
                break;
        }
    }

    switch (m_ports.deviceAction(m_deviceId, isDeviceReady(), previous)) {
        case MidiPortList::DeviceAction::Close:
            releaseDevice();
            break;
        case MidiPortList::DeviceAction::Open:
            // The port came back, or the device was released by another program.
            setDeviceId(m_deviceId);
            break;
        case MidiPortList::DeviceAction::None:
            break;
    }
}

bool MidiSystem::refreshPorts() {
    const auto names = enumeratePorts();
    if (!names) // a failed poll says nothing about the ports
        return false;
    const MidiPortList ports(*names);
    if (ports == m_ports)
        return false;
    m_ports = ports;
    emit devicesChanged();
    return true;
}

void MidiSystem::releaseDevice() {
    // Closing reports the source as stopped, which silences its notes.
    qDebug() << "Audio::MidiSystem: MIDI device lost" << m_deviceId
             << MidiPortList::nameOf(m_deviceId);
    dropDevice();
}

void MidiSystem::attachDevice() {
    // An error queued by a former device must not be taken for this one.
    ++m_deviceGeneration;
    m_errorListener->setGeneration(m_deviceGeneration);
    m_device->listener()->addFilter(m_integrator.get());
    m_device->listener()->addFilter(m_errorListener.get());
}

void MidiSystem::handleDeviceError(const QString &errorString, const quint64 generation) {
    if (!m_device || generation != m_deviceGeneration)
        return;
    qWarning() << "Audio::MidiSystem: MIDI device error" << m_deviceId << errorString;
    m_error = errorString;
    dropDevice();
}

void MidiSystem::dropDevice() {
    ++m_deviceGeneration;
    m_device.reset();
    emit deviceChanged();
}

MidiSystem::ErrorListener::ErrorListener(MidiSystem *system) : m_system(system) {
}

void MidiSystem::ErrorListener::setGeneration(const quint64 generation) {
    m_generation.store(generation);
}

void MidiSystem::ErrorListener::processError(const QString &errorString) {
    const auto error = errorString;
    const auto generation = m_generation.load();
    QMetaObject::invokeMethod(
        m_system,
        [system = m_system, error, generation] { system->handleDeviceError(error, generation); },
        Qt::QueuedConnection);
}

talcs::MidiMessageIntegrator *MidiSystem::integrator() const {
    return m_integrator.get();
}

talcs::MidiNoteSynthesizer *MidiSystem::synthesizer() const {
    return m_synthesizer;
}

void MidiSystem::setGenerator(int g) const {
    AudioSettings::setMidiSynthesizerGenerator(g);
    m_synthesizer->noteSynthesizer()->setGenerator(
        static_cast<talcs::NoteSynthesizer::Generator>(g));
}

int MidiSystem::generator() const {
    Q_UNUSED(this)
    return AudioSettings::midiSynthesizerGenerator();
}

void MidiSystem::setAmplitudeDecibel(const double dB) const {
    AudioSettings::setMidiSynthesizerAmplitude(dB);
    m_synthesizerMixer->setGain(talcs::Decibels::decibelsToGain(static_cast<float>(dB)));
}

double MidiSystem::amplitudeDecibel() const {
    Q_UNUSED(this)
    return AudioSettings::midiSynthesizerAmplitude();
}

void MidiSystem::setAttackMsec(const int msec) const {
    AudioSettings::setMidiSynthesizerAttackMsec(msec);
    m_synthesizer->noteSynthesizer()->setAttackTime(msecToSample(msec));
}

int MidiSystem::attackMsec() const {
    Q_UNUSED(this)
    return AudioSettings::midiSynthesizerAttackMsec();
}

void MidiSystem::setDecayMsec(const int msec) const {
    AudioSettings::setMidiSynthesizerDecayMsec(msec);
    m_synthesizer->noteSynthesizer()->setDecayTime(msecToSample(msec));
}

int MidiSystem::decayMsec() const {
    Q_UNUSED(this)
    return AudioSettings::midiSynthesizerDecayMsec();
}

void MidiSystem::setDecayRatio(const double ratio) const {
    AudioSettings::setMidiSynthesizerDecayRatio(ratio);
    m_synthesizer->noteSynthesizer()->setDecayRatio(ratio);
}

double MidiSystem::decayRatio() const {
    Q_UNUSED(this)
    return AudioSettings::midiSynthesizerDecayRatio();
}

void MidiSystem::setReleaseMsec(const int msec) const {
    AudioSettings::setMidiSynthesizerReleaseMsec(msec);
    m_synthesizer->noteSynthesizer()->setReleaseTime(msecToSample(msec));
}

int MidiSystem::releaseMsec() const {
    Q_UNUSED(this)
    return AudioSettings::midiSynthesizerReleaseMsec();
}

void MidiSystem::setFrequencyOfA(const double frequency) const {
    AudioSettings::setMidiSynthesizerFrequencyOfA(frequency);
    if (qFuzzyIsNull(frequency)) {
        // TODO adjust by cent shift
    } else {
        m_synthesizer->setFrequencyOfA(frequency);
    }
}

double MidiSystem::frequencyOfA() const {
    Q_UNUSED(this)
    return AudioSettings::midiSynthesizerFrequencyOfA();
}

void MidiSystem::updateControl() {
}

void MidiSystem::updateRateOnSampleRateChange(const double sampleRate) const {
    m_synthesizer->noteSynthesizer()->setAttackTime(
        msecToSample(AudioSettings::midiSynthesizerAttackMsec(), sampleRate));
    m_synthesizer->noteSynthesizer()->setDecayTime(
        msecToSample(AudioSettings::midiSynthesizerDecayMsec(), sampleRate));
    m_synthesizer->noteSynthesizer()->setReleaseTime(
        msecToSample(AudioSettings::midiSynthesizerReleaseMsec(), sampleRate));
}
