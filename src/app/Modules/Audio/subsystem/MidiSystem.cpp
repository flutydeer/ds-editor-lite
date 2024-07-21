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

#include <Model/AppOptions/AppOptions.h>

static double msecToRate(int msec, double sampleRate) {
    if (msec == 0)
        return 0.005;
    return std::pow(0.005, 1000.0 / (msec * sampleRate));
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
    connect(AudioSystem::outputSystem()->outputContext(), &talcs::OutputContext::sampleRateChanged, this, &MidiSystem::updateRateOnSampleRateChange);
    m_synthesizer->noteSynthesizer()->setGenerator(static_cast<talcs::NoteSynthesizer::Generator>(appOptions->audio()->midiSynthesizerGenerator));
    auto audioDevice = AudioSystem::outputSystem()->outputContext()->device();
    m_synthesizer->noteSynthesizer()->setAttackRate(msecToRate(appOptions->audio()->midiSynthesizerAttackMsec, audioDevice && audioDevice->isOpen() ? audioDevice->sampleRate() : 48000));
    m_synthesizer->noteSynthesizer()->setReleaseRate(msecToRate(appOptions->audio()->midiSynthesizerReleaseMsec, audioDevice && audioDevice->isOpen() ? audioDevice->sampleRate() : 48000));
    m_synthesizerMixer->setGain(talcs::Decibels::decibelsToGain(appOptions->audio()->midiSynthesizerAmplitude));
    if (qFuzzyIsNull(appOptions->audio()->midiSynthesizerFrequencyOfA)) {
        // TODO
    } else {
        m_synthesizer->setFrequencyOfA(appOptions->audio()->midiSynthesizerFrequencyOfA);
    }

    auto savedDeviceIndex = appOptions->audio()->midiDeviceIndex;
    qDebug() << "Audio::MidiSystem: saved device index" << savedDeviceIndex;
    auto deviceCount = talcs::MidiInputDevice::devices().size();
    for(int i = -1; i < deviceCount; i++) {
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
    qDebug() << "Audio::MidiSystem: MIDI device initialized" << m_device->deviceIndex() << m_device->name();
    postSetDevice();
    return true;
}
talcs::MidiInputDevice *MidiSystem::device() {
    return m_device.get();
}
bool MidiSystem::setDevice(int deviceIndex) {
    auto dev = std::make_unique<talcs::MidiInputDevice>(deviceIndex);
    if (!dev->open()) {
        return false;
    }
    qDebug() << "Audio::MidiSystem: MIDI device changed" << dev->name();
    m_device = std::move(dev);
    appOptions->audio()->midiDeviceIndex = deviceIndex;
    postSetDevice();
    return true;
}
void MidiSystem::postSetDevice() {
    m_device->listener()->addFilter(m_integrator.get());
}
talcs::MidiMessageIntegrator *MidiSystem::integrator() {
    return m_integrator.get();
}
talcs::MidiNoteSynthesizer *MidiSystem::synthesizer() {
    return m_synthesizer;
}
void MidiSystem::setGenerator(int g) {
    appOptions->audio()->midiSynthesizerGenerator = static_cast<talcs::NoteSynthesizer::Generator>(g);
    m_synthesizer->noteSynthesizer()->setGenerator(static_cast<talcs::NoteSynthesizer::Generator>(g));
}
int MidiSystem::generator() const {
    
    
    return appOptions->audio()->midiSynthesizerGenerator;
}
void MidiSystem::setAmplitudeDecibel(double dB) {
    appOptions->audio()->midiSynthesizerAmplitude = dB;
    m_synthesizerMixer->setGain(talcs::Decibels::decibelsToGain(static_cast<float>(dB)));
}
double MidiSystem::amplitudeDecibel() const {
    
    
    return appOptions->audio()->midiSynthesizerAmplitude;
}
void MidiSystem::setAttackMsec(int msec) {
    appOptions->audio()->midiSynthesizerAttackMsec = msec;
    auto audioDevice = AudioSystem::outputSystem()->outputContext()->device();
    m_synthesizer->noteSynthesizer()->setAttackRate(msecToRate(msec, audioDevice && audioDevice->isOpen() ? audioDevice->sampleRate() : 48000));
}
int MidiSystem::attackMsec() const {
    
    
    return appOptions->audio()->midiSynthesizerAttackMsec;
}
void MidiSystem::setReleaseMsec(int msec) {
    appOptions->audio()->midiSynthesizerReleaseMsec = msec;
    auto audioDevice = AudioSystem::outputSystem()->outputContext()->device();
    m_synthesizer->noteSynthesizer()->setReleaseRate(msecToRate(msec, audioDevice && audioDevice->isOpen() ? audioDevice->sampleRate() : 48000));
}
int MidiSystem::releaseMsec() const {
    
    
    return appOptions->audio()->midiSynthesizerReleaseMsec;
}
void MidiSystem::setFrequencyOfA(double frequency) {
    appOptions->audio()->midiSynthesizerFrequencyOfA = frequency;
    if (qFuzzyIsNull(frequency)) {
        // TODO
    } else {
        m_synthesizer->setFrequencyOfA(frequency);
    }

}
double MidiSystem::frequencyOfA() const {
    
    
    return appOptions->audio()->midiSynthesizerFrequencyOfA;
}
void MidiSystem::updateControl() {
}
void MidiSystem::updateRateOnSampleRateChange(double sampleRate) {
    
    
    m_synthesizer->noteSynthesizer()->setAttackRate(msecToRate(appOptions->audio()->midiSynthesizerAttackMsec, sampleRate));
    m_synthesizer->noteSynthesizer()->setReleaseRate(msecToRate(appOptions->audio()->midiSynthesizerReleaseMsec, sampleRate));
}