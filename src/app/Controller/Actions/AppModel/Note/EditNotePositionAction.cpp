//
// Created by fluty on 2024/2/8.
//

#include "EditNotePositionAction.h"

#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Note.h"

void EditNotePositionAction::execute() {
    for (const auto &note : m_notes) {
        m_clip->removeNote(note);
        note->setStart(note->start() + m_deltaTick);
        note->setKeyIndex(note->keyIndex() + m_deltaKey);
        m_clip->insertNote(note);
    }
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, m_notes);
}

void EditNotePositionAction::undo() {
    for (const auto &note : m_notes) {
        m_clip->removeNote(note);
        note->setStart(note->start() - m_deltaTick);
        note->setKeyIndex(note->keyIndex() - m_deltaKey);
        m_clip->insertNote(note);
    }
    m_clip->notifyNoteChanged(SingingClip::TimeKeyPropertyChange, m_notes);
}