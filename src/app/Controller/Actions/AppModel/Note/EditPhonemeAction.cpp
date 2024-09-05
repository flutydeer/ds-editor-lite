//
// Created by fluty on 24-2-14.
//

#include "EditPhonemeAction.h"

#include "Model/AppModel/Clip.h"

EditPhonemeAction *EditPhonemeAction::build(Note *note, const QList<Phoneme> &phonemes) {
    auto a = new EditPhonemeAction;
    a->m_note = note;
    a->m_oldPhonemes = note->phonemeInfo().edited;
    a->m_newPhonemes = phonemes;
    return a;
}

void EditPhonemeAction::execute() {
    m_note->setPhonemeInfo(Note::Edited, m_newPhonemes);
    m_note->notifyWordPropertyChanged(Note::Edited);
}

void EditPhonemeAction::undo() {
    m_note->setPhonemeInfo(Note::Edited, m_oldPhonemes);
    m_note->notifyWordPropertyChanged(Note::Edited);
}