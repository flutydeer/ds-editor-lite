//
// Created by fluty on 2024/2/8.
//

#include "EditNotesLengthAction.h"

#include "Model/Clip.h"
#include "Model/Note.h"

EditNotesLengthAction *EditNotesLengthAction::build(Note *note, int deltaTick) {
    auto a = new EditNotesLengthAction;
    a->m_note = note;
    a->m_deltaTick = deltaTick;
    return a;
}
void EditNotesLengthAction::execute() {
    m_note->setLength(m_note->length() + m_deltaTick);
    m_note->notifyPropertyChanged(Note::TimeAndKey);
}
void EditNotesLengthAction::undo() {
    m_note->setLength(m_note->length() - m_deltaTick);
    m_note->notifyPropertyChanged(Note::TimeAndKey);
}