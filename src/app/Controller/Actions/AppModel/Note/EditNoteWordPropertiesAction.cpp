//
// Created by fluty on 2024/2/8.
//

#include "EditNoteWordPropertiesAction.h"

#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"

EditNoteWordPropertiesAction::EditNoteWordPropertiesAction(const QList<Note *> &notes,
                                                           const QList<Note::WordProperties> &args,
                                                           SingingClip *clip) {
    m_notes = notes;
    m_newArgs = args;
    m_clip = clip;
    for (const auto &note : notes) {
        auto properties = Note::WordProperties::fromNote(*note);
        m_oldArgs.append(properties);
    }
}

void EditNoteWordPropertiesAction::execute() {
    qsizetype i = 0;
    for (const auto note : m_notes) {
        auto [lyric, language, pronunciation, pronCandidates, phonemes] = m_newArgs.at(i);
        note->setLyric(lyric);
        note->setLanguage(language);
        // note->setPhonemeInfo(Note::Original, arg.phonemes.original);
        // note->setPhonemeInfo(Note::Edited, arg.phonemes.edited);
        note->setPhonemes(phonemes);
        note->setPronunciation(Note::Edited, pronunciation.edited);
        note->setPronCandidates(pronCandidates);
        i++;
    }
    m_clip->notifyNoteChanged(SingingClip::EditedWordPropertyChange, m_notes);
}

void EditNoteWordPropertiesAction::undo() {
    for (auto i = m_notes.count() - 1; i >= 0; i--) {
        auto [lyric, language, pronunciation, pronCandidates, phonemes] = m_oldArgs.at(i);
        const auto note = m_notes.at(i);
        note->setLyric(lyric);
        note->setLanguage(language);
        // note->setPhonemeInfo(Note::Original, arg.phonemes.original);
        note->setPhonemes(phonemes);
        note->setPronunciation(Note::Edited, pronunciation.edited);
        note->setPronCandidates(pronCandidates);
    }
    m_clip->notifyNoteChanged(SingingClip::EditedWordPropertyChange, m_notes);
}