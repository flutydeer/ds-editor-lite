#include "InsertNoteAction.h"


#include <lite/ProjectModel/AppModel/SingingClip.h>

void InsertNoteAction::execute() {
    const auto previousWordStates =
        SingingClipPhonemeNormalizer::captureWordStates(*m_clip);
    for (const auto &note : m_notes)
        m_clip->insertNote(note);
    m_resetRecords =
        SingingClipPhonemeNormalizer::normalizeEditedOffsets(*m_clip, previousWordStates);
    m_clip->notifyNoteChanged(SingingClip::Insert, m_notes);
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}

void InsertNoteAction::undo() {
    for (const auto &note : m_notes)
        m_clip->removeNote(note);
    SingingClipPhonemeNormalizer::restoreEditedOffsets(m_resetRecords);
    m_clip->notifyNoteChanged(SingingClip::Remove, m_notes);
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}
