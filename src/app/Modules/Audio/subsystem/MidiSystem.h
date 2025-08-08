#ifndef MIDISYSTEM_H
#define MIDISYSTEM_H

#include <memory>

#include <QObject>

namespace talcs {

    class MixerAudioSource;

    class MidiInputDevice;
    class MidiMessageIntegrator;
    class MidiNoteSynthesizer;
}

class MidiSystem : public QObject {
    Q_OBJECT
public:
    explicit MidiSystem(QObject *parent = nullptr);
    ~MidiSystem() override;

    bool initialize();
    talcs::MidiInputDevice *device() const;
    bool setDevice(int deviceIndex);
    talcs::MidiMessageIntegrator *integrator() const;
    talcs::MidiNoteSynthesizer *synthesizer() const;

    void setGenerator(int g) const;
    int generator() const;
    void setAmplitudeDecibel(double dB) const;
    double amplitudeDecibel() const;
    void setAttackMsec(int msec) const;
    int attackMsec() const;
    void setDecayMsec(int msec) const;
    int decayMsec() const;
    void setDecayRatio(double ratio) const;
    double decayRatio() const;
    void setReleaseMsec(int msec) const;
    int releaseMsec() const;
    void setFrequencyOfA(double frequency) const;
    double frequencyOfA() const;

    static void updateControl();

private:
    std::unique_ptr<talcs::MidiInputDevice> m_device;
    std::unique_ptr<talcs::MidiMessageIntegrator> m_integrator;
    talcs::MidiNoteSynthesizer *m_synthesizer;
    std::unique_ptr<talcs::MixerAudioSource> m_synthesizerMixer;

    void postSetDevice() const;

    void updateRateOnSampleRateChange(double sampleRate) const;
};


#endif // MIDISYSTEM_H
