//
// Created by fluty on 24-2-14.
//

#include "EditPhonemeAction.h"

#include "Model/Clip.h"

EditPhonemeAction *EditPhonemeAction::build(Note *note, const Phoneme &phoneme, SingingClip *clip) {
    auto a = new EditPhonemeAction;
    a->m_note = note;
    a->m_phonemes = note->phonemes().edited;
    a->m_phoneme = phoneme;
    a->m_clip = clip;
    return a;
}
void EditPhonemeAction::execute() {
    QList<Phoneme> newPhonemes;
    for (const auto &phoneme : m_phonemes) {
        newPhonemes.append(phoneme);
    }
    if (m_phoneme.type == Phoneme::Ahead) {
        newPhonemes.first() = m_phoneme;
    } else if (m_phoneme.type == Phoneme::Normal) {
        newPhonemes.last() = m_phoneme;
    }
    m_note->setPhonemes(Phonemes::Edited, newPhonemes);
    m_note->notifyPropertyChanged(Note::Word);
}
void EditPhonemeAction::undo() {
    m_note->setPhonemes(Phonemes::Edited, m_phonemes);
    m_note->notifyPropertyChanged(Note::Word);
}