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

void ClipEditorViewController::setCurrentSingingClip(SingingClip *clip) {
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
        if (!phonemes.isEmpty()) {
            properties->phonemes.original.append(Phoneme(Phoneme::Ahead, phonemes.first(), 0));
            properties->phonemes.original.append(Phoneme(Phoneme::Normal, phonemes.last(), 0));
        }
        args.append(properties);
    }

    auto a = new NoteActions;
    a->editNotesWordProperties(notesToEdit, args, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::onInsertNote(Note *note) {
    auto a = new NoteActions;
    QList<Note *> notes;
    notes.append(note);
    a->insertNotes(notes, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::onMoveNotes(const QList<int> &notesId, int deltaTick, int deltaKey) {
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(m_clip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotePosition(notesToEdit, deltaTick, deltaKey, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::onResizeNotesLeft(const QList<int> &notesId, int deltaTick) {
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(m_clip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotesStartAndLength(notesToEdit, deltaTick, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::onResizeNotesRight(const QList<int> &notesId, int deltaTick) {
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(m_clip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotesLength(notesToEdit, deltaTick, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}