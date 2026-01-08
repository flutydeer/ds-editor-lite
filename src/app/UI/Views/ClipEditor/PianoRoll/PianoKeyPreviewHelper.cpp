#include "PianoKeyPreviewHelper.h"
#include <cmath>

PianoKeyPreviewHelper::PianoKeyPreviewHelper() = default;

void PianoKeyPreviewHelper::detectInterval(qint64 intervalLength) {
    QMutexLocker locker(&m_mutex);
    m_currentPosition += intervalLength;
}

talcs::NoteSynthesizerDetectorMessage PianoKeyPreviewHelper::nextMessage() {
    QMutexLocker locker(&m_mutex);

    if (m_pendingMessages.isEmpty()) {
        return talcs::NoteSynthesizerDetectorMessage::Null;
    }

    auto message = m_pendingMessages.takeFirst();
    return message;
}

void PianoKeyPreviewHelper::noteOn(int midiNote, double velocity) {
    QMutexLocker locker(&m_mutex);

    const double frequency = midiNoteToFrequency(midiNote);
    talcs::NoteSynthesizerDetectorMessage message(
        0,
        talcs::NoteSynthesizerDetectorMessage::Note(
            frequency,
            velocity,
            talcs::NoteSynthesizerDetectorMessage::NoteOn
        )
    );

    m_pendingMessages.append(message);
}

void PianoKeyPreviewHelper::noteOff(int midiNote) {
    QMutexLocker locker(&m_mutex);

    const double frequency = midiNoteToFrequency(midiNote);
    talcs::NoteSynthesizerDetectorMessage message(
        0,
        talcs::NoteSynthesizerDetectorMessage::Note(
            frequency,
            talcs::NoteSynthesizerDetectorMessage::NoteOff
        )
    );

    m_pendingMessages.append(message);
}

double PianoKeyPreviewHelper::midiNoteToFrequency(int midiNote, double frequencyOfA) {
    // MIDI note 69 (A4) = 440 Hz by default
    // Formula: f = 440 * 2^((n - 69) / 12)
    return frequencyOfA * std::pow(2.0, (midiNote - 69) / 12.0);
}
