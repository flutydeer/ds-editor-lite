//
// Created by fluty on 2024/2/8.
//

#include "InsertNoteAction.h"

#include "Model/AppModel/Clip.h"

void InsertNoteAction::execute() {
    for (const auto &note : m_notes)
        m_clip->insertNote(note);
    m_clip->notifyNoteChanged(SingingClip::Insert, m_notes);
}

void InsertNoteAction::undo() {
    for (const auto &note : m_notes)
        m_clip->removeNote(note);
    m_clip->notifyNoteChanged(SingingClip::Remove, m_notes);
}