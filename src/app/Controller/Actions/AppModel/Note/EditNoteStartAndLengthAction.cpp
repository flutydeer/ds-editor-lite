//
// Created by fluty on 2024/2/8.
//

#include "EditNoteStartAndLengthAction.h"

#include "Model/Clip.h"
#include "Model/Note.h"

EditNoteStartAndLengthAction *EditNoteStartAndLengthAction::build(Note *note, int deltaTick,
                                                                  SingingClip *clip) {
    auto a = new EditNoteStartAndLengthAction;
    a->m_note = note;
    a->m_deltaTick = deltaTick;
    a->m_clip = clip;
    return a;
}
void EditNoteStartAndLengthAction::execute() {
    m_clip->removeNoteQuietly(m_note);

    m_note->setStart(m_note->start() + m_deltaTick);
    m_note->setLength(m_note->length() - m_deltaTick);

    m_clip->insertNoteQuietly(m_note);
    m_note->notifyPropertyChanged(Note::TimeAndKey);
}
void EditNoteStartAndLengthAction::undo() {
    m_clip->removeNoteQuietly(m_note);

    m_note->setStart(m_note->start() - m_deltaTick);
    m_note->setLength(m_note->length() + m_deltaTick);

    m_clip->insertNoteQuietly(m_note);
    m_note->notifyPropertyChanged(Note::TimeAndKey);
}