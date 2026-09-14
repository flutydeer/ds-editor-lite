#include <lite/ProjectModel/Utils/Syllabification.h>

#include <lite/ProjectModel/AppModel/Note.h>

#include <algorithm>

namespace {
    using PhonemeRange = Syllabification::PhonemeRange;

    QList<PhonemeRange> splitSyllables(const QList<PhonemeName> &phonemes) {
        QList<PhonemeRange> syllables;
        int syllableStart = 0;
        bool hasOnset = false;

        for (int i = 0; i < phonemes.size(); ++i) {
            const auto &phoneme = phonemes.at(i);
            if (phoneme.isOnset && hasOnset) {
                syllables.append({syllableStart, i - syllableStart});
                syllableStart = i;
            }
            if (phoneme.isOnset)
                hasOnset = true;
        }

        if (!phonemes.isEmpty())
            syllables.append({syllableStart, static_cast<int>(phonemes.size()) - syllableStart});
        return syllables;
    }
}

namespace Syllabification {
    bool isSyllabificationLyric(const QString &lyric) {
        return Note::isSyllabificationLyric(lyric);
    }

    QList<PhonemeRange> phonemeRangesForNotes(const QStringList &lyrics,
                                            const QList<PhonemeName> &phonemes) {
        QList<PhonemeRange> result(lyrics.size());
        if (lyrics.isEmpty() || phonemes.isEmpty())
            return result;

        QList<int> syllabificationTargets{0};
        for (int i = 1; i < lyrics.size(); ++i) {
            if (isSyllabificationLyric(lyrics.at(i)))
                syllabificationTargets.append(i);
        }

        const auto syllables = splitSyllables(phonemes);
        int syllableIndex = 0;
        for (int i = 0; i < syllabificationTargets.size(); ++i) {
            const auto target = syllabificationTargets.at(i);
            if (syllableIndex >= syllables.size())
                break;

            const bool isLastTarget = i == syllabificationTargets.size() - 1;
            const int quota = target == 0 ? 1 + Note::trailingSyllabificationCount(lyrics.first())
                                         : static_cast<int>(lyrics.at(target).trimmed().size());
            const int remainingSyllables = static_cast<int>(syllables.size()) - syllableIndex;
            const int takenSyllables =
                isLastTarget ? remainingSyllables : std::min(quota, remainingSyllables);
            if (takenSyllables <= 0)
                continue;

            const auto first = syllables.at(syllableIndex);
            const auto last = syllables.at(syllableIndex + takenSyllables - 1);
            result[target] = {first.start, last.start + last.count - first.start};
            syllableIndex += takenSyllables;
        }
        return result;
    }
}
