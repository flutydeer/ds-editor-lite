//
// Created by fluty on 2024/2/8.
//

#include "EditNotePositionAction.h"

#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"

void EditNotePositionAction::execute() {
    for (const auto &note : m_notes) {
        m_clip->removeNote(note);
        note->setRStart(note->rStart() + m_deltaTick);
        note->setKeyIndex(note->keyIndex() + m_deltaKey);
        m_clip->insertNote(note);
    }
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, m_notes);
}

void EditNotePositionAction::undo() {
    for (const auto &note : m_notes) {
        m_clip->removeNote(note);
        note->setRStart(note->rStart() - m_deltaTick);
        note->setKeyIndex(note->keyIndex() - m_deltaKey);
        m_clip->insertNote(note);
    }
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, m_notes);
}