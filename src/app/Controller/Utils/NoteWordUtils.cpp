//
// Created by fluty on 24-7-21.
//

#include "NoteWordUtils.h"

#include "language-manager/ILanguageManager.h"
#include "Model/AppModel/Note.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Language/S2p.h"

QList<QString> NoteWordUtils::getPronunciations(const QList<Note *> &notes) {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        qFatal() << "Language module not ready yet";
        return {};
    }
    const auto langMgr = LangMgr::ILanguageManager::instance();
    QList<LangNote *> langNotes;
    for (const auto note : notes) {
        const auto language = note->language() == "unknown" ? "unknown" : note->language();
        const auto category =
            note->language() == "unknown" ? "unknown" : langMgr->language(language)->category();
        langNotes.append(new LangNote(note->lyric(), language, category));
    }

    langMgr->correct(langNotes);
    langMgr->convert(langNotes);

    QList<QString> result;
    for (const auto pNote : langNotes) {
        result.append(pNote->syllable);
        delete pNote;
    }
    return result;
}

QList<PhonemeNameResult> NoteWordUtils::getPhonemeNames(const QList<QString> &input) {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        qFatal() << "Language module not ready yet";
        return {};
    }
    const auto syllable2p = S2p::instance();
    QList<PhonemeNameResult> result;
    for (const auto &pronunciation : input) {
        PhonemeNameResult note;
        if (const auto phonemes = syllable2p->syllableToPhoneme(pronunciation); !phonemes.empty()) {
            if (phonemes.size() == 1) {
                note.normalNames.append(phonemes.at(0));
            } else if (phonemes.size() == 2) {
                note.aheadNames.append(phonemes.at(0));
                note.normalNames.append(phonemes.at(1));
            } else
                qCritical() << "Cannot handle more than 2 phonemes" << phonemes;
        } else {
            //TODO: 处理转音记号
            qCritical() << "Failed to get phoneme names of pronunciation:" << pronunciation;
        }
        result.append(note);
    }

    return result;
}

// void NoteWordUtils::fillEditedPhonemeNames(const QList<Note *> &notes) {
//     if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
//         qFatal() << "Language module not ready yet";
//         return;
//     }
//     const auto langMgr = LangMgr::ILanguageManager::instance();
//     const auto syllable2p = S2p::instance();
//     QList<LangNote *> langNotes;
//     QList<PhonemeInfo> notesPhonemes;
//     for (const auto note : notes) {
//         const auto language = note->language() == "unknown" ? "unknown" : note->language();
//         const auto category =
//             note->language() == "unknown" ? "unknown" : langMgr->language(language)->category();
//         langNotes.append(new LangNote(note->lyric(), language, category));
//         notesPhonemes.append(note->phonemeInfo());
//     }
//
//     langMgr->correct(langNotes);
//     langMgr->convert(langNotes);
//
//     for (int i = 0; i < langNotes.size(); i++) {
//         auto note = notes[i];
//         auto notePhonemes = note->phonemeInfo();
//         const auto phonemes = syllable2p->syllableToPhoneme(langNotes[i]->syllable);
//         if (!phonemes.empty()) {
//             if (phonemes.size() == 1) {
//                 const QString &first = phonemes.at(0);
//                 note->setPhonemeInfo(
//                     Note::Edited,
//                     {Phoneme(Phoneme::Normal, first, notePhonemes.edited.first().start)});
//             } else if (phonemes.size() == 2) {
//                 const QString &first = phonemes.at(0);
//                 const QString &last = phonemes.at(1);
//                 const auto phones = {
//                     Phoneme(Phoneme::Ahead, first, notePhonemes.edited.first().start),
//                     Phoneme(Phoneme::Normal, last, notePhonemes.edited.last().start)};
//                 note->setPhonemeInfo(Note::Edited, phones);
//             }
//         }
//     }
// }