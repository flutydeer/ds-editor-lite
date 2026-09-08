#include "Syllabification.h"

#include "Modules/Inference/Models/InferInputNote.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/Note.h>

#include <algorithm>

namespace {
    bool isWordContinuationLyric(const QString &lyric) {
        return Note::isSlurLyric(lyric) || Syllabification::isSyllabificationLyric(lyric);
    }

    int noteStartDeltaMs(const InferInputNote &root, const InferInputNote &note,
                         const Timeline &timeline, const int clipStartTick) {
        const auto rootStartMs = timeline.tickToMs(clipStartTick + root.start);
        const auto noteStartMs = timeline.tickToMs(clipStartTick + note.start);
        return qRound(noteStartMs - rootStartMs);
    }

    int wordEnd(const QStringList &lyrics, const QList<InferInputNote> &notes,
                const int rootIndex) {
        auto wordEndTick = notes.at(rootIndex).start + notes.at(rootIndex).length;
        int end = rootIndex + 1;
        for (; end < notes.size() && isWordContinuationLyric(lyrics.at(end)); ++end) {
            const auto &continuationNote = notes.at(end);
            if (continuationNote.start > wordEndTick)
                break;
            wordEndTick = std::max(wordEndTick, continuationNote.start + continuationNote.length);
        }
        return end;
    }
}

namespace Syllabification {
    void keepPhonemesOnWordRoots(const QList<NoteInferenceSnapshot> &notes,
                                 QList<PhonemeNameResult> &results) {
        if (notes.size() != results.size())
            return;

        for (int i = 0; i < notes.size(); ++i) {
            if (!isWordContinuationLyric(notes.at(i).lyric))
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
            if (isWordContinuationLyric(lyrics.at(rootIndex))) {
                notes[rootIndex].phonemeNames.clear();
                notes[rootIndex].phonemeOffsets.clear();
                ++rootIndex;
                continue;
            }

            const auto end = wordEnd(lyrics, notes, rootIndex);

            const auto wordLyrics = lyrics.mid(rootIndex, end - rootIndex);
            const auto storedNames = notes.at(rootIndex).phonemeNames;
            const auto storedOffsets = notes.at(rootIndex).phonemeOffsets;
            const auto ranges = phonemeRangesForNotes(wordLyrics, storedNames);
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
            if (isWordContinuationLyric(lyrics.at(rootIndex))) {
                ++rootIndex;
                continue;
            }

            const auto end = wordEnd(lyrics, notes, rootIndex);

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
