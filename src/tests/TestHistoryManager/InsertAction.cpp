//
// Created by fluty on 2024/2/7.
//

#include "InsertAction.h"
#include "Note.h"

InsertAction *InsertAction::build(Note *note, SingingClip *clip) {
    auto action = new InsertAction;
    action->m_note = note;
    action->m_clip = clip;
    return action;
}
void InsertAction::execute() {
    m_clip->notes.append(m_note);
}
void InsertAction::undo() {
    m_clip->notes.removeOne(m_note);
}