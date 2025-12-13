//
// Created for split note functionality
//

#include "SplitNoteAction.h"

#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"

SplitNoteAction::SplitNoteAction(Note *originalNote, Note *newNote, int newLength, SingingClip *clip)
    : m_originalNote(originalNote), m_newNote(newNote), m_originalLength(originalNote->length()),
      m_newLength(newLength), m_clip(clip) {
}

void SplitNoteAction::execute() {
    // Resize original note
    m_clip->removeNote(m_originalNote);
    m_originalNote->setLength(m_newLength);
    m_clip->insertNote(m_originalNote);
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, QList<Note *>{m_originalNote});

    // Insert new note
    m_clip->insertNote(m_newNote);
    m_clip->notifyNoteChanged(SingingClip::Insert, QList<Note *>{m_newNote});
}

void SplitNoteAction::undo() {
    // Remove new note
    m_clip->removeNote(m_newNote);
    m_clip->notifyNoteChanged(SingingClip::Remove, QList<Note *>{m_newNote});

    // Restore original note length
    m_clip->removeNote(m_originalNote);
    m_originalNote->setLength(m_originalLength);
    m_clip->insertNote(m_originalNote);
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, QList<Note *>{m_originalNote});
}

