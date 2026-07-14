#include "PhonemeDistribution.h"

#include <utility>

namespace {
    using Syllable = QList<PhonemeName>;

    bool isPlusNote(const QString &lyric) {
        if (lyric.isEmpty())
            return false;
        for (const auto &ch : lyric) {
            if (ch != '+')
                return false;
        }
        return true;
    }

    std::pair<bool, int> checkTrailingPlus(const QString &lyric) {
        if (!lyric.endsWith('+'))
            return {false, 0};

        int count = 0;
        for (int i = lyric.length() - 1; i >= 0 && lyric[i] == '+'; --i)
            ++count;
        return {true, count};
    }

    QList<Syllable> splitSyllables(const QList<PhonemeName> &phonemes) {
        QList<Syllable> syllables;
        Syllable currentSyllable;
        bool hasOnset = false;

        for (const auto &phoneme : phonemes) {
            if (phoneme.isOnset && hasOnset) {
                syllables.append(currentSyllable);
                currentSyllable.clear();
                hasOnset = false;
            }
            if (phoneme.isOnset)
                hasOnset = true;
            currentSyllable.append(phoneme);
        }

        if (!currentSyllable.isEmpty())
            syllables.append(currentSyllable);
        return syllables;
    }
}

void distributePhonemesToNotes(const QList<NoteInferenceSnapshot> &notes,
                               QList<PhonemeNameResult> &results, const int gapThresholdTicks) {
    const int count = notes.size();
    int i = 0;

    auto tickGap = [&notes](int a, int b) {
        return notes[b].globalStart - (notes[a].globalStart + notes[a].length);
    };

    while (i < count) {
        const auto &lyric = notes[i].lyric;
        const auto &pronunciation = notes[i].pronunciation;

        if (lyric == "SP" || lyric == "AP" || pronunciation == "-") {
            ++i;
            continue;
        }

        if (isPlusNote(lyric)) {
            results[i].phonemeNames.clear();
            ++i;
            continue;
        }

        QList<int> plusIndices;
        int j = i + 1;
        int previousIndex = i;
        while (j < count) {
            if (tickGap(previousIndex, j) > gapThresholdTicks)
                break;

            if (isPlusNote(notes[j].lyric)) {
                plusIndices.append(j);
                previousIndex = j;
                ++j;
            } else if (notes[j].pronunciation == "-") {
                previousIndex = j;
                ++j;
            } else {
                break;
            }
        }

        const auto [hasWordPlus, wordExtraCount] = checkTrailingPlus(lyric);
        if (plusIndices.isEmpty() && !hasWordPlus) {
            i = j;
            continue;
        }

        const auto syllables = splitSyllables(results[i].phonemeNames);
        if (syllables.isEmpty()) {
            i = j;
            continue;
        }

        int groupIndex = 0;
        const int wordGroupCount = 1 + (hasWordPlus ? wordExtraCount : 0);
        QList<PhonemeName> wordPhonemes;
        for (int c = 0; c < wordGroupCount && groupIndex < syllables.size(); ++c, ++groupIndex) {
            wordPhonemes.append(syllables[groupIndex]);
        }
        results[i].phonemeNames = wordPhonemes;

        for (int pi = 0; pi < plusIndices.size(); ++pi) {
            const int index = plusIndices[pi];
            const int plusCount = notes[index].lyric.length();
            const bool isLast = pi == plusIndices.size() - 1;
            QList<PhonemeName> merged;

            if (isLast) {
                for (int group = groupIndex; group < syllables.size(); ++group)
                    merged.append(syllables[group]);
                groupIndex = syllables.size();
            } else {
                for (int c = 0; c < plusCount && groupIndex < syllables.size(); ++c, ++groupIndex) {
                    merged.append(syllables[groupIndex]);
                }
            }

            results[index].phonemeNames = merged;
            results[index].success = !merged.isEmpty();
        }

        i = j;
    }
}
