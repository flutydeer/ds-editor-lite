#include "PhonemeDistribution.h"

#include "Modules/Inference/Models/InferInputNote.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/Note.h>

#include <algorithm>

namespace {
    using PhonemeRange = PhonemeDistribution::PhonemeRange;

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
            syllables.append(
                {syllableStart, static_cast<int>(phonemes.size()) - syllableStart});
        return syllables;
    }

    bool isMarkerLyric(const QString &lyric) {
        return lyric.trimmed() == "-" || PhonemeDistribution::isPlusLyric(lyric);
    }

    int noteStartDeltaMs(const InferInputNote &root, const InferInputNote &note,
                         const Timeline &timeline, const int clipStartTick) {
        const auto rootStartMs = timeline.tickToMs(clipStartTick + root.start);
        const auto noteStartMs = timeline.tickToMs(clipStartTick + note.start);
        return qRound(noteStartMs - rootStartMs);
    }

    int continuousGroupEnd(const QStringList &lyrics, const QList<InferInputNote> &notes,
                           const int rootIndex) {
        auto groupEndTick = notes.at(rootIndex).start + notes.at(rootIndex).length;
        int groupEnd = rootIndex + 1;
        for (; groupEnd < notes.size() && isMarkerLyric(lyrics.at(groupEnd)); ++groupEnd) {
            const auto &marker = notes.at(groupEnd);
            if (marker.start > groupEndTick)
                break;
            groupEndTick = std::max(groupEndTick, marker.start + marker.length);
        }
        return groupEnd;
    }
}

namespace PhonemeDistribution {
    bool isPlusLyric(const QString &lyric) {
        return Note::isPlusLyric(lyric);
    }

    QList<PhonemeRange> phonemeRangesForNotes(const QStringList &lyrics,
                                              const QList<PhonemeName> &phonemes) {
        QList<PhonemeRange> result(lyrics.size());
        if (lyrics.isEmpty() || phonemes.isEmpty())
            return result;

        QList<int> allocationTargets{0};
        for (int i = 1; i < lyrics.size(); ++i) {
            if (isPlusLyric(lyrics.at(i)))
                allocationTargets.append(i);
        }

        const auto syllables = splitSyllables(phonemes);
        int syllableIndex = 0;
        for (int i = 0; i < allocationTargets.size(); ++i) {
            const auto target = allocationTargets.at(i);
            if (syllableIndex >= syllables.size())
                break;

            const bool isLastTarget = i == allocationTargets.size() - 1;
            const int quota =
                target == 0 ? 1 + Note::trailingPlusCount(lyrics.first())
                            : static_cast<int>(lyrics.at(target).trimmed().size());
            const int remainingSyllables =
                static_cast<int>(syllables.size()) - syllableIndex;
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

    void keepPhonemesOnGroupRoots(const QList<NoteInferenceSnapshot> &notes,
                                  QList<PhonemeNameResult> &results) {
        if (notes.size() != results.size())
            return;

        for (int i = 0; i < notes.size(); ++i) {
            if (!isMarkerLyric(notes.at(i).lyric))
                continue;
            results[i].phonemeNames.clear();
            results[i].success = true;
        }
    }

    void distributeForInference(const QStringList &lyrics, QList<InferInputNote> &notes,
                                const Timeline &timeline, const int clipStartTick) {
        if (lyrics.size() != notes.size())
            return;

        int rootIndex = 0;
        while (rootIndex < notes.size()) {
            if (isMarkerLyric(lyrics.at(rootIndex))) {
                notes[rootIndex].phonemeNames.clear();
                notes[rootIndex].phonemeOffsets.clear();
                ++rootIndex;
                continue;
            }

            const auto groupEnd = continuousGroupEnd(lyrics, notes, rootIndex);

            const auto groupLyrics = lyrics.mid(rootIndex, groupEnd - rootIndex);
            const auto storedNames = notes.at(rootIndex).phonemeNames;
            const auto storedOffsets = notes.at(rootIndex).phonemeOffsets;
            const auto ranges = phonemeRangesForNotes(groupLyrics, storedNames);
            const bool offsetsReady = storedOffsets.size() == storedNames.size();
            const auto root = notes.at(rootIndex);

            for (int i = rootIndex; i < groupEnd; ++i) {
                const auto range = ranges.at(i - rootIndex);
                auto &note = notes[i];
                note.phonemeNames = storedNames.mid(range.start, range.count);
                note.phonemeOffsets.clear();
                if (!offsetsReady || range.count == 0)
                    continue;

                const int deltaMs = noteStartDeltaMs(root, note, timeline, clipStartTick);
                note.phonemeOffsets.reserve(range.count);
                for (int k = range.start; k < range.start + range.count; ++k)
                    note.phonemeOffsets.append(storedOffsets.at(k) - deltaMs);
            }
            rootIndex = groupEnd;
        }
    }

    QList<QList<int>> collectForStorage(const QStringList &lyrics,
                                        const QList<InferInputNote> &notes,
                                        const Timeline &timeline, const int clipStartTick) {
        QList<QList<int>> result(notes.size());
        if (lyrics.size() != notes.size())
            return result;

        int rootIndex = 0;
        while (rootIndex < notes.size()) {
            if (isMarkerLyric(lyrics.at(rootIndex))) {
                ++rootIndex;
                continue;
            }

            const auto groupEnd = continuousGroupEnd(lyrics, notes, rootIndex);

            const auto &root = notes.at(rootIndex);
            bool offsetsReady = true;
            QList<int> storedOffsets;
            for (int i = rootIndex; i < groupEnd; ++i) {
                const auto &note = notes.at(i);
                if (note.phonemeNames.size() != note.phonemeOffsets.size()) {
                    offsetsReady = false;
                    break;
                }

                const int deltaMs = noteStartDeltaMs(root, note, timeline, clipStartTick);
                storedOffsets.reserve(storedOffsets.size() + note.phonemeOffsets.size());
                for (const auto offset : note.phonemeOffsets)
                    storedOffsets.append(offset + deltaMs);
            }
            if (offsetsReady)
                result[rootIndex] = storedOffsets;
            rootIndex = groupEnd;
        }
        return result;
    }
}
