#include <lite/ProjectModel/SingingClipSlicer/SingingClipSlicer.h>

#include <lite/ProjectModel/SingingClipSlicer/SingingClipSlicerGlobal.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/Utils/Syllabification.h>

#include <QDebug>

#include <algorithm>

namespace {
    // Returns the syllabification notes that got zero phonemes assigned within
    // their word root groups, plus a syllabification note that would have to
    // root a word group itself
    NoteList unassignedSyllabificationNotes(const NoteList &notes) {
        NoteList result;
        int rootIndex = 0;
        while (rootIndex < notes.size()) {
            const auto root = notes.at(rootIndex);
            if (root->isSyllabification()) {
                result.append(root);
                break;
            }

            QStringList lyrics{root->lyric()};
            auto wordEndTick = root->localStart() + root->length();
            int end = rootIndex + 1;
            for (; end < notes.size(); ++end) {
                const auto note = notes.at(end);
                if ((!note->isSlur() && !note->isSyllabification()) ||
                    note->localStart() > wordEndTick)
                    break;
                lyrics.append(note->lyric());
                wordEndTick = std::max(wordEndTick, note->localStart() + note->length());
            }

            const auto ranges =
                Syllabification::phonemeRangesForNotes(lyrics, root->phonemeNameSeq().result());
            for (int i = 1; i < lyrics.size(); ++i) {
                if (Note::isSyllabificationLyric(lyrics.at(i)) && ranges.at(i).count == 0)
                    result.append(notes.at(rootIndex + i));
            }
            rootIndex = end;
        }
        return result;
    }
}

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
    // Minimum = base amount + header phoneme count (all phonemes before the onset) *
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
    // 2. Rest note (AP/SP), no tail phoneme sequence, padding length = 0
    auto getTailLength = [=](const Note &note) -> double {
        if (isRestNote(note))
            return 0.0;
        return padBaseLength;
    };

    auto addExclusion = [](SliceResult &result, const Note *note, SliceExclusionReason reason) {
        for (const auto &excluded : result.excludedNotes)
            if (excluded.noteId == note->id())
                return;
        result.excludedNotes.append({note->id(), reason});
    };

    SliceResult result;

    // Filter out overlapped notes before processing
    NoteList notes;
    NoteList overlappedNotes;
    for (const auto &note : source) {
        if (note->overlapped()) {
            overlappedNotes.append(note);
        } else {
            notes.append(note);
        }
    }
    for (const auto note : overlappedNotes)
        addExclusion(result, note, SliceExclusionReason::Overlapped);

    if (notes.isEmpty()) {
        qWarning() << "advancedSlice: no valid notes after filtering overlapped notes";
        if (!overlappedNotes.isEmpty()) {
            int minStart = overlappedNotes.first()->localStart();
            int maxEnd = minStart + overlappedNotes.first()->length();
            for (const auto note : overlappedNotes) {
                minStart = std::min(minStart, note->localStart());
                maxEnd = std::max(maxEnd, note->localStart() + note->length());
            }
            result.skippedPhraseRanges.append({minStart, maxEnd});
        }
        return result;
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
            const bool firstNoteIsInvalid = firstNote->isSlur() || firstNote->isSyllabification();

            // Check for missing phoneme info
            bool hasMissingPhonemeInfo = false;
            NoteList missingPhonemeNotes;
            for (const auto &note : buffer) {
                auto isCommonNote =
                    !isRestNote(*note) && !note->isSlur() && !note->isSyllabification();
                if (isCommonNote && note->phonemes().nameSeq.result().isEmpty()) {
                    hasMissingPhonemeInfo = true;
                    missingPhonemeNotes.append(note);
                }
            }

            const auto unassignedSylNotes = unassignedSyllabificationNotes(buffer);

            // Skip phrases with missing phonemes or unassigned continuation notes.
            if (hasMissingPhonemeInfo || firstNoteIsInvalid || !unassignedSylNotes.isEmpty()) {
                for (const auto note : missingPhonemeNotes)
                    addExclusion(result, note, SliceExclusionReason::MissingPhonemes);
                if (firstNoteIsInvalid)
                    addExclusion(result, firstNote, SliceExclusionReason::FirstNoteInvalid);
                for (const auto note : unassignedSylNotes)
                    addExclusion(result, note, SliceExclusionReason::UnassignedSyllabification);

                const auto rangeLast = buffer.last();
                result.skippedPhraseRanges.append(
                    {firstNote->localStart(), rangeLast->localStart() + rangeLast->length()});
                buffer.clear();
                continue;
            }

            segment.notes = buffer;

            segments.append(segment);
            buffer.clear();
        }
    }
    result.segments = segments;
    return result;
}
