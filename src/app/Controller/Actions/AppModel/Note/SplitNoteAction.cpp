//
// Created for split note functionality
//

#include "SplitNoteAction.h"

#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"

SplitNoteAction::SplitNoteAction(Note *originalNote, Note *newNote, int newLength,
                                 SingingClip *clip)
    : m_originalNote(originalNote), m_newNote(newNote), m_originalLength(originalNote->length()),
      m_newLength(newLength), m_clip(clip) {
}

void SplitNoteAction::execute() {
    m_clip->removeNote(m_originalNote);
    m_originalNote->setLength(m_newLength);
    m_clip->insertNote(m_originalNote);

    m_clip->insertNote(m_newNote);
    m_resetRecords = SingingClipPhonemeNormalizer::normalizeEditedOffsets(*m_clip);
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, QList<Note *>{m_originalNote});
    m_clip->notifyNoteChanged(SingingClip::Insert, QList<Note *>{m_newNote});
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}

void SplitNoteAction::undo() {
    m_clip->removeNote(m_newNote);

    m_clip->removeNote(m_originalNote);
    m_originalNote->setLength(m_originalLength);
    m_clip->insertNote(m_originalNote);

    SingingClipPhonemeNormalizer::restoreEditedOffsets(m_resetRecords);
    m_clip->notifyNoteChanged(SingingClip::Remove, QList<Note *>{m_newNote});
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, QList<Note *>{m_originalNote});
    if (!m_resetRecords.isEmpty())
        m_clip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
}
