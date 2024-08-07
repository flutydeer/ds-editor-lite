//
// Created by fluty on 2024/2/8.
//

#include "InsertNoteAction.h"

#include "Model/AppModel/Clip.h"

InsertNoteAction *InsertNoteAction::build(Note *note, SingingClip *clip) {
    auto a = new InsertNoteAction;
    a->m_note = note;
    a->m_clip = clip;
    return a;
}
void InsertNoteAction::execute() {
    m_clip->insertNote(m_note);
    m_clip->notifyNoteChanged(SingingClip::Inserted, m_note);
}
void InsertNoteAction::undo() {
    m_clip->removeNote(m_note);
    m_clip->notifyNoteChanged(SingingClip::Removed, m_note);
}