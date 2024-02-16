//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorViewController.h"
#include "TracksViewController.h"
#include "Actions/AppModel/Note/NoteActions.h"
#include "History/HistoryManager.h"

#include "Utils/G2p/S2p.h"
#include "Utils/G2p/G2pMandarin.h"

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

    removeNotes(notesToDelete);
}
void ClipEditorViewController::onEditNotesLyric(const QList<int> &notesId) {
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(m_clip->findNoteById(id));
    editNotesLyric(notesToEdit);
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
void ClipEditorViewController::onAdjustPhoneme(const QList<int> &notesId,
                                               const QList<Phoneme> &phonemes) {
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(m_clip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotesPhoneme(notesToEdit, phonemes, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::onNoteSelectionChanged(const QList<int> &notesId,
                                                      bool unselectOther) {
    if (unselectOther)
        for (const auto note : m_clip->notes())
            note->setSelected(false);

    for (const auto id : notesId) {
        if (auto note = m_clip->findNoteById(id))
            note->setSelected(true);
    }
    m_clip->notifyNoteSelectionChanged();
}
void ClipEditorViewController::onEditSelectedNotesLyric() {
    auto notes = m_clip->selectedNotes();
    editNotesLyric(notes);
}
void ClipEditorViewController::onRemoveSelectedNotes() {
    auto notes = m_clip->selectedNotes();
    removeNotes(notes);
}
void ClipEditorViewController::editNotesLyric(const QList<Note *> &notes) {
    QList<Note::NoteWordProperties *> args;

    auto g2p_man = G2pMandarin::instance();
    auto syllable2p = S2p::instance();
    QStringList lyrics;
    QList<Phonemes> notesPhonemes;
    for (const auto note : notes) {
        lyrics.append(note->lyric());
        notesPhonemes.append(note->phonemes());
    }

    auto syllableRes = g2p_man->hanziToPinyin(lyrics, false, false);
    for (int i = 0; i < syllableRes.size(); i++) {
        auto properties = new Note::NoteWordProperties;
        properties->lyric = lyrics[i];
        properties->pronunciation = syllableRes[i];
        properties->phonemes.edited = notesPhonemes[i].edited;
        auto phonemes = syllable2p->syllableToPhoneme(syllableRes[i]);
        if (!phonemes.isEmpty()) {
            if (phonemes.count() == 1) {
                properties->phonemes.original.append(Phoneme(Phoneme::Normal, phonemes.first(), 0));

                if (properties->phonemes.edited.count() != 1) {
                    properties->phonemes.edited.clear();
                    auto phoneme = Phoneme();
                    phoneme.type = Phoneme::Normal;
                    phoneme.start = 0;
                    properties->phonemes.edited.append(phoneme);
                }
                properties->phonemes.edited.last().name = phonemes.first();
            } else if (phonemes.count() == 2) {
                properties->phonemes.original.append(Phoneme(Phoneme::Ahead, phonemes.first(), 0));
                properties->phonemes.original.append(Phoneme(Phoneme::Normal, phonemes.last(), 0));

                if (properties->phonemes.edited.count() != 2) {
                    properties->phonemes.edited.clear();
                    auto phoneme = Phoneme();
                    phoneme.type = Phoneme::Ahead;
                    phoneme.start = 0;
                    properties->phonemes.edited.append(phoneme);

                    phoneme.type = Phoneme::Normal;
                    phoneme.start = 0;
                    properties->phonemes.edited.append(phoneme);
                }
                properties->phonemes.edited.first().name = phonemes.first();
                properties->phonemes.edited.last().name = phonemes.last();
            }
        }
        args.append(properties);
    }

    auto a = new NoteActions;
    a->editNotesWordProperties(notes, args, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::removeNotes(const QList<Note *> &notes) {
    auto a = new NoteActions;
    a->removeNotes(notes, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}