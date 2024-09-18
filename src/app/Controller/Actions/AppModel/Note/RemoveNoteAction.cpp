//
// Created by fluty on 2024/2/8.
//

#include "RemoveNoteAction.h"

#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"

void RemoveNoteAction::execute() {
    for (const auto &note : m_notes)
        m_clip->removeNote(note);
    m_clip->notifyNoteChanged(SingingClip::Remove, m_notes);
}

void RemoveNoteAction::undo() {
    for (const auto &note : m_notes)
        m_clip->insertNote(note);
    m_clip->notifyNoteChanged(SingingClip::Insert, m_notes);
}