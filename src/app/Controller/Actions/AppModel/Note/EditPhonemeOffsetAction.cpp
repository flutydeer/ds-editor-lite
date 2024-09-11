//
// Created by fluty on 24-9-11.
//

#include "EditPhonemeOffsetAction.h"

#include "Model/AppModel/Clip.h"

EditPhonemeOffsetAction::EditPhonemeOffsetAction(Note *note, PhonemeInfoSeperated::PhonemeType type,
                                                 const QList<int> &offsets, SingingClip *clip)
    : m_note(note), m_type(type), m_newOffsets(offsets), m_clip(clip) {
    if (type == PhonemeInfoSeperated::Ahead)
        m_oldOffsets = m_note->phonemeOffsetInfo().ahead.edited;
    else if (type == PhonemeInfoSeperated::Normal)
        m_oldOffsets = m_note->phonemeOffsetInfo().normal.edited;
    else if (type == PhonemeInfoSeperated::Final)
        m_oldOffsets = m_note->phonemeOffsetInfo().final.edited;
}

void EditPhonemeOffsetAction::execute() {
    m_note->setPhonemeOffsetInfo(m_type, Note::Edited, m_newOffsets);
    m_clip->notifyNoteChanged(SingingClip::EditedWordPropertyChange, {m_note});
}

void EditPhonemeOffsetAction::undo() {
    m_note->setPhonemeOffsetInfo(m_type, Note::Edited, m_oldOffsets);
    m_clip->notifyNoteChanged(SingingClip::EditedWordPropertyChange, {m_note});
}