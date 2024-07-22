//
// Created by fluty on 2024/2/8.
//

#include "EditNotesWordPropertiesAction.h"

#include <utility>

#include "Model/Clip.h"
#include "Model/Note.h"

EditNotesWordPropertiesAction *EditNotesWordPropertiesAction::build(Note *note,
                                                                    Note::NoteWordProperties args) {
    Note::NoteWordProperties oldArgs;
    oldArgs = Note::NoteWordProperties::fromNote(*note);

    auto a = new EditNotesWordPropertiesAction;
    a->m_note = note;
    a->m_oldArgs = oldArgs;
    a->m_newArgs = std::move(args);
    return a;
}
void EditNotesWordPropertiesAction::execute() {
    m_note->setLyric(m_newArgs.lyric);
    m_note->setLanguage(m_newArgs.language);
    m_note->setPhonemes(Phonemes::Original, m_newArgs.phonemes.original);
    m_note->setPhonemes(Phonemes::Edited, m_newArgs.phonemes.edited);
    m_note->setPronunciation(m_newArgs.pronunciation);
    m_note->setPronCandidates(m_newArgs.pronCandidates);
    m_note->notifyPropertyChanged(Note::Word);
}
void EditNotesWordPropertiesAction::undo() {
    m_note->setLyric(m_oldArgs.lyric);
    m_note->setLanguage(m_oldArgs.language);
    m_note->setPhonemes(Phonemes::Original, m_oldArgs.phonemes.original);
    m_note->setPhonemes(Phonemes::Edited, m_oldArgs.phonemes.edited);
    m_note->setPronunciation(m_oldArgs.pronunciation);
    m_note->setPronCandidates(m_oldArgs.pronCandidates);
    m_note->notifyPropertyChanged(Note::Word);
}