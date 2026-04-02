//
// Created by FlutyDeer on 2025/11/27.
//

#include "SingingClipSlicer.h"

#include "Global/SingingClipSlicerGlobal.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/Timeline.h"

#include <QDebug>

SliceResult SingingClipSlicer::slice(const Timeline &timeline, const NoteList &source) {
    // Slice options
    auto headerLengthMax = SingingClipSlicerGlobal::headerAvailableLengthMax;
    auto padBaseLength = SingingClipSlicerGlobal::padBaseLength;
    auto padUnitAdditionalLength = SingingClipSlicerGlobal::padUnitAdditionalLength;

    if (source.isEmpty()) {
        qWarning() << "advancedSlice: source is empty";
        return {};
    }

    auto isRestNote = [](const Note &note) {
        const auto lyric = note.lyric().trimmed();
        return lyric == "AP" || lyric == "SP";
    };

    // Calculate minimum available header length based on note, two cases:
    //
    // 1. Non-rest note (AP/SP), needs SP note padding
    // Note:  |          SP          |        Lyric           |
    // Phone: | SP | ph1 | ... | phn | (onset ph) |    ...    |
    // Minimum = base amount + header phoneme count (all phonemes before the beat point) *
    // additional base
    //
    // If the note has no header phonemes, the minimum degrades to base amount
    // Note:  | SP |        Lyric           |
    // Phone: | SP | (onset ph) |    ...    |
    //
    // 2. Rest note (AP/SP), no SP padding needed
    // Note:  |      AP      |        Lyric           |
    // Phone: |      AP      | (onset ph) |    ...    |
    // Minimum = 0
    //
    // TODO: Update the method to get header phoneme count when refactoring phonemes
    auto getHeaderMinLength = [=](const Note &note) -> double {
        if (isRestNote(note))
            return 0.0;

        qsizetype headerPhonemeCount = 0;
        for (int i = 0; i < note.phonemes().nameSeq.result().count(); i++) {
            auto phonemeName = note.phonemes().nameSeq.result().at(i);
            if (phonemeName.isOnset)
                break;
            headerPhonemeCount++;
        }
        return padBaseLength + static_cast<double>(headerPhonemeCount) * padUnitAdditionalLength;
    };

    // Calculate tail padding length based on note, two cases:
    // 1. Non-rest note (AP/SP), needs SP note padding
    // Padding length = base amount
    // 2. Rest note (AP/SP), no SP padding needed
    // Tail phoneme sequence is empty, padding length is 0
    auto getTailLength = [=](const Note &note) -> double {
        if (isRestNote(note))
            return 0.0;
        return padBaseLength;
    };

    // Filter out overlapped notes before processing
    NoteList notes;
    for (const auto &note : source) {
        if (!note->overlapped()) {
            notes.append(note);
        }
    }

    if (notes.isEmpty()) {
        qWarning() << "advancedSlice: no valid notes after filtering overlapped notes";
        return {};
    }

    QList<Segment> segments;
    double lastTailEndInMs = 0;

    NoteList buffer;
    for (int i = 0; i < notes.count(); i++) {
        const auto curNote = notes.at(i);
        buffer.append(curNote);
        bool commitFlag = false;
        if (i < notes.count() - 1) {
            // Next note's header start time = note start time - note header minimum available
            // length
            const auto nextNote = notes.at(i + 1);
            const auto nextStartInMs = timeline.tickToMs(nextNote->globalStart());
            const auto nextHeaderStartInMs = nextStartInMs - getHeaderMinLength(*nextNote);

            // Current note's tail end time
            const auto curEndInMs = timeline.tickToMs(curNote->globalStart() + curNote->length());
            const auto curTailEndInMs = curEndInMs + getTailLength(*curNote);
            commitFlag = nextHeaderStartInMs > curTailEndInMs;
        } else if (i == notes.count() - 1)
            commitFlag = true;
        if (commitFlag) {
            Segment segment;
            const auto firstNote = buffer.first();
            const auto firstStartInMs = timeline.tickToMs(firstNote->globalStart());

            // Calculate header available length
            // If the gap between current segment and previous segment is long enough, use max
            // value; otherwise use actual available length
            const auto gap = firstStartInMs - lastTailEndInMs;
            segment.headAvailableLengthMs = gap > headerLengthMax ? headerLengthMax : gap;

            // Calculate header and tail padding length
            if (const auto headerMinLength = getHeaderMinLength(*firstNote); headerMinLength > 0) {
                const auto headStartInMs = firstStartInMs - headerMinLength;
                segment.paddingStartMs = firstStartInMs - headStartInMs;
            }

            const auto last = buffer.last();
            const auto lastEndInMs = timeline.tickToMs(last->globalStart() + last->length());
            const auto tailLength = getTailLength(*last);
            const auto tailEndInMs = lastEndInMs + tailLength;
            lastTailEndInMs = tailEndInMs;
            if (tailLength > 0)
                segment.paddingEndMs = tailEndInMs - lastEndInMs;

            // Check if the complete phrase (buffer) has any note missing phoneme name info
            // or if the first note of the phrase is a slur
            bool hasMissingPhonemeInfo = false;
            bool firstNoteIsSlur = false;

            // Check if first note is slur
            if (!buffer.isEmpty()) {
                const auto firstNote = buffer.first();
                firstNoteIsSlur = firstNote->isSlur();
            }

            // Check for missing phoneme info
            for (const auto &note : buffer) {
                auto isCommonNote = !isRestNote(*note) && !note->isSlur();
                if (isCommonNote && note->phonemes().nameSeq.result().isEmpty()) {
                    hasMissingPhonemeInfo = true;
                    break;
                }
            }

            // Skip the segment if the complete phrase has missing phoneme info
            // or if the first note is a slur
            // TODO: Mark as error segment?
            if (hasMissingPhonemeInfo || firstNoteIsSlur) {
                buffer.clear();
                continue;
            }

            segment.notes = buffer;

            segments.append(segment);
            buffer.clear();
        }
    }
    return {segments};
}

// SliceResult SingingClipSlicer::simpleSlice(const NoteList &source, double threshold) {
//     QList<NoteList> target;
//     if (source.isEmpty()) {
//         qWarning() << "simpleSlice: source is empty";
//         return {};
//     }
//
//     if (!target.isEmpty()) {
//         target.clear();
//         qWarning() << "simpleSlice: target is not empty, cleared";
//     }
//
//     NoteList buffer;
//     for (int i = 0; i < source.count(); i++) {
//         const auto note = source.at(i);
//         buffer.append(note);
//         bool commitFlag = false;
//         if (i < source.count() - 1) {
//             const auto nextStartInMs = appModel->tickToMs(source.at(i + 1)->globalStart());
//             const auto curEndInMs = appModel->tickToMs(note->globalStart() + note->length());
//             commitFlag = nextStartInMs - curEndInMs > threshold;
//         } else if (i == source.count() - 1)
//             commitFlag = true;
//         if (commitFlag) {
//             target.append(buffer);
//             buffer.clear();
//         }
//     }
//     return target;
// }