//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorViewController.h"
#include "ClipEditorViewController_p.h"

#include "TracksViewController.h"
#include "Actions/AppModel/Note/NoteActions.h"
#include "Actions/AppModel/Param/ParamsActions.h"
#include "Global/ControllerGlobal.h"
#include "Interface/IClipEditorView.h"
#include "Model/AppModel/AppModel.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Language/S2p.h"
#include "UI/Controls/Toast.h"
#include "UI/Dialogs/FillLyric/LyricDialog.h"

#include <QClipboard>
#include <QMimeData>
#include <LangMgr/ILanguageManager.h>

ClipEditorViewController::ClipEditorViewController()
    : d_ptr(new ClipEditorViewControllerPrivate(this)) {
}

ClipEditorViewController::~ClipEditorViewController() {
    delete d_ptr;
}

void ClipEditorViewController::setView(IClipEditorView *view) {
    Q_D(ClipEditorViewController);
    d->m_view = view;
}

void ClipEditorViewController::setClip(Clip *clip) {
    Q_D(ClipEditorViewController);
    d->m_clip = clip;
    emit canSelectAllChanged(canSelectAll());
    emit hasSelectedNotesChanged(hasSelectedNotes());
}

void ClipEditorViewController::copySelectedNotesWithParams() const {
    Q_D(const ClipEditorViewController);
    qDebug() << "ClipEditorViewController::copySelectedNotesWithParams";
    auto info = d->buildNoteParamsInfo();
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
    Q_D(ClipEditorViewController);
    // TODO: pasteNotesWithParams
    // qDebug() << "ClipEditorViewController::pasteNotesWithParams";
    // auto notes = info.selectedNotes;
    // qDebug() << "info.selectedNotes count" << notes.count();
    // if (notes.count() == 0)
    //     return;
    // auto start = notes.first()->start();
    // auto offset = tick - start;
    // QList<Note *> notesPtr;
    // for (auto &note : notes) {
    //     note->setStart(note->start() + offset);
    //
    //     auto notePtr = new Note;
    //     notePtr->setStart(note->start());
    //     notePtr->setLength(note->length());
    //     notePtr->setKeyIndex(note->keyIndex());
    //     notePtr->setLyric(note->lyric());
    //     notePtr->setPronunciation(note->pronunciation());
    //     notePtr->setPhonemes(Phonemes::Original, note->phonemes().original);
    //     notePtr->setPhonemes(Phonemes::Edited, note->phonemes().edited);
    //     notesPtr.append(notePtr);
    // }
    // auto a = new NoteActions;
    // a->insertNotes(notesPtr, d->m_clip);
    // a->execute();
    // historyManager->record(a);
}

bool ClipEditorViewController::canSelectAll() const {
    Q_D(const ClipEditorViewController);
    if (!d->m_clip)
        return false;
    if (d->m_clip->clipType() != Clip::Singing)
        return false;
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    if (singingClip->notes().count() == 0)
        return false;
    // TODO: 仅在选择和绘制模式下可全选
    return true;
}

bool ClipEditorViewController::hasSelectedNotes() const {
    Q_D(const ClipEditorViewController);
    if (!d->m_clip)
        return false;
    if (d->m_clip->clipType() != Clip::Singing)
        return false;
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    if (singingClip->notes().count() == 0)
        return false;
    auto selectedNotes = singingClip->selectedNotes();
    return !selectedNotes.isEmpty();
}

void ClipEditorViewController::centerAt(double tick, double keyIndex) {
    Q_D(ClipEditorViewController);
    if (d->m_view)
        d->m_view->centerAt(tick, keyIndex);
}

void ClipEditorViewController::centerAt(const Note &note) {
    Q_D(ClipEditorViewController);
    d->m_view->centerAt(note.start(), note.length(), note.keyIndex());
}

void ClipEditorViewController::onClipPropertyChanged(const Clip::ClipCommonProperties &args) {
    trackController->onClipPropertyChanged(args);
}

void ClipEditorViewController::onRemoveNotes(const QList<int> &notesId) {
    Q_D(ClipEditorViewController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    QList<Note *> notesToDelete;
    for (const auto id : notesId)
        notesToDelete.append(singingClip->findNoteById(id));

    d->removeNotes(notesToDelete);
}

void ClipEditorViewController::onInsertNote(Note *note) {
    Q_D(ClipEditorViewController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    auto a = new NoteActions;
    QList<Note *> notes;
    notes.append(note);
    a->insertNotes(notes, singingClip);
    a->execute();
    historyManager->record(a);
    emit hasSelectedNotesChanged(hasSelectedNotes());
    // updateAndNotifyCanSelectAll();
}

void ClipEditorViewController::onMoveNotes(const QList<int> &notesId, int deltaTick, int deltaKey) {
    Q_D(ClipEditorViewController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(singingClip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotePosition(notesToEdit, deltaTick, deltaKey, singingClip);
    a->execute();
    historyManager->record(a);
}

void ClipEditorViewController::onResizeNotesLeft(const QList<int> &notesId, int deltaTick) const {
    Q_D(const ClipEditorViewController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(singingClip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotesStartAndLength(notesToEdit, deltaTick, singingClip);
    a->execute();
    historyManager->record(a);
}

void ClipEditorViewController::onResizeNotesRight(const QList<int> &notesId, int deltaTick) const {
    Q_D(const ClipEditorViewController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(singingClip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotesLength(notesToEdit, deltaTick, singingClip);
    a->execute();
    historyManager->record(a);
}

void ClipEditorViewController::onAdjustPhoneme(const QList<int> &notesId,
                                               const QList<Phoneme> &phonemes) const {
    Q_D(const ClipEditorViewController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(singingClip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotesPhoneme(notesToEdit, phonemes);
    a->execute();
    historyManager->record(a);
}

void ClipEditorViewController::onNoteSelectionChanged(const QList<int> &notesId,
                                                      bool unselectOther) {
    Q_D(ClipEditorViewController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    if (unselectOther)
        for (const auto note : singingClip->notes())
            note->setSelected(false);

    for (const auto id : notesId) {
        if (auto note = singingClip->findNoteById(id))
            note->setSelected(true);
    }
    singingClip->notifyNoteSelectionChanged();
    emit hasSelectedNotesChanged(hasSelectedNotes());
}

void ClipEditorViewController::onOriginalPitchChanged(
    const OverlappableSerialList<Curve> &curves) const {
    Q_D(const ClipEditorViewController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    auto a = new ParamsActions;
    a->replacePitchOriginal(curves, singingClip);
    a->execute();
    historyManager->record(a);
}

void ClipEditorViewController::onPitchEdited(const OverlappableSerialList<Curve> &curves) const {
    Q_D(const ClipEditorViewController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    auto a = new ParamsActions;
    a->replacePitchEdited(curves, singingClip);
    a->execute();
    historyManager->record(a);
}

void ClipEditorViewController::onDeleteSelectedNotes() {
    Q_D(const ClipEditorViewController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    auto notes = singingClip->selectedNotes();
    d->removeNotes(notes);
    emit hasSelectedNotesChanged(false);
}

void ClipEditorViewController::onSelectAllNotes() {
    Q_D(const ClipEditorViewController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    for (const auto note : singingClip->notes())
        note->setSelected(true);
    emit hasSelectedNotesChanged(true);
    singingClip->notifyNoteSelectionChanged();
}

void ClipEditorViewController::onFillLyric(QWidget *parent) {
    Q_D(const ClipEditorViewController);
    // if (d->m_clip == nullptr)
    //     return;
    // if (d->m_clip->type() != Clip::Singing)
    //     return;

    auto selectedNotes = reinterpret_cast<SingingClip *>(d->m_clip)->selectedNotes();

    int slurCount = 0;
    QList<Note *> inputNotes;
    for (const auto &note : selectedNotes) {
        if (note->lyric() == '-')
            slurCount++;
        const auto inputNote = new Note();
        inputNote->setLyric(note->lyric());
        inputNote->setPronunciation(note->pronunciation());
        inputNote->setPronCandidates(note->pronCandidates());
        inputNote->setLanguage(note->language());
        inputNote->setPhonemes(Phonemes::Original, note->phonemes().original);
        inputNote->setPhonemes(Phonemes::Edited, note->phonemes().edited);
        inputNotes.append(inputNote);
    }

    LyricDialog lyricDialog(inputNotes, parent);
    lyricDialog.exec();

    const auto result = lyricDialog.result();
    if (result != QDialog::Accepted)
        return;

    const auto lyricRes = lyricDialog.noteResult();
    auto noteRes = lyricRes.langNotes;
    auto notesToEdit = selectedNotes;
    if (!lyricRes.skipSlur)
        slurCount = 0;
    if (noteRes.count() + slurCount > selectedNotes.count()) {
        // Toast::show("输出音符数大于输入，多余的歌词将被忽略");
    } else if (noteRes.count() + slurCount < selectedNotes.count()) {
        // Toast::show("输出音符数小于输入");
        auto i = noteRes.count() + slurCount;
        auto n = selectedNotes.count() - noteRes.count() - slurCount;
        notesToEdit.remove(i, n);
    }

    QList<Note::NoteWordProperties> args;
    int skipCount = 0;
    for (int i = 0; i < notesToEdit.size(); i++) {
        auto arg = Note::NoteWordProperties::fromNote(*selectedNotes[i]);
        if (lyricRes.skipSlur && arg.lyric == '-') {
            args.append(arg);
            skipCount++;
            continue;
        }
        arg.lyric = noteRes[i - skipCount].lyric;
        arg.language = noteRes[i - skipCount].language;
        arg.pronunciation = Pronunciation(noteRes[i - skipCount].syllable,
                                          noteRes[i - skipCount].syllableRevised);
        arg.pronCandidates = noteRes[i - skipCount].candidates;
        args.append(arg);
    }
    auto a = new NoteActions;
    a->editNotesWordProperties(notesToEdit, args);
    a->execute();
    historyManager->record(a);
}

void ClipEditorViewControllerPrivate::removeNotes(const QList<Note *> &notes) const {
    auto singingClip = reinterpret_cast<SingingClip *>(m_clip);
    auto a = new NoteActions;
    a->removeNotes(notes, singingClip);
    a->execute();
    historyManager->record(a);
    // updateAndNotifyCanSelectAll();
}

NotesParamsInfo ClipEditorViewControllerPrivate::buildNoteParamsInfo() const {
    auto singingClip = reinterpret_cast<SingingClip *>(m_clip);
    auto notes = singingClip->selectedNotes();
    NotesParamsInfo info;
    for (const auto &note : notes)
        info.selectedNotes.append(note);
    return info;
}