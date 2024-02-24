//
// Created by fluty on 2024/2/10.
//

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QJsonDocument>

#include "ClipEditorViewController.h"
#include "ControllerGlobal.h"
#include "TracksViewController.h"
#include "Actions/AppModel/Note/NoteActions.h"
#include "Actions/AppModel/Param/ParamsActions.h"
#include "History/HistoryManager.h"

#include "Utils/G2p/S2p.h"
#include "Utils/G2p/G2pMandarin.h"
#include "Utils/FillLyric/LyricDialog.h"

void ClipEditorViewController::setCurrentSingingClip(SingingClip *clip) {
    m_clip = clip;
    // updateAndNotifyCanSelectAll();
}
void ClipEditorViewController::copySelectedNotesWithParams() const {
    qDebug() << "ClipEditorViewController::copySelectedNotesWithParams";
    auto info = buildNoteParamsInfo();
    if (info.selectedNotes.count() < 0)
        return;

    auto array = NotesParamsInfo::serializeToBinary(info);
    // auto jObj = NotesParamsInfo::serializeToJson(info);
    // QJsonDocument jDoc;
    // jDoc.setObject(jObj);
    // auto array = jDoc.toJson();
    auto data = new QMimeData;
    data->setData(ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams), array);
    QGuiApplication::clipboard()->setMimeData(data);
    // qDebug() << QString("Copied %1 notes").arg(info.selectedNotes.count());
}
void ClipEditorViewController::cutSelectedNotesWithParams() {
    copySelectedNotesWithParams();
    onRemoveSelectedNotes();
}
void ClipEditorViewController::pasteNotesWithParams(const NotesParamsInfo &info, int tick) {
    qDebug() << "ClipEditorViewController::pasteNotesWithParams";
    auto notes = info.selectedNotes;
    qDebug() << "info.selectedNotes count" << notes.count();
    if (notes.count() == 0)
        return;
    auto start = notes.first().start();
    auto offset = tick - start;
    QList<Note *> notesPtr;
    for (auto &note : notes) {
        note.setStart(note.start() + offset);

        auto notePtr = new Note;
        notePtr->setStart(note.start());
        notePtr->setLength(note.length());
        notePtr->setKeyIndex(note.keyIndex());
        notePtr->setLyric(note.lyric());
        notePtr->setPronunciation(note.pronunciation());
        notePtr->setPhonemes(Phonemes::Original, note.phonemes().original);
        notePtr->setPhonemes(Phonemes::Edited, note.phonemes().edited);
        notesPtr.append(notePtr);
    }
    auto a = new NoteActions;
    a->insertNotes(notesPtr, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
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
void ClipEditorViewController::onInsertNote(Note *note) const {
    auto a = new NoteActions;
    QList<Note *> notes;
    notes.append(note);
    a->insertNotes(notes, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
    // updateAndNotifyCanSelectAll();
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
void ClipEditorViewController::onResizeNotesLeft(const QList<int> &notesId, int deltaTick) const {
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(m_clip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotesStartAndLength(notesToEdit, deltaTick, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::onResizeNotesRight(const QList<int> &notesId, int deltaTick) const {
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(m_clip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotesLength(notesToEdit, deltaTick, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::onAdjustPhoneme(const QList<int> &notesId,
                                               const QList<Phoneme> &phonemes) const {
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
    if (notesId.isEmpty())
        emit canRemoveChanged(false);
    else
        emit canRemoveChanged(true);

    m_clip->notifyNoteSelectionChanged();
}
void ClipEditorViewController::onOriginalPitchChanged(
    const OverlapableSerialList<Curve> &curves) const {
    auto a = new ParamsActions;
    a->replacePitchOriginal(curves, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::onPitchEdited(const OverlapableSerialList<Curve> &curves) const {
    auto a = new ParamsActions;
    a->replacePitchEdited(curves, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::onEditSelectedNotesLyric() const {
    auto notes = m_clip->selectedNotes();
    editNotesLyric(notes);
}
void ClipEditorViewController::onRemoveSelectedNotes() {
    auto notes = m_clip->selectedNotes();
    removeNotes(notes);
    emit canRemoveChanged(false);
}
void ClipEditorViewController::onSelectAllNotes() {
    if (m_clip->notes().count() == 0)
        return;

    for (const auto note : m_clip->notes())
        note->setSelected(true);
    emit canRemoveChanged(true);
    m_clip->notifyNoteSelectionChanged();
}
void ClipEditorViewController::onFillLyric(QWidget *parent) {
    int selectedTrackIndex;
    auto selectedClipIndex = AppModel::instance()->selectedClipId();
    auto selectedClip = AppModel::instance()->findClipById(selectedClipIndex, selectedTrackIndex);

    QList<Note *> selectedNotes;
    if (selectedClip != nullptr) {
        if (selectedClip->type() == Clip::Singing)
            selectedNotes = dynamic_cast<SingingClip *>(selectedClip)->selectedNotes();
    }

    auto lyricDialog = new FillLyric::LyricDialog(selectedNotes, parent);
    lyricDialog->show();

    auto result = lyricDialog->exec();
    if (result == QDialog::Accepted) {
        // TODO: lyricDialog->exportPhonics();
        ClipEditorViewController::instance()->onEditSelectedNotesLyric();
    }
    delete lyricDialog;
}
void ClipEditorViewController::editNotesLyric(const QList<Note *> &notes) const {
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
        properties->pronunciation.original = syllableRes[i];
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
void ClipEditorViewController::removeNotes(const QList<Note *> &notes) const {
    auto a = new NoteActions;
    a->removeNotes(notes, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
    // updateAndNotifyCanSelectAll();
}
NotesParamsInfo ClipEditorViewController::buildNoteParamsInfo() const {
    auto notes = m_clip->selectedNotes();
    NotesParamsInfo info;
    for (const auto &note : notes)
        info.selectedNotes.append(*note);
    return info;
}
// void ClipEditorViewController::updateAndNotifyCanSelectAll() {
//     if (!m_clip) {
//         emit canSelectAllChanged(false);
//         return;
//     }
//
//     if (m_clip->notes().count() == 0) {
//         emit canSelectAllChanged(false);
//         return;
//     }
//
//     emit canSelectAllChanged(true);
// }