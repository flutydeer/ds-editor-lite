#include "SingingClipPhonemeNormalizer.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/Phonemes.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/SingingClipSlicer/SingingClipSlicer.h>
#include <lite/ProjectModel/Utils/PhonemeHeadLayout.h>

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

    bool hasUnorderedOffsets(Note *note) {
        if (!note || !note->phonemeOffsetSeq().isEdited())
            return false;
        const auto &offsets = note->phonemeOffsetSeq().edited;
        return !std::is_sorted(offsets.cbegin(), offsets.cend());
    }

    void appendRelativeTimingAffectedRoots(const QList<Note *> &notes,
                                           const QHash<Note *, int> &startDeltas,
                                           QList<Note *> &result, QSet<Note *> &resultSet) {
        if (startDeltas.isEmpty())
            return;

        for (int i = 0; i < notes.count(); ++i) {
            const auto root = notes.at(i);
            if (!root || root->isSlur() || root->isPlus())
                continue;

            const auto rootDelta = startDeltas.value(root);
            bool hasPlus = false;
            bool relativeTimingChanged = false;
            int groupEnd = i + 1;
            for (; groupEnd < notes.count(); ++groupEnd) {
                const auto marker = notes.at(groupEnd);
                if (!marker || (!marker->isSlur() && !marker->isPlus()))
                    break;
                hasPlus = hasPlus || marker->isPlus();
                relativeTimingChanged =
                    relativeTimingChanged || startDeltas.value(marker) != rootDelta;
            }

            if (hasPlus && relativeTimingChanged && root->phonemeOffsetSeq().isEdited())
                appendUnique(result, resultSet, root);
            i = groupEnd - 1;
        }
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

    bool editedOffsetExceedsLeftBoundary(const EffectiveNote &effective,
                                         const int leftBoundaryGlobalTick,
                                         const int clipStartTick, const Timeline &timeline) {
        const auto note = effective.note;
        if (!note || isRestNote(*note) || !note->phonemeOffsetSeq().isEdited())
            return false;
        const auto &offsets = note->phonemeOffsetSeq().edited;
        if (offsets.isEmpty())
            return false;

        const auto noteStartMs = timeline.tickToMs(clipStartTick + effective.start);
        const auto earliestStartTick =
            qCeil(timeline.msToTick(noteStartMs + minimumEditedOffset(*note)));
        return earliestStartTick < leftBoundaryGlobalTick;
    }

    QList<Note *> collectInvalidEditedOffsetNotes(SingingClip &clip, const Timeline &timeline,
                                                  const QHash<Note *, int> &startDeltas) {
        QList<Note *> result;
        QSet<Note *> resultSet;

        for (const auto note : clip.notes()) {
            if (hasInvalidOffsetCount(note) || hasUnorderedOffsets(note))
                appendUnique(result, resultSet, note);
        }

        const auto sliceResult = SingingClipSlicer::slice(timeline, clip.notes().toList());
        for (const auto &segment : sliceResult.segments) {
            appendRelativeTimingAffectedRoots(segment.notes, startDeltas, result, resultSet);
            const auto effectiveNotes = buildEffectiveNotes(segment.notes);
            for (int i = 0; i < effectiveNotes.count(); ++i) {
                const auto &effective = effectiveNotes.at(i);
                const auto note = effective.note;
                if (!note || resultSet.contains(note) || !note->phonemeOffsetSeq().isEdited())
                    continue;

                if (i == 0) {
                    const auto headLayout =
                        PhonemeHeadLayout::calculate(segment.paddingStartMs,
                                                     segment.headAvailableLengthMs,
                                                     note->phonemeOffsetSeq().edited);
                    if (!headLayout.isWithinBounds())
                        appendUnique(result, resultSet, note);
                    continue;
                }

                const auto &previous = effectiveNotes.at(i - 1);
                const auto leftBoundary =
                    clip.start() +
                    (previous.end < effective.start ? previous.end : previous.start);
                if (editedOffsetExceedsLeftBoundary(effective, leftBoundary, clip.start(),
                                                    timeline))
                    appendUnique(result, resultSet, note);
            }
        }

        return result;
    }

    QList<SingingClipPhonemeNormalizer::ResetRecord>
        normalizeEditedOffsetsWithTimeline(SingingClip &clip, const Timeline &timeline,
                                           const QHash<Note *, int> &startDeltas = {}) {
        QList<SingingClipPhonemeNormalizer::ResetRecord> records;
        const auto notes = collectInvalidEditedOffsetNotes(clip, timeline, startDeltas);
        for (const auto note : notes) {
            if (!note || !note->phonemeOffsetSeq().isEdited())
                continue;

            SingingClipPhonemeNormalizer::ResetRecord record;
            record.note = note;
            record.editedOffsets = note->phonemeOffsetSeq().edited;
            records.append(record);
            note->setPhonemeOffsetSeq(Note::Edited, {});
        }
        return records;
    }
}

QList<Note *> SingingClipPhonemeNormalizer::collectInvalidEditedOffsetNotes(SingingClip &clip) {
    return ::collectInvalidEditedOffsetNotes(clip, appModel->timeline(), {});
}

QList<SingingClipPhonemeNormalizer::ResetRecord>
    SingingClipPhonemeNormalizer::normalizeEditedOffsets(SingingClip &clip) {
    return normalizeEditedOffsetsWithTimeline(clip, appModel->timeline());
}

QList<SingingClipPhonemeNormalizer::ResetRecord>
    SingingClipPhonemeNormalizer::normalizeEditedOffsets(
        SingingClip &clip, const QHash<Note *, int> &startDeltas) {
    return normalizeEditedOffsetsWithTimeline(clip, appModel->timeline(), startDeltas);
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
    const auto &timeline = model.timeline();
    for (const auto track : model.tracks()) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() == IClip::Singing)
                normalizeEditedOffsetsWithTimeline(*static_cast<SingingClip *>(clip), timeline);
        }
    }
}
