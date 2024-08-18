//
// Created by fluty on 24-2-16.
//

#include "SelectNoteAction.h"

#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Note.h"

SelectNoteAction *SelectNoteAction::build(Note *note, bool selected, SingingClip *clip) {
    auto a = new SelectNoteAction;
    a->m_note = note;
    a->m_oldValue = note->selected();
    a->m_newValue = selected;
    a->m_clip = clip;
    return a;
}

void SelectNoteAction::execute() {
    m_note->setSelected(m_newValue);
    m_clip->notifyNoteSelectionChanged();
}

void SelectNoteAction::undo() {
    m_note->setSelected(m_oldValue);
    m_clip->notifyNoteSelectionChanged();
}