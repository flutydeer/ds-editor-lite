//
// Created by fluty on 2024/2/8.
//

#include "EditNotesLengthAction.h"

#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Note.h"

EditNotesLengthAction *EditNotesLengthAction::build(Note *note, int deltaTick, SingingClip *clip) {
    auto a = new EditNotesLengthAction;
    a->m_note = note;
    a->m_deltaTick = deltaTick;
    a->m_clip = clip;
    return a;
}
void EditNotesLengthAction::execute() {
    m_clip->removeNote(m_note);
    m_note->setLength(m_note->length() + m_deltaTick);
    m_clip->insertNote(m_note);
    m_note->notifyPropertyChanged(Note::TimeAndKey);
}
void EditNotesLengthAction::undo() {
    m_clip->removeNote(m_note);
    m_note->setLength(m_note->length() - m_deltaTick);
    m_clip->insertNote(m_note);
    m_note->notifyPropertyChanged(Note::TimeAndKey);
}