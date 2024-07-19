//
// Created by fluty on 2024/2/8.
//

#include "EditNotePositionAction.h"

#include "Model/Clip.h"
#include "Model/Note.h"

EditNotePositionAction *EditNotePositionAction::build(Note *note, int deltaTick, int deltaKey) {
    auto a = new EditNotePositionAction;
    a->m_note = note;
    a->m_deltaTick = deltaTick;
    a->m_deltaKey = deltaKey;
    return a;
}
void EditNotePositionAction::execute() {
    m_note->setStart(m_note->start() + m_deltaTick);
    m_note->setKeyIndex(m_note->keyIndex() + m_deltaKey);
    m_note->notifyPropertyChanged(Note::TimeAndKey);
}
void EditNotePositionAction::undo() {
    m_note->setStart(m_note->start() - m_deltaTick);
    m_note->setKeyIndex(m_note->keyIndex() - m_deltaKey);
    m_note->notifyPropertyChanged(Note::TimeAndKey);
}