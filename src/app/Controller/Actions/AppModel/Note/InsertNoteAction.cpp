//
// Created by fluty on 2024/2/8.
//

#include "InsertNoteAction.h"


#include "Model/AppModel/SingingClip.h"

void InsertNoteAction::execute() {
    for (const auto &note : m_notes)
        m_clip->insertNote(note);
    m_resetRecords = SingingClipPhonemeNormalizer::normalizeEditedOffsets(*m_clip);
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
