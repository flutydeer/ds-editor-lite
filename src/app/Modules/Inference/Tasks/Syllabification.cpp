#include "Syllabification.h"

#include "Modules/Inference/Models/InferInputNote.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/Note.h>

#include <algorithm>

namespace {
    using PhonemeRange = Syllabification::PhonemeRange;

    QList<PhonemeRange> splitOnsetGroups(const QList<PhonemeName> &phonemes) {
        QList<PhonemeRange> onsetGroups;
        int onsetGroupStart = 0;
        bool hasOnset = false;

        for (int i = 0; i < phonemes.size(); ++i) {
            const auto &phoneme = phonemes.at(i);
            if (phoneme.isOnset && hasOnset) {
                onsetGroups.append({onsetGroupStart, i - onsetGroupStart});
                onsetGroupStart = i;
            }
            if (phoneme.isOnset)
                hasOnset = true;
        }

        if (!phonemes.isEmpty())
            onsetGroups.append(
                {onsetGroupStart, static_cast<int>(phonemes.size()) - onsetGroupStart});
        return onsetGroups;
    }

    bool isLyricGroupMarker(const QString &lyric) {
        return Note::isSlurLyric(lyric) || Syllabification::isSyllabificationLyric(lyric);
    }

    int noteStartDeltaMs(const InferInputNote &root, const InferInputNote &note,
                         const Timeline &timeline, const int clipStartTick) {
        const auto rootStartMs = timeline.tickToMs(clipStartTick + root.start);
        const auto noteStartMs = timeline.tickToMs(clipStartTick + note.start);
        return qRound(noteStartMs - rootStartMs);
    }

    int lyricGroupEnd(const QStringList &lyrics, const QList<InferInputNote> &notes,
                      const int rootIndex) {
        auto lyricGroupEndTick = notes.at(rootIndex).start + notes.at(rootIndex).length;
        int end = rootIndex + 1;
        for (; end < notes.size() && isLyricGroupMarker(lyrics.at(end)); ++end) {
            const auto &marker = notes.at(end);
            if (marker.start > lyricGroupEndTick)
                break;
            lyricGroupEndTick = std::max(lyricGroupEndTick, marker.start + marker.length);
        }
        return end;
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

        const auto onsetGroups = splitOnsetGroups(phonemes);
        int onsetGroupIndex = 0;
        for (int i = 0; i < syllabificationTargets.size(); ++i) {
            const auto target = syllabificationTargets.at(i);
            if (onsetGroupIndex >= onsetGroups.size())
                break;

            const bool isLastTarget = i == syllabificationTargets.size() - 1;
            const int quota = target == 0 ? 1 + Note::trailingSyllabificationCount(lyrics.first())
                                          : static_cast<int>(lyrics.at(target).trimmed().size());
            const int remainingOnsetGroups =
                static_cast<int>(onsetGroups.size()) - onsetGroupIndex;
            const int takenOnsetGroups =
                isLastTarget ? remainingOnsetGroups : std::min(quota, remainingOnsetGroups);
            if (takenOnsetGroups <= 0)
                continue;

            const auto first = onsetGroups.at(onsetGroupIndex);
            const auto last = onsetGroups.at(onsetGroupIndex + takenOnsetGroups - 1);
            result[target] = {first.start, last.start + last.count - first.start};
            onsetGroupIndex += takenOnsetGroups;
        }
        return result;
    }

    void keepPhonemesOnLyricGroupRoots(const QList<NoteInferenceSnapshot> &notes,
                                       QList<PhonemeNameResult> &results) {
        if (notes.size() != results.size())
            return;

        for (int i = 0; i < notes.size(); ++i) {
            if (!isLyricGroupMarker(notes.at(i).lyric))
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
            if (isLyricGroupMarker(lyrics.at(rootIndex))) {
                notes[rootIndex].phonemeNames.clear();
                notes[rootIndex].phonemeOffsets.clear();
                ++rootIndex;
                continue;
            }

            const auto end = lyricGroupEnd(lyrics, notes, rootIndex);

            const auto lyricGroupLyrics = lyrics.mid(rootIndex, end - rootIndex);
            const auto storedNames = notes.at(rootIndex).phonemeNames;
            const auto storedOffsets = notes.at(rootIndex).phonemeOffsets;
            const auto ranges = phonemeRangesForNotes(lyricGroupLyrics, storedNames);
            const bool offsetsReady = storedOffsets.size() == storedNames.size();
            const auto root = notes.at(rootIndex);

            for (int i = rootIndex; i < end; ++i) {
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
            rootIndex = end;
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
            if (isLyricGroupMarker(lyrics.at(rootIndex))) {
                ++rootIndex;
                continue;
            }

            const auto end = lyricGroupEnd(lyrics, notes, rootIndex);

            const auto &root = notes.at(rootIndex);
            bool offsetsReady = true;
            QList<int> storedOffsets;
            for (int i = rootIndex; i < end; ++i) {
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
            rootIndex = end;
        }
        return result;
    }
}
