//
// Created by fluty on 24-2-14.
//

#include "EditPhonemeAction.h"

#include "Model/AppModel/Clip.h"

EditPhonemeAction::EditPhonemeAction(Note *note, const QList<Phoneme> &phonemes,
                                     SingingClip *clip) {
    m_note = note;
    m_oldPhonemes = note->phonemeInfo().edited;
    m_newPhonemes = phonemes;
    m_clip = clip;
}

void EditPhonemeAction::execute() {
    m_note->setPhonemeInfo(Note::Edited, m_newPhonemes);
    m_clip->notifyNoteChanged(SingingClip::EditedWordPropertyChange, {m_note});
}

void EditPhonemeAction::undo() {
    m_note->setPhonemeInfo(Note::Edited, m_oldPhonemes);
    m_clip->notifyNoteChanged(SingingClip::EditedWordPropertyChange, {m_note});
}