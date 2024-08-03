//
// Created by fluty on 2024/2/8.
//

#include "RemoveNoteAction.h"

#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Note.h"

RemoveNoteAction *RemoveNoteAction::build(Note *note, SingingClip *clip) {
    auto a = new RemoveNoteAction;
    a->m_note = note;
    a->m_clip = clip;
    return a;
}
void RemoveNoteAction::execute() {
    m_clip->removeNote(m_note);
    m_clip->notifyNoteChanged(SingingClip::Removed, m_note);
}
void RemoveNoteAction::undo() {
    m_clip->insertNote(m_note);
    m_clip->notifyNoteChanged(SingingClip::Inserted, m_note);
}