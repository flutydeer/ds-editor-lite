#include "EditNoteStartAndLengthAction.h"

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

void EditNoteStartAndLengthAction::execute() {
    const auto previousWordStates =
        SingingClipPhonemeNormalizer::captureWordStates(*m_clip);

    for (const auto &note : m_notes) {
        m_clip->removeNote(note);
        note->setLocalStart(note->localStart() + m_deltaTick);
        note->setLength(note->length() - m_deltaTick);
        m_clip->insertNote(note);
    }
    m_resetRecords =
        SingingClipPhonemeNormalizer::normalizeEditedOffsets(*m_clip, previousWordStates);
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, m_notes);
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}

void EditNoteStartAndLengthAction::undo() {
    for (const auto &note : m_notes) {
        m_clip->removeNote(note);
        note->setLocalStart(note->localStart() - m_deltaTick);
        note->setLength(note->length() + m_deltaTick);
        m_clip->insertNote(note);
    }
    SingingClipPhonemeNormalizer::restoreEditedOffsets(m_resetRecords);
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, m_notes);
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}
