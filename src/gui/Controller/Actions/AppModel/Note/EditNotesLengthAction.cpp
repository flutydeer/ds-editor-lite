//
// Created by fluty on 2024/2/8.
//

#include "EditNotesLengthAction.h"
EditNotesLengthAction *EditNotesLengthAction::build(Note *note, int deltaTick,
                                                       SingingClip *clip) {
    auto a = new EditNotesLengthAction;
    a->m_note = note;
    a->m_deltaTick = deltaTick;
    a->m_clip = clip;
    return a;
}
void EditNotesLengthAction::execute() {
    m_clip->removeNoteQuietly(m_note);

    m_note->setLength(m_note->length() + m_deltaTick);

    m_clip->insertNoteQuietly(m_note);
    m_clip->notifyNotePropertyChanged(SingingClip::TimeAndKey, m_note);
}
void EditNotesLengthAction::undo() {
    m_clip->removeNoteQuietly(m_note);

    m_note->setLength(m_note->length() - m_deltaTick);

    m_clip->insertNoteQuietly(m_note);
    m_clip->notifyNotePropertyChanged(SingingClip::TimeAndKey, m_note);
}