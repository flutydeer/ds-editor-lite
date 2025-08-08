#include "MidiSystem.h"

#include <TalcsCore/NoteSynthesizer.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsCore/Decibels.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsMidi/MidiInputDevice.h>
#include <TalcsMidi/MidiMessageIntegrator.h>
#include <TalcsMidi/MidiNoteSynthesizer.h>

#include <Modules/Audio/AudioSystem.h>
#include <Modules/Audio/subsystem/OutputSystem.h>
#include <Modules/Audio/AudioSettings.h>
#include <Modules/Audio/utils/AudioHelpers.h>

#include <Model/AppOptions/AppOptions.h>

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
}

MidiSystem::~MidiSystem() {
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

    const auto savedDeviceIndex = AudioSettings::midiDeviceIndex();
    qDebug() << "Audio::MidiSystem: saved device index" << savedDeviceIndex;
    const auto deviceCount = talcs::MidiInputDevice::devices().size();
    for (int i = -1; i < deviceCount; i++) {
        if (i == savedDeviceIndex)
            continue;
        int deviceIndex = i;
        if (deviceIndex == -1)
            deviceIndex = savedDeviceIndex;
        if (deviceIndex == -1)
            deviceIndex = 0;
        auto dev = std::make_unique<talcs::MidiInputDevice>(deviceIndex);
        if (dev->open()) {
            m_device = std::move(dev);
            break;
        }
    }
    if (!m_device) {
        qWarning() << "Audio::MidiSystem: fatal: no available device";
        return false;
    }
    qDebug() << "Audio::MidiSystem: MIDI device initialized" << m_device->deviceIndex()
             << m_device->name();
    postSetDevice();
    return true;
}

talcs::MidiInputDevice *MidiSystem::device() const {
    return m_device.get();
}

bool MidiSystem::setDevice(int deviceIndex) {
    auto dev = std::make_unique<talcs::MidiInputDevice>(deviceIndex);
    if (!dev->open()) {
        return false;
    }
    qDebug() << "Audio::MidiSystem: MIDI device changed" << dev->name();
    m_device = std::move(dev);
    AudioSettings::setMidiDeviceIndex(deviceIndex);
    postSetDevice();
    return true;
}

void MidiSystem::postSetDevice() const {
    m_device->listener()->addFilter(m_integrator.get());
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