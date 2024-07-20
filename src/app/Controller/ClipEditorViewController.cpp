//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorViewController.h"

#include "TracksViewController.h"
#include "Actions/AppModel/Note/NoteActions.h"
#include "Actions/AppModel/Param/ParamsActions.h"
#include "Global/ControllerGlobal.h"
#include "Interface/IClipEditorView.h"
#include "Model/AppModel.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Language/S2p.h"
#include "UI/Dialogs/FillLyric/LyricDialog.h"

#include <QClipboard>
#include <QMimeData>
#include <LangMgr/ILanguageManager.h>

void ClipEditorViewController::setView(IClipEditorView *view) {
    m_view = view;
}
void ClipEditorViewController::setCurrentSingingClip(SingingClip *clip) {
    m_clip = clip;
    emit canSelectAllChanged(canSelectAll());
    emit hasSelectedNotesChanged(hasSelectedNotes());
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
    onDeleteSelectedNotes();
}
void ClipEditorViewController::pasteNotesWithParams(const NotesParamsInfo &info, int tick) {
    qDebug() << "ClipEditorViewController::pasteNotesWithParams";
    auto notes = info.selectedNotes;
    qDebug() << "info.selectedNotes count" << notes.count();
    if (notes.count() == 0)
        return;
    auto start = notes.first()->start();
    auto offset = tick - start;
    QList<Note *> notesPtr;
    for (auto &note : notes) {
        note->setStart(note->start() + offset);

        auto notePtr = new Note;
        notePtr->setStart(note->start());
        notePtr->setLength(note->length());
        notePtr->setKeyIndex(note->keyIndex());
        notePtr->setLyric(note->lyric());
        notePtr->setPronunciation(note->pronunciation());
        notePtr->setPhonemes(Phonemes::Original, note->phonemes().original);
        notePtr->setPhonemes(Phonemes::Edited, note->phonemes().edited);
        notesPtr.append(notePtr);
    }
    auto a = new NoteActions;
    a->insertNotes(notesPtr, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}
bool ClipEditorViewController::canSelectAll() const {
    if (!m_clip || m_clip->notes().count() == 0)
        return false;
    // TODO: 仅在选择和绘制模式下可全选
    return true;
}
bool ClipEditorViewController::hasSelectedNotes() const {
    if (!m_clip || m_clip->notes().count() == 0)
        return false;
    auto selectedNotes = m_clip->selectedNotes();
    return !selectedNotes.isEmpty();
}
void ClipEditorViewController::centerAt(double tick, double keyIndex) {
    if (m_view)
        m_view->centerAt(tick, keyIndex);
}
void ClipEditorViewController::centerAt(const Note &note) {
    m_view->centerAt(note.start(), note.length(), note.keyIndex());
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
    emit hasSelectedNotesChanged(hasSelectedNotes());
    // updateAndNotifyCanSelectAll();
}
void ClipEditorViewController::onMoveNotes(const QList<int> &notesId, int deltaTick, int deltaKey) {
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(m_clip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotePosition(notesToEdit, deltaTick, deltaKey);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::onResizeNotesLeft(const QList<int> &notesId, int deltaTick) const {
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(m_clip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotesStartAndLength(notesToEdit, deltaTick);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::onResizeNotesRight(const QList<int> &notesId, int deltaTick) const {
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(m_clip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotesLength(notesToEdit, deltaTick);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::onAdjustPhoneme(const QList<int> &notesId,
                                               const QList<Phoneme> &phonemes) const {
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(m_clip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotesPhoneme(notesToEdit, phonemes);
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
    emit hasSelectedNotesChanged(hasSelectedNotes());
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
void ClipEditorViewController::onDeleteSelectedNotes() {
    auto notes = m_clip->selectedNotes();
    removeNotes(notes);
    emit hasSelectedNotesChanged(false);
}
void ClipEditorViewController::onSelectAllNotes() {
    if (m_clip->notes().count() == 0)
        return;

    for (const auto note : m_clip->notes())
        note->setSelected(true);
    emit hasSelectedNotesChanged(true);
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

    QList<Note *> inputNotes;
    for (const auto &note : selectedNotes) {
        const auto inputNote = new Note();
        inputNote->setLyric(note->lyric());
        inputNote->setLanguage(note->language());
        inputNotes.append(inputNote);
    }

    LyricDialog lyricDialog(inputNotes, parent);
    lyricDialog.exec();

    const auto result = lyricDialog.result();
    if (result == QDialog::Accepted) {
        auto noteRes = lyricDialog.exportLangNotes();
        for (int i = 0; i < selectedNotes.size(); i++) {
            selectedNotes[i]->setLyric(noteRes[i].lyric);
            selectedNotes[i]->setLanguage(noteRes[i].language);
            selectedNotes[i]->setPronunciation(
                Pronunciation(noteRes[i].syllable, noteRes[i].syllableRevised));
            selectedNotes[i]->setPronCandidates(noteRes[i].candidates);
        }
        ClipEditorViewController::instance()->onEditSelectedNotesLyric();
    }
}
void ClipEditorViewController::editNotesLyric(const QList<Note *> &notes) const {
    QList<Note::NoteWordProperties *> args;

    const auto langMgr = LangMgr::ILanguageManager::instance();

    const auto syllable2p = S2p::instance();
    QList<LangNote *> langNotes;
    QList<Phonemes> notesPhonemes;
    for (const auto note : notes) {
        const auto language = note->language() == "Unknown" ? "Unknown" : note->language();
        const auto category =
            note->language() == "Unknown" ? "Unknown" : langMgr->language(language)->category();
        langNotes.append(new LangNote(note->lyric(), language, category));
        notesPhonemes.append(note->phonemes());
    }

    langMgr->correct(langNotes);
    langMgr->convert(langNotes);

    for (int i = 0; i < langNotes.size(); i++) {
        const auto properties = new Note::NoteWordProperties;
        properties->lyric = langNotes[i]->lyric;
        properties->pronunciation.original = langNotes[i]->syllable;
        properties->phonemes.edited = notesPhonemes[i].edited;
        const auto phonemes =
            syllable2p->syllableToPhoneme(langNotes[i]->syllable.toUtf8().toStdString());
        if (!phonemes.empty()) {
            if (phonemes.size() == 1) {
                const QString first = QString::fromUtf8(phonemes.at(0));
                properties->phonemes.original.append(Phoneme(Phoneme::Normal, first, 0));

                if (properties->phonemes.edited.count() != 1) {
                    properties->phonemes.edited.clear();
                    auto phoneme = Phoneme();
                    phoneme.type = Phoneme::Normal;
                    phoneme.start = 0;
                    properties->phonemes.edited.append(phoneme);
                }
                properties->phonemes.edited.last().name = first;
            } else if (phonemes.size() == 2) {
                const QString first = QString::fromUtf8(phonemes.at(0));
                const QString last = QString::fromUtf8(phonemes.at(1));
                properties->phonemes.original.append(Phoneme(Phoneme::Ahead, first, 0));
                properties->phonemes.original.append(Phoneme(Phoneme::Normal, last, 0));

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
                properties->phonemes.edited.first().name = first;
                properties->phonemes.edited.last().name = last;
            }
        }
        args.append(properties);
    }

    auto a = new NoteActions;
    a->editNotesWordProperties(notes, args);
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
        info.selectedNotes.append(note);
    return info;
}