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

#include <limits>

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

    SingingClipPhonemeNormalizer::WordStates captureWordStates(const SingingClip &clip,
                                                                 const Timeline &timeline) {
        SingingClipPhonemeNormalizer::WordStates result;
        const auto notes = clip.notes().toList();
        for (int i = 0; i < notes.count(); ++i) {
            const auto root = notes.at(i);
            if (!root || root->isSlur() || root->isSyllabification() || root->overlapped())
                continue;

            SingingClipPhonemeNormalizer::WordState state;
            state.rootSyllabificationCount = Note::trailingSyllabificationCount(root->lyric());
            for (const auto &phoneme : root->phonemeNameSeq().result())
                state.rootOnsets.append(phoneme.isOnset);
            const auto rootStartMs =
                timeline.tickToMs(clip.start() + root->localStart());
            auto wordEndTick = root->localStart() + root->length();
            bool hasSyllabification = false;
            int wordEnd = i + 1;
            for (; wordEnd < notes.count(); ++wordEnd) {
                const auto continuationNote = notes.at(wordEnd);
                if (!continuationNote ||
                    (!continuationNote->isSlur() && !continuationNote->isSyllabification()) ||
                    continuationNote->overlapped())
                    break;
                if (continuationNote->localStart() > wordEndTick)
                    break;

                const auto continuationStartMs =
                    timeline.tickToMs(clip.start() + continuationNote->localStart());
                state.members.append({continuationNote, continuationNote->lyric().trimmed(),
                                      qRound(continuationStartMs - rootStartMs)});
                hasSyllabification =
                    hasSyllabification || continuationNote->isSyllabification();
                wordEndTick = std::max(
                    wordEndTick, continuationNote->localStart() + continuationNote->length());
            }

            if (hasSyllabification)
                result.insert(root, state);
            i = wordEnd - 1;
        }
        return result;
    }

    void appendChangedWordRoots(const SingingClip &clip,
                                 const SingingClipPhonemeNormalizer::WordStates &previousStates,
                                 const SingingClipPhonemeNormalizer::WordStates &currentStates,
                                 QList<Note *> &result, QSet<Note *> &resultSet) {
        for (const auto root : clip.notes()) {
            if (!root || !root->phonemeOffsetSeq().isEdited())
                continue;
            const auto previous = previousStates.constFind(root);
            const auto current = currentStates.constFind(root);
            if (previous == previousStates.cend() && current == currentStates.cend())
                continue;
            if (previous == previousStates.cend() || current == currentStates.cend() ||
                previous.value() != current.value())
                appendUnique(result, resultSet, root);
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

    // Word span in global ticks. Mirrors PhonemeView::buildPhonemeList: a word is
    // its root note plus the slur / syllabification member chain that overlaps.
    class WordSpan {
    public:
        Note *root = nullptr;
        int startTick = 0;
        int endTick = 0;
    };

    QList<WordSpan> buildWordSpans(const QList<Note *> &notes) {
        QList<WordSpan> result;
        for (int i = 0; i < notes.count(); ++i) {
            const auto root = notes.at(i);
            if (!root || root->isSlur() || root->isSyllabification() || root->overlapped())
                continue;
            WordSpan span;
            span.root = root;
            span.startTick = root->globalStart();
            span.endTick = span.startTick + root->length();
            for (int j = i + 1; j < notes.count(); ++j) {
                const auto next = notes.at(j);
                if (!next || (!next->isSlur() && !next->isSyllabification()) ||
                    next->overlapped())
                    break;
                const auto nextStart = next->globalStart();
                if (nextStart > span.endTick)
                    break;
                span.endTick = std::max(span.endTick, nextStart + next->length());
            }
            result.append(span);
        }
        return result;
    }

    QList<Note *> collectCascadeResetRootsWithTimeline(SingingClip &clip,
                                                       const QList<Note *> &selectedRoots,
                                                       const Timeline &timeline) {
        const auto notes = clip.notes().toList();
        const auto spans = buildWordSpans(notes);
        if (spans.isEmpty())
            return {};

        QSet<Note *> closedSet;
        QList<Note *> result;
        const auto appendUnique = [&](Note *root) {
            if (closedSet.contains(root) || !root)
                return;
            closedSet.insert(root);
            result.append(root);
        };

        // Seed with the caller's word roots (must also hold the normalizer's
        // invariants: not rest/overlapped and have a baseline to restore to).
        for (const auto root : selectedRoots) {
            if (!root || root->overlapped() || !root->canEditPhonemes())
                continue;
            const auto &offsets = root->phonemeOffsetSeq();
            if (offsets.original.isEmpty())
                continue;
            appendUnique(root);
        }

        // Restored "last phoneme start tick" and current edited "first phoneme
        // start tick" helpers; empty baseline returns max() so it never triggers.
        const auto restoredLastStartTick = [&](const Note *root) {
            const auto &offsets = root->phonemeOffsetSeq().original;
            if (offsets.isEmpty())
                return std::numeric_limits<int>::max();
            const auto rootMs = timeline.tickToMs(root->globalStart());
            return qRound(timeline.msToTick(rootMs + offsets.last()));
        };
        const auto editedFirstStartTick = [&](const Note *root) {
            const auto &offsets = root->phonemeOffsetSeq().edited;
            if (offsets.isEmpty())
                return std::numeric_limits<int>::max();
            const auto rootMs = timeline.tickToMs(root->globalStart());
            return qRound(timeline.msToTick(rootMs + offsets.first()));
        };

        // Rightward cascade: resetting A may leave B (edited) overlapping A's
        // restored last phoneme. Each word participates at most once -> converges.
        bool changed = true;
        while (changed) {
            changed = false;
            for (int i = 0; i + 1 < spans.count(); ++i) {
                const auto *left = &spans.at(i);
                if (!closedSet.contains(left->root))
                    continue;
                for (int j = i + 1; j < spans.count(); ++j) {
                    const auto *right = &spans.at(j);
                    if (closedSet.contains(right->root))
                        break; // already reset, chain is closed on the right
                    if (!right->root->canEditPhonemes())
                        break; // non-editable word boundary stops the cascade
                    const auto &offsets = right->root->phonemeOffsetSeq();
                    if (!offsets.isEdited() || offsets.original.isEmpty() ||
                        offsets.edited.isEmpty())
                        break; // nothing to reset (or no baseline) stops the cascade
                    if (editedFirstStartTick(right->root) >=
                        restoredLastStartTick(left->root))
                        break; // no overlap -> stop
                    appendUnique(right->root);
                    changed = true;
                    break; // re-scan whole clip so the next neighbor is re-evaluated
                }
            }
        }
        return result;
    }

    QList<Note *> collectInvalidEditedOffsetNotes(
        SingingClip &clip, const Timeline &timeline,
        const SingingClipPhonemeNormalizer::WordStates *previousWordStates) {
        QList<Note *> result;
        QSet<Note *> resultSet;

        for (const auto note : clip.notes()) {
            if (hasInvalidOffsetCount(note) || hasUnorderedOffsets(note))
                appendUnique(result, resultSet, note);
        }

        if (previousWordStates) {
            const auto currentWordStates = captureWordStates(clip, timeline);
            appendChangedWordRoots(clip, *previousWordStates, currentWordStates, result,
                                    resultSet);
        }

        const auto sliceResult = SingingClipSlicer::slice(timeline, clip.notes().toList());
        for (const auto &segment : sliceResult.segments) {
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
                                           const SingingClipPhonemeNormalizer::WordStates
                                               *previousWordStates = nullptr) {
        QList<SingingClipPhonemeNormalizer::ResetRecord> records;
        const auto notes = collectInvalidEditedOffsetNotes(clip, timeline, previousWordStates);
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

SingingClipPhonemeNormalizer::WordStates
    SingingClipPhonemeNormalizer::captureWordStates(const SingingClip &clip) {
    return ::captureWordStates(clip, appModel->timeline());
}

SingingClipPhonemeNormalizer::WordStates
    SingingClipPhonemeNormalizer::captureWordStates(const SingingClip &clip,
                                                     const Timeline &timeline) {
    return ::captureWordStates(clip, timeline);
}

QList<Note *> SingingClipPhonemeNormalizer::collectInvalidEditedOffsetNotes(SingingClip &clip) {
    return ::collectInvalidEditedOffsetNotes(clip, appModel->timeline(), nullptr);
}

QList<SingingClipPhonemeNormalizer::ResetRecord>
    SingingClipPhonemeNormalizer::normalizeEditedOffsets(SingingClip &clip) {
    return normalizeEditedOffsetsWithTimeline(clip, appModel->timeline());
}

QList<SingingClipPhonemeNormalizer::ResetRecord>
    SingingClipPhonemeNormalizer::normalizeEditedOffsets(
        SingingClip &clip, const WordStates &previousWordStates) {
    return normalizeEditedOffsetsWithTimeline(clip, appModel->timeline(), &previousWordStates);
}

QList<SingingClipPhonemeNormalizer::ResetRecord>
    SingingClipPhonemeNormalizer::normalizeEditedOffsets(
        SingingClip &clip, const WordStates &previousWordStates, const Timeline &timeline) {
    return normalizeEditedOffsetsWithTimeline(clip, timeline, &previousWordStates);
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

QList<Note *> SingingClipPhonemeNormalizer::collectCascadeResetRoots(
    SingingClip &clip, const QList<Note *> &selectedRoots, const Timeline &timeline) {
    return ::collectCascadeResetRootsWithTimeline(clip, selectedRoots, timeline);
}

QList<Note *> SingingClipPhonemeNormalizer::collectCascadeResetRoots(
    SingingClip &clip, const QList<Note *> &selectedRoots) {
    return ::collectCascadeResetRootsWithTimeline(clip, selectedRoots, appModel->timeline());
}
