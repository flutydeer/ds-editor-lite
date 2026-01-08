#ifndef PIANOKEYPREVIEWHELPER_H
#define PIANOKEYPREVIEWHELPER_H

#include <QMutex>
#include <QList>
#include <TalcsCore/NoteSynthesizer.h>
#include <TalcsMidi/MidiMessage.h>

class PianoKeyPreviewHelper : public talcs::NoteSynthesizerDetector {
public:
    PianoKeyPreviewHelper();
    ~PianoKeyPreviewHelper() = default;

    void detectInterval(qint64 intervalLength) override;
    talcs::NoteSynthesizerDetectorMessage nextMessage() override;

    void noteOn(int midiNote, double velocity);
    void noteOff(int midiNote);

private:
    static double midiNoteToFrequency(int midiNote, double frequencyOfA = 440.0);

    QMutex m_mutex;
    QList<talcs::NoteSynthesizerDetectorMessage> m_pendingMessages;
    qint64 m_currentPosition = 0;
};

#endif // PIANOKEYPREVIEWHELPER_H
