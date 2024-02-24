//
// Created by fluty on 2024/2/8.
//

#include "EditNotePositionAction.h"

#include "Model/Clip.h"
#include "Model/Note.h"

EditNotePositionAction *EditNotePositionAction::build(Note *note, int deltaTick, int deltaKey,
                                                      SingingClip *clip) {
    auto a = new EditNotePositionAction;
    a->m_note = note;
    a->m_deltaTick = deltaTick;
    a->m_deltaKey = deltaKey;
    a->m_clip = clip;
    return a;
}
void EditNotePositionAction::execute() {
    m_clip->removeNoteQuietly(m_note);

    m_note->setStart(m_note->start() + m_deltaTick);
    m_note->setKeyIndex(m_note->keyIndex() + m_deltaKey);

    m_clip->insertNoteQuietly(m_note);
    m_clip->notifyNotePropertyChanged(SingingClip::TimeAndKey, m_note);
}
void EditNotePositionAction::undo() {
    m_clip->removeNoteQuietly(m_note);

    m_note->setStart(m_note->start() - m_deltaTick);
    m_note->setKeyIndex(m_note->keyIndex() - m_deltaKey);

    m_clip->insertNoteQuietly(m_note);
    m_clip->notifyNotePropertyChanged(SingingClip::TimeAndKey, m_note);
}