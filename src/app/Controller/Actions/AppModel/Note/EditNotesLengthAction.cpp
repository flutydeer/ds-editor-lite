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
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, m_notes);
}

void EditNotesLengthAction::undo() {
    for (const auto &note : m_notes) {
        m_clip->removeNote(note);
        note->setLength(note->length() - m_deltaTick);
        m_clip->insertNote(note);
    }
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, m_notes);
}