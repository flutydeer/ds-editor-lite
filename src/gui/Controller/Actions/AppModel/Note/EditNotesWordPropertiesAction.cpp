//
// Created by fluty on 2024/2/8.
//

#include "EditNotesWordPropertiesAction.h"
EditNotesWordPropertiesAction *
    EditNotesWordPropertiesAction::build(DsNote *note, DsNote::NoteWordProperties *args, DsSingingClip *clip) {
    DsNote::NoteWordProperties oldArgs;
    oldArgs.lyric = note->lyric();
    oldArgs.phonemes = note->phonemes();
    oldArgs.pronunciation = note->pronunciation();

    auto a = new EditNotesWordPropertiesAction;
    a->m_note = note;
    a->m_oldArgs = oldArgs;
    a->m_newArgs = *args;
    a->m_clip = clip;
    return a;
}
void EditNotesWordPropertiesAction::execute() {
    m_note->setLyric(m_newArgs.lyric);
    m_note->setPhonemes(DsPhonemes::Original, m_newArgs.phonemes.original);
    m_note->setPhonemes(DsPhonemes::Edited, m_newArgs.phonemes.edited);
    m_note->setPronunciation(m_newArgs.pronunciation);
    m_clip->notifyNotePropertyChanged(m_note);
}
void EditNotesWordPropertiesAction::undo() {
    m_note->setLyric(m_oldArgs.lyric);
    m_note->setPhonemes(DsPhonemes::Original, m_oldArgs.phonemes.original);
    m_note->setPhonemes(DsPhonemes::Edited, m_oldArgs.phonemes.edited);
    m_note->setPronunciation(m_oldArgs.pronunciation);
    m_clip->notifyNotePropertyChanged(m_note);
}