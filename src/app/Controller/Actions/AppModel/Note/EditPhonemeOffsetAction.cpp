//
// Created by fluty on 24-9-11.
//

#include "EditPhonemeOffsetAction.h"

#include "Model/AppModel/SingingClip.h"

EditPhonemeOffsetAction::EditPhonemeOffsetAction(Note *note, Phonemes::Type type,
                                                 const QList<int> &offsets, SingingClip *clip)
    : m_note(note), m_type(type), m_newOffsets(offsets), m_clip(clip) {
    if (type == Phonemes::Ahead)
        m_oldOffsets = m_note->phonemeOffsetInfo().ahead.edited;
    else if (type == Phonemes::Normal)
        m_oldOffsets = m_note->phonemeOffsetInfo().normal.edited;
    else if (type == Phonemes::Final)
        m_oldOffsets = m_note->phonemeOffsetInfo().final.edited;
}

void EditPhonemeOffsetAction::execute() {
    m_note->setPhonemeOffsetInfo(m_type, Note::Edited, m_newOffsets);
    m_clip->notifyNoteChanged(SingingClip::EditedPhonemeOffsetChange, {m_note});
}

void EditPhonemeOffsetAction::undo() {
    m_note->setPhonemeOffsetInfo(m_type, Note::Edited, m_oldOffsets);
    m_clip->notifyNoteChanged(SingingClip::EditedPhonemeOffsetChange, {m_note});
}