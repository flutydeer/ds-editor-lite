//
// Created by fluty on 2024/2/8.
//

#include "EditNotesLengthAction.h"

#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"

void EditNotesLengthAction::execute() {
    for (const auto &note : m_notes) {
        m_clip->removeNote(note);
        note->setLength(note->length() + m_deltaTick);
        m_clip->insertNote(note);
    }
    m_resetRecords = SingingClipPhonemeNormalizer::normalizeEditedOffsets(*m_clip);
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, m_notes);
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}

void EditNotesLengthAction::undo() {
    for (const auto &note : m_notes) {
        m_clip->removeNote(note);
        note->setLength(note->length() - m_deltaTick);
        m_clip->insertNote(note);
    }
    SingingClipPhonemeNormalizer::restoreEditedOffsets(m_resetRecords);
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, m_notes);
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}
