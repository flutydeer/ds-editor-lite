#include "SingingClipPhonemeNormalizer.h"

#include "AppModel.h"
#include "Clip.h"
#include "Note.h"
#include "Phonemes.h"
#include "SingingClip.h"
#include "Timeline.h"
#include "Track.h"
#include "Modules/SingingClipSlicer/SingingClipSlicer.h"

#include <algorithm>

#include <QSet>

namespace {
    bool isRestNote(const Note &note) {
        const auto lyric = note.lyric().trimmed();
        return lyric == "AP" || lyric == "SP";
    }

    class EffectiveNote {
    public:
        Note *note = nullptr;
        int start = 0;
        int end = 0;
    };

    void appendUnique(QList<Note *> &notes, QSet<Note *> &noteSet, Note *note) {
        if (!note || noteSet.contains(note))
            return;
        notes.append(note);
        noteSet.insert(note);
    }

    bool hasInvalidOffsetCount(Note *note) {
        if (!note || !note->phonemeOffsetSeq().isEdited())
            return false;
        return note->phonemeOffsetSeq().edited.count() != note->phonemeNameSeq().result().count();
    }

    double minimumEditedOffset(const Note &note) {
        const auto &offsets = note.phonemeOffsetSeq().edited;
        return *std::min_element(offsets.cbegin(), offsets.cend());
    }

    QList<EffectiveNote> buildEffectiveNotes(const QList<Note *> &notes) {
        QList<EffectiveNote> result;
        for (int i = 0; i < notes.count(); ++i) {
            const auto note = notes.at(i);
            if (!note || note->isSlur() || note->overlapped())
                continue;

            EffectiveNote effective;
            effective.note = note;
            effective.start = note->localStart();
            effective.end = note->localStart() + note->length();

            for (int j = i + 1; j < notes.count(); ++j) {
                const auto nextNote = notes.at(j);
                if (!nextNote || !nextNote->isSlur() || nextNote->overlapped())
                    break;
                const auto nextStart = nextNote->localStart();
                if (nextStart > effective.end)
                    break;
                effective.end = nextStart + nextNote->length();
            }
            result.append(effective);
        }
        return result;
    }

    bool editedOffsetExceedsLeftBoundary(const EffectiveNote &effective, double leftBoundary) {
        const auto note = effective.note;
        if (!note || isRestNote(*note) || !note->phonemeOffsetSeq().isEdited())
            return false;
        const auto &offsets = note->phonemeOffsetSeq().edited;
        if (offsets.isEmpty())
            return false;

        const auto earliestStart = effective.start + appModel->msToTick(minimumEditedOffset(*note));
        return earliestStart < leftBoundary;
    }
}

QList<Note *> SingingClipPhonemeNormalizer::collectInvalidEditedOffsetNotes(SingingClip &clip) {
    QList<Note *> result;
    QSet<Note *> resultSet;

    for (const auto note : clip.notes()) {
        if (hasInvalidOffsetCount(note))
            appendUnique(result, resultSet, note);
    }

    const Timeline timeline{{{0, appModel->tempo()}}};
    const auto sliceResult = SingingClipSlicer::slice(timeline, clip.notes().toList());
    for (const auto &segment : sliceResult.segments) {
        const auto effectiveNotes = buildEffectiveNotes(segment.notes);
        for (int i = 0; i < effectiveNotes.count(); ++i) {
            const auto &effective = effectiveNotes.at(i);
            const auto note = effective.note;
            if (!note || resultSet.contains(note) || !note->phonemeOffsetSeq().isEdited())
                continue;

            double leftBoundary = effective.start;
            if (i == 0) {
                const auto availableExtraHeadMs =
                    std::max(0.0, segment.headAvailableLengthMs - segment.paddingStartMs);
                leftBoundary = effective.start - appModel->msToTick(availableExtraHeadMs);
            } else {
                const auto &previous = effectiveNotes.at(i - 1);
                leftBoundary = previous.end < effective.start ? previous.end : previous.start;
            }

            if (editedOffsetExceedsLeftBoundary(effective, leftBoundary))
                appendUnique(result, resultSet, note);
        }
    }

    return result;
}

QList<SingingClipPhonemeNormalizer::ResetRecord>
    SingingClipPhonemeNormalizer::normalizeEditedOffsets(SingingClip &clip) {
    QList<ResetRecord> records;
    const auto notes = collectInvalidEditedOffsetNotes(clip);
    for (const auto note : notes) {
        if (!note || !note->phonemeOffsetSeq().isEdited())
            continue;

        ResetRecord record;
        record.note = note;
        record.editedOffsets = note->phonemeOffsetSeq().edited;
        records.append(record);
        note->setPhonemeOffsetSeq(Note::Edited, {});
    }
    return records;
}

void SingingClipPhonemeNormalizer::restoreEditedOffsets(const QList<ResetRecord> &records) {
    for (const auto &record : records) {
        if (!record.note)
            continue;
        record.note->setPhonemeOffsetSeq(Note::Edited, record.editedOffsets);
    }
}

QList<Note *>
    SingingClipPhonemeNormalizer::notesFromResetRecords(const QList<ResetRecord> &records) {
    QList<Note *> notes;
    for (const auto &record : records) {
        if (record.note)
            notes.append(record.note);
    }
    return notes;
}

void SingingClipPhonemeNormalizer::normalizeEditedOffsets(AppModel &model) {
    for (const auto track : model.tracks()) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() == IClip::Singing)
                normalizeEditedOffsets(*static_cast<SingingClip *>(clip));
        }
    }
}
