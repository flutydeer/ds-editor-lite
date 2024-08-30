//
// Created by fluty on 24-7-21.
//

#include "NoteWordUtils.h"

#include "Controller/Actions/AppModel/Note/NoteActions.h"
#include "language-manager/ILanguageManager.h"
#include "Model/AppModel/Note.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Language/S2p.h"

void NoteWordUtils::updateOriginalWordProperties(const QList<Note *> &notes) {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        qFatal() << "Language module not ready yet";
        return;
    }
    const auto langMgr = LangMgr::ILanguageManager::instance();

    const auto syllable2p = S2p::instance();
    QList<LangNote *> langNotes;
    QList<PhonemeInfo> notesPhonemes;
    for (const auto note : notes) {
        const auto language = note->language() == "unknown" ? "unknown" : note->language();
        const auto category =
            note->language() == "unknown" ? "unknown" : langMgr->language(language)->category();
        langNotes.append(new LangNote(note->lyric(), language, category));
        notesPhonemes.append(note->phonemeInfo());
    }

    langMgr->correct(langNotes);
    langMgr->convert(langNotes);

    QList<Note::NoteWordProperties> args;
    for (int i = 0; i < langNotes.size(); i++) {
        auto arg = Note::NoteWordProperties::fromNote(*notes[i]);
        arg.lyric = langNotes[i]->lyric;
        arg.pronunciation.original = langNotes[i]->syllable;
        arg.phonemes.original = notesPhonemes[i].original;
        const auto phonemes = syllable2p->syllableToPhoneme(langNotes[i]->syllable);
        if (!phonemes.empty()) {
            if (phonemes.size() == 1) {
                const QString &first = phonemes.at(0);
                arg.phonemes.original.append(Phoneme(Phoneme::Normal, first, 0));

                if (arg.phonemes.original.count() != 1) {
                    arg.phonemes.original.clear();
                    auto phoneme = Phoneme();
                    phoneme.type = Phoneme::Normal;
                    phoneme.start = 0;
                    arg.phonemes.original.append(phoneme);
                }
                arg.phonemes.original.last().name = first;
            } else if (phonemes.size() == 2) {
                const QString &first = phonemes.at(0);
                const QString &last = phonemes.at(1);
                arg.phonemes.original.append(Phoneme(Phoneme::Ahead, first, 0));
                arg.phonemes.original.append(Phoneme(Phoneme::Normal, last, 0));

                if (arg.phonemes.original.count() != 2) {
                    arg.phonemes.original.clear();
                    auto phoneme = Phoneme();
                    phoneme.type = Phoneme::Ahead;
                    phoneme.start = 0;
                    arg.phonemes.original.append(phoneme);

                    phoneme.type = Phoneme::Normal;
                    phoneme.start = 0;
                    arg.phonemes.original.append(phoneme);
                }
                arg.phonemes.original.first().name = first;
                arg.phonemes.original.last().name = last;
            }
        }
        args.append(arg);
    }

    auto a = new NoteActions;
    a->editNotesWordProperties(notes, args);
    a->execute();
    // historyManager->record(a);
}

void NoteWordUtils::fillEditedPhonemeNames(const QList<Note *> &notes) {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        qFatal() << "Language module not ready yet";
        return;
    }
    const auto langMgr = LangMgr::ILanguageManager::instance();
    const auto syllable2p = S2p::instance();
    QList<LangNote *> langNotes;
    QList<PhonemeInfo> notesPhonemes;
    for (const auto note : notes) {
        const auto language = note->language() == "unknown" ? "unknown" : note->language();
        const auto category =
            note->language() == "unknown" ? "unknown" : langMgr->language(language)->category();
        langNotes.append(new LangNote(note->lyric(), language, category));
        notesPhonemes.append(note->phonemeInfo());
    }

    langMgr->correct(langNotes);
    langMgr->convert(langNotes);

    for (int i = 0; i < langNotes.size(); i++) {
        auto note = notes[i];
        auto notePhonemes = note->phonemeInfo();
        const auto phonemes = syllable2p->syllableToPhoneme(langNotes[i]->syllable);
        if (!phonemes.empty()) {
            if (phonemes.size() == 1) {
                const QString &first = phonemes.at(0);
                note->setPhonemeInfo(
                    PhonemeInfo::Edited,
                    {Phoneme(Phoneme::Normal, first, notePhonemes.edited.first().start)});
            } else if (phonemes.size() == 2) {
                const QString &first = phonemes.at(0);
                const QString &last = phonemes.at(1);
                const auto phones = {
                    Phoneme(Phoneme::Ahead, first, notePhonemes.edited.first().start),
                    Phoneme(Phoneme::Normal, last, notePhonemes.edited.last().start)};
                note->setPhonemeInfo(PhonemeInfo::Edited, phones);
            }
        }
    }
}