#ifndef NOTEUTILS_H
#define NOTEUTILS_H

#include <optional>

#include <QStringView>

namespace NoteUtils {
    inline std::optional<int> noteStringToMidi(const QStringView noteString) {
        const QStringView s = noteString.trimmed();
        if (s.isEmpty()) {
            return std::nullopt;
        }

        // 1. Note name
        const QChar letterUpper = s.at(0).toUpper();
        if (letterUpper < 'A' || letterUpper > 'G') {
            return std::nullopt;
        }

        qsizetype i = 1;
        int accidental = 0;

        // 2. Accidentals:
        //   Sharp: #, +1 smt
        //   Flat: b, -1 smt
        //   Double Sharp; x or X, +2 smt
        // Any combinations of accidentals is also supported:
        //   ##, +2 smt (1 + 1)
        //   bb, -2 smt (-1 + -1)
        //   #b, 0 smt (1 + -1)
        //   ####b, +3 smt (1 + 1 + 1 + 1 + -1)
        while (i < s.size()) {
            const QChar c = s.at(i);
            if (c == u'#') {
                accidental += 1;
                i++;
            } else if (c == u'b') {
                accidental -= 1;
                i++;
            } else if (c == u'x' || c == u'X') {
                accidental += 2; // x 表示 double sharp
                i++;
            } else {
                break;
            }
        }

        // 3. Octave (negative is also supported)
        if (i >= s.size()) {
            return std::nullopt;
        }

        bool ok = false;
        const int octave = s.sliced(i).toInt(&ok);
        if (!ok) {
            return std::nullopt;
        }

        // 4. Calculate note base semitone
        int baseSemitone = 0;
        if (letterUpper == 'C') {
            baseSemitone = 0;
        } else if (letterUpper == 'D') {
            baseSemitone = 2;
        } else if (letterUpper == 'E') {
            baseSemitone = 4;
        } else if (letterUpper == 'F') {
            baseSemitone = 5;
        } else if (letterUpper == 'G') {
            baseSemitone = 7;
        } else if (letterUpper == 'A') {
            baseSemitone = 9;
        } else if (letterUpper == 'B') {
            baseSemitone = 11;
        } else {
            return std::nullopt;
        }

        const int totalSemitone = baseSemitone + accidental;
        const int midi = 12 * (octave + 1) + totalSemitone;

        return midi;
    }
}

#endif // NOTEUTILS_H