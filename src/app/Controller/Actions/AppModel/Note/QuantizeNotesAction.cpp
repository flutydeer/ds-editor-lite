#include "QuantizeNotesAction.h"

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

void QuantizeNotesAction::execute() {
    const auto previousWordStates = SingingClipPhonemeNormalizer::captureWordStates(*m_clip);

    for (const auto &change : m_changes) {
        m_clip->removeNote(change.note);
        change.note->setLocalStart(change.newStart);
        change.note->setLength(change.newLength);
        m_clip->insertNote(change.note);
    }
    m_resetRecords =
        SingingClipPhonemeNormalizer::normalizeEditedOffsets(*m_clip, previousWordStates);
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, notes());
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}

void QuantizeNotesAction::undo() {
    for (const auto &change : m_changes) {
        m_clip->removeNote(change.note);
        change.note->setLocalStart(change.oldStart);
        change.note->setLength(change.oldLength);
        m_clip->insertNote(change.note);
    }
    SingingClipPhonemeNormalizer::restoreEditedOffsets(m_resetRecords);
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, notes());
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}

QList<Note *> QuantizeNotesAction::notes() const {
    QList<Note *> result;
    result.reserve(m_changes.size());
    for (const auto &change : m_changes)
        result.append(change.note);
    return result;
}
