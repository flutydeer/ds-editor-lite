//
// Created by fluty on 2024/2/8.
//

#include "RemoveNoteAction.h"
RemoveNoteAction *RemoveNoteAction::build(Note *note, DsSingingClip *clip) {
    auto a = new RemoveNoteAction;
    a->m_note = note;
    a->m_clip = clip;
    return a;
}
void RemoveNoteAction::execute() {
    m_clip->removeNote(m_note);
}
void RemoveNoteAction::undo() {
    m_clip->insertNote(m_note);
}