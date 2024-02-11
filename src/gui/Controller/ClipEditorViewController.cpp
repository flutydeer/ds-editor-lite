//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorViewController.h"
#include "TracksViewController.h"
#include "Actions/AppModel/Note/NoteActions.h"
#include "History/HistoryManager.h"
#include "mandarin.h"
#include "syllable2p.h"
#include <QApplication>

void ClipEditorViewController::setCurrentSingingClip(DsSingingClip *clip) {
    m_clip = clip;
}
void ClipEditorViewController::onClipPropertyChanged(const Clip::ClipCommonProperties &args) {
    TracksViewController::instance()->onClipPropertyChanged(args);
}
void ClipEditorViewController::onRemoveNotes(const QList<int> &notesId) {
    QList<Note *> notesToDelete;
    for (const auto id : notesId)
        notesToDelete.append(m_clip->findNoteById(id));

    auto a = new NoteActions;
    a->removeNotes(notesToDelete, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::onEditNotesLyrics(const QList<int> &notesId) {
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(m_clip->findNoteById(id));
    QList<Note::NoteWordProperties *> args;

    auto g2p_man = new IKg2p::Mandarin();
    auto syllable2p = new IKg2p::Syllable2p(qApp->applicationDirPath() +
                                            "/res/phonemeDict/opencpop-extension.txt");
    QStringList lyrics;
    for (const auto note : notesToEdit) {
        lyrics.append(note->lyric());
    }

    auto syllableRes = g2p_man->hanziToPinyin(lyrics, false, false);
    for (int i = 0; i < syllableRes.size(); i++) {
        auto properties = new Note::NoteWordProperties;
        properties->lyric = lyrics[i];
        properties->pronunciation = syllableRes[i];
        auto phonemes = syllable2p->syllableToPhoneme(syllableRes[i]);
        properties->phonemes.original.append(Phoneme(Phoneme::Ahead, phonemes.first(), 0));
        properties->phonemes.original.append(Phoneme(Phoneme::Final, phonemes.last(), 0));
        args.append(properties);
    }

    auto a = new NoteActions;
    a->editNotesWordProperties(notesToEdit, args, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}