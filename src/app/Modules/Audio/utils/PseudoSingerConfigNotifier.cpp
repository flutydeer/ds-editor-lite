#include "PseudoSingerConfigNotifier.h"

#include <TalcsCore/Decibels.h>
#include <TalcsCore/NoteSynthesizer.h>
#include <TalcsDevice/AudioDevice.h>

#include <Modules/Audio/AudioSettings.h>
#include <Modules/Audio/AudioSystem.h>
#include <Modules/Audio/utils/AudioHelpers.h>
#include <Modules/Audio/subsystem/OutputSystem.h>

static PseudoSingerConfigNotifier *m_instance = nullptr;

PseudoSingerConfigNotifier::PseudoSingerConfigNotifier(QObject *parent) : QObject(parent) {
    m_instance = this;
}

PseudoSingerConfigNotifier::~PseudoSingerConfigNotifier() = default;

PseudoSingerConfigNotifier *PseudoSingerConfigNotifier::instance() {
    return m_instance;
}

static qint64 msecToSample(int msec, double sampleRate = {}) { // FIXME use exporter sample rate
    auto audioDevice = AudioSystem::outputSystem()->outputContext()->device();
    sampleRate = qFuzzyIsNull(sampleRate)
                     ? audioDevice && audioDevice->isOpen() ? audioDevice->sampleRate() : 48000.0
                     : sampleRate;
    return AudioHelpers::msecToSample(msec, sampleRate);
}

talcs::NoteSynthesizerConfig PseudoSingerConfigNotifier::config(int synthIndex) {
    talcs::NoteSynthesizerConfig config;
    config.setGenerator(static_cast<talcs::NoteSynthesizer::Generator>(AudioSettings::pseudoSingerSynthGenerator(synthIndex)));
    config.setAmplitude(talcs::Decibels::decibelsToGain(AudioSettings::pseudoSingerSynthAmplitude(synthIndex)));
    config.setAttackTime(msecToSample(AudioSettings::pseudoSingerSynthAttackMsec(synthIndex)));
    config.setDecayTime(msecToSample(AudioSettings::pseudoSingerSynthDecayMsec(synthIndex)));
    config.setDecayRatio(AudioSettings::pseudoSingerSynthDecayRatio(synthIndex));
    config.setReleaseTime(msecToSample(AudioSettings::pseudoSingerSynthReleaseMsec(synthIndex)));
    return config;
}

void PseudoSingerConfigNotifier::notify(int synthIndex) {
    emit m_instance->configChanged(synthIndex, config(synthIndex));
}
