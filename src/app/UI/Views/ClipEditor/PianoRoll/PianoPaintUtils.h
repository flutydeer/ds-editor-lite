//
// Created by fluty on 24-8-19.
//

#ifndef PIANOPAINTUTILS_H
#define PIANOPAINTUTILS_H

#include <QString>

class PianoPaintUtils {
public:
    static bool isWhiteKey(int midiKey);
    static QString noteName(int midiKey);
};

inline bool PianoPaintUtils::isWhiteKey(int midiKey) {
    int index = midiKey % 12;
    bool pianoKeys[] = {true,  false, true,  false, true,  true,
                        false, true,  false, true,  false, true};
    return pianoKeys[index];
}

inline QString PianoPaintUtils::noteName(int midiKey) {
    const QString noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int index = qAbs(midiKey) % 12;
    int octave = midiKey / 12 - 1;
    QString noteName = noteNames[index] + QString::number(octave);
    return noteName;
}

#endif // PIANOPAINTUTILS_H
