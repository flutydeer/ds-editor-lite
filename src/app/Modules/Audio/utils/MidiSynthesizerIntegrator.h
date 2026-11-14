#ifndef AUDIO_MIDISYNTHESIZERINTEGRATOR_H
#define AUDIO_MIDISYNTHESIZERINTEGRATOR_H

#include <QMutex>

#include <TalcsMidi/MidiMessageIntegrator.h>

namespace talcs {
    class MidiNoteSynthesizer;
}

// Serializes queue changes and voice resets with the entire render interval.
// Owners must detach input callbacks and audio sources before destruction.
class MidiSynthesizerIntegrator final : public talcs::MidiMessageIntegrator {
public:
    explicit MidiSynthesizerIntegrator(talcs::MidiNoteSynthesizer *synthesizer,
                                       bool takeOwnership = false);

    bool open(qint64 bufferSize, double sampleRate) override;
    void close() override;
    void interruptAllNotes(bool force = false);

protected:
    qint64 processReading(const talcs::AudioSourceReadData &readData) override;
    bool processMessage(const talcs::MidiMessage &message) override;
    bool processDeviceWillStart(talcs::MidiInputDevice *device) override;
    void processDeviceStopped() override;
    void processError(const QString &errorString) override;

private:
    QMutex m_mutex;
    talcs::MidiNoteSynthesizer *m_synthesizer;
};

#endif // AUDIO_MIDISYNTHESIZERINTEGRATOR_H
