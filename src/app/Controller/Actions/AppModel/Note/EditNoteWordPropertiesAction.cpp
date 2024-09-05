//
// Created by fluty on 2024/2/8.
//

#include "EditNoteWordPropertiesAction.h"

#include <utility>

#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Note.h"

EditNoteWordPropertiesAction *EditNoteWordPropertiesAction::build(Note *note,
                                                                  Note::WordProperties args) {
    Note::WordProperties oldArgs;
    oldArgs = Note::WordProperties::fromNote(*note);

    auto a = new EditNoteWordPropertiesAction;
    a->m_note = note;
    a->m_oldArgs = oldArgs;
    a->m_newArgs = std::move(args);
    return a;
}

void EditNoteWordPropertiesAction::execute() {
    m_note->setLyric(m_newArgs.lyric);
    m_note->setLanguage(m_newArgs.language);
    // m_note->setPhonemeInfo(Note::Original, m_newArgs.phonemes.original);
    m_note->setPhonemeInfo(Note::Edited, m_newArgs.phonemes.edited);
    m_note->setPronunciation(Note::Edited, m_newArgs.pronunciation.edited);
    m_note->setPronCandidates(m_newArgs.pronCandidates);
    m_note->notifyWordPropertyChanged(Note::Edited);
}

void EditNoteWordPropertiesAction::undo() {
    m_note->setLyric(m_oldArgs.lyric);
    m_note->setLanguage(m_oldArgs.language);
    // m_note->setPhonemeInfo(Note::Original, m_oldArgs.phonemes.original);
    m_note->setPhonemeInfo(Note::Edited, m_oldArgs.phonemes.edited);
    m_note->setPronunciation(Note::Edited, m_newArgs.pronunciation.edited);
    m_note->setPronCandidates(m_oldArgs.pronCandidates);
    m_note->notifyWordPropertyChanged(Note::Edited);
}