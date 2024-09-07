//
// Created by fluty on 2024/2/8.
//

#include "EditNoteStartAndLengthAction.h"

#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Note.h"

void EditNoteStartAndLengthAction::execute() {
    for (const auto &note : m_notes) {
        m_clip->removeNote(note);
        note->setStart(note->start() + m_deltaTick);
        note->setLength(note->length() - m_deltaTick);
        m_clip->insertNote(note);
    }
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, m_notes);
}

void EditNoteStartAndLengthAction::undo() {
    for (const auto &note : m_notes) {
        m_clip->removeNote(note);
        note->setStart(note->start() - m_deltaTick);
        note->setLength(note->length() + m_deltaTick);
        m_clip->insertNote(note);
    }
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, m_notes);
}