//
// Created by fluty on 24-9-11.
//

#include "EditPhonemeOffsetAction.h"

#include "Model/AppModel/SingingClip.h"

EditPhonemeOffsetAction::EditPhonemeOffsetAction(Note *note, const QList<int> &offsets,
                                                 SingingClip *clip)
    : m_note(note), m_newOffsets(offsets), m_clip(clip) {
    m_oldOffsets = m_note->phonemeOffsetSeq().edited;
    
}

void EditPhonemeOffsetAction::execute() {
    m_note->setPhonemeOffsetSeq(Note::Edited, m_newOffsets);
    m_clip->notifyNoteChanged(SingingClip::EditedPhonemeOffsetChange, {m_note});
}

void EditPhonemeOffsetAction::undo() {
    m_note->setPhonemeOffsetSeq(Note::Edited, m_oldOffsets);
    m_clip->notifyNoteChanged(SingingClip::EditedPhonemeOffsetChange, {m_note});
}