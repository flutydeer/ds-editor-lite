//
// Created by fluty on 2024/2/10.
//

#include "ClipController.h"

#include "ClipController_p.h"
#include "TrackController.h"
#include "Actions/AppModel/Note/NoteActions.h"
#include "Actions/AppModel/Param/ParamsActions.h"
#include "Interface/IClipEditorView.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Language/S2p.h"
#include "UI/Controls/Toast.h"
#include "UI/Dialogs/FillLyric/LyricDialog.h"

#include <QClipboard>
#include <QMimeData>

ClipController::ClipController() : d_ptr(new ClipControllerPrivate(this)) {
}

ClipController::~ClipController() {
    delete d_ptr;
}

void ClipController::setView(IClipEditorView *view) {
    Q_D(ClipController);
    d->m_view = view;
}

void ClipController::setClip(Clip *clip) {
    Q_D(ClipController);
    d->m_clip = clip;
    emit canSelectAllChanged(canSelectAll());
    emit hasSelectedNotesChanged(hasSelectedNotes());
}

void ClipController::copySelectedNotesWithParams() const {
    Q_D(const ClipController);
    qDebug() << "ClipController::copySelectedNotesWithParams";
    auto info = d->buildNoteParamsInfo();
    if (info.selectedNotes.count() < 0)
        return;

    // auto array = NotesParamsInfo::serializeToBinary(info);
    // auto jObj = NotesParamsInfo::serializeToJson(info);
    // QJsonDocument jDoc;
    // jDoc.setObject(jObj);
    // auto array = jDoc.toJson();
    auto data = new QMimeData;
    // data->setData(ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams), array);
    QGuiApplication::clipboard()->setMimeData(data);
    // qDebug() << QString("Copied %1 notes").arg(info.selectedNotes.count());
}

void ClipController::cutSelectedNotesWithParams() {
    copySelectedNotesWithParams();
    onDeleteSelectedNotes();
}

void ClipController::pasteNotesWithParams(const NotesParamsInfo &info, int tick) {
    Q_D(ClipController);
    // TODO: pasteNotesWithParams
    // qDebug() << "ClipController::pasteNotesWithParams";
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

bool ClipController::canSelectAll() const {
    Q_D(const ClipController);
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

bool ClipController::hasSelectedNotes() const {
    Q_D(const ClipController);
    if (!d->m_clip)
        return false;
    if (d->m_clip->clipType() != Clip::Singing)
        return false;
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    if (singingClip->notes().count() == 0)
        return false;
    auto selectedNotes = appStatus->selectedNotes;
    return !selectedNotes.get().isEmpty();
}

void ClipController::centerAt(double tick, double keyIndex) {
    Q_D(ClipController);
    if (d->m_view)
        d->m_view->centerAt(tick, keyIndex);
}

void ClipController::centerAt(const Note &note) {
    Q_D(ClipController);
    d->m_view->centerAt(note.start(), note.length(), note.keyIndex());
}

void ClipController::onClipPropertyChanged(const Clip::ClipCommonProperties &args) {
    trackController->onClipPropertyChanged(args);
}

void ClipController::onRemoveNotes(const QList<int> &notesId) {
    Q_D(ClipController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    QList<Note *> notesToDelete;
    for (const auto id : notesId)
        notesToDelete.append(singingClip->findNoteById(id));

    d->removeNotes(notesToDelete);
}

void ClipController::onInsertNote(Note *note) {
    Q_D(ClipController);
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

void ClipController::onMoveNotes(const QList<int> &notesId, int deltaTick, int deltaKey) {
    Q_D(ClipController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(singingClip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotePosition(notesToEdit, deltaTick, deltaKey, singingClip);
    a->execute();
    historyManager->record(a);
}

void ClipController::onResizeNotesLeft(const QList<int> &notesId, int deltaTick) const {
    Q_D(const ClipController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(singingClip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotesStartAndLength(notesToEdit, deltaTick, singingClip);
    a->execute();
    historyManager->record(a);
}

void ClipController::onResizeNotesRight(const QList<int> &notesId, int deltaTick) const {
    Q_D(const ClipController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(singingClip->findNoteById(id));

    auto a = new NoteActions;
    a->editNotesLength(notesToEdit, deltaTick, singingClip);
    a->execute();
    historyManager->record(a);
}

void ClipController::onAdjustPhonemeOffset(int noteId, Phonemes::Type type,
                                           const QList<int> &offsets) const {
    Q_D(const ClipController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    auto note = singingClip->findNoteById(noteId);

    auto a = new NoteActions;
    a->editNotePhonemeOffset(note, type, offsets, singingClip);
    a->execute();
    historyManager->record(a);
}

// void ClipController::onAdjustPhoneme(int noteId, const QList<Phoneme> &phonemes) const
// {
//     Q_D(const ClipController);
//     auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
//     auto note = singingClip->findNoteById(noteId);
//
//     auto a = new NoteActions;
//     a->editNotesPhoneme(note, phonemes, singingClip);
//     a->execute();
//     historyManager->record(a);
// }

void ClipController::selectNotes(const QList<int> &notesId, bool unselectOther) {
    Q_D(ClipController);
    auto selectedNotes = appStatus->selectedNotes.get();
    if (unselectOther)
        selectedNotes.clear();

    selectedNotes.append(notesId);
    appStatus->selectedNotes = selectedNotes;
    qDebug() << "select notes:" << selectedNotes;
    emit hasSelectedNotesChanged(hasSelectedNotes());
}

void ClipController::unselectNotes(const QList<int> &notesId) {
    auto selectedNotes = appStatus->selectedNotes.get();
    for (const auto id : notesId)
        selectedNotes.removeIf([=](int note) { return note == id; });
    appStatus->selectedNotes = selectedNotes;
    qDebug() << "unselect notes:" << notesId;
    emit hasSelectedNotesChanged(hasSelectedNotes());
}

void ClipController::onParamEdited(ParamInfo::Name name, const QList<Curve *> &curves) const {
    Q_D(const ClipController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    auto a = new ParamsActions;
    a->replaceParam(name, Param::Edited, curves, singingClip);
    a->execute();
    historyManager->record(a);
}

void ClipController::onNotePropertiesEdited(int noteId, const NoteDialogResult &result) {
    Q_D(ClipController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    auto note = singingClip->findNoteById(noteId);
    auto arg = Note::WordProperties::fromNote(*note);
    arg.language = result.language;
    arg.lyric = result.lyric;
    arg.pronunciation = result.pronunciation;

    // 检查音素名称是否经过编辑，如果已编辑，则需重置相应的音素时长
    auto curNameInfo = arg.phonemes.nameInfo;
    auto resultNameInfo = result.phonemeNameInfo;
    if (!curNameInfo.ahead.editedEqualsWith(resultNameInfo.ahead))
        arg.phonemes.offsetInfo.ahead.clear();
    if (!curNameInfo.normal.editedEqualsWith(resultNameInfo.normal))
        arg.phonemes.offsetInfo.normal.clear();
    if (!curNameInfo.final.editedEqualsWith(resultNameInfo.final))
        arg.phonemes.offsetInfo.final.clear();

    arg.phonemes.nameInfo = resultNameInfo;

    QList list = {note};
    QList args = {arg};

    auto a = new NoteActions;
    a->editNotesWordProperties(list, args, singingClip);
    a->execute();
    historyManager->record(a);
}

void ClipController::onDeleteSelectedNotes() {
    Q_D(const ClipController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    auto notes = ClipControllerPrivate::selectedNotesFromId(appStatus->selectedNotes, singingClip);
    d->removeNotes(notes);
    emit hasSelectedNotesChanged(false);
}

void ClipController::onSelectAllNotes() {
    Q_D(const ClipController);
    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    QList<int> notesId;
    for (const auto note : singingClip->notes())
        notesId.append(note->id());
    emit hasSelectedNotesChanged(true);
    appStatus->selectedNotes = notesId;
}

void ClipController::onFillLyric(QWidget *parent) {
    Q_D(const ClipController);
    // if (d->m_clip == nullptr)
    //     return;
    // if (d->m_clip->type() != Clip::Singing)
    //     return;

    auto singingClip = reinterpret_cast<SingingClip *>(d->m_clip);
    auto selectedNotes =
        ClipControllerPrivate::selectedNotesFromId(appStatus->selectedNotes, singingClip);

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
        inputNotes.append(inputNote);
    }

    LyricDialog lyricDialog(inputNotes, parent);
    lyricDialog.show();
    lyricDialog.setLangNotes();
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

    QList<Note::WordProperties> args;
    int skipCount = 0;
    for (int i = 0; i < notesToEdit.size(); i++) {
        auto arg = Note::WordProperties::fromNote(*selectedNotes[i]);
        if (lyricRes.skipSlur && arg.lyric == '-') {
            args.append(arg);
            skipCount++;
            continue;
        }
        arg.lyric = noteRes[i - skipCount].lyric;
        arg.language = noteRes[i - skipCount].language;
        arg.pronunciation =
            Pronunciation(noteRes[i - skipCount].syllable, noteRes[i - skipCount].syllableRevised);
        arg.pronCandidates = noteRes[i - skipCount].candidates;
        args.append(arg);
    }
    auto a = new NoteActions;
    a->editNotesWordProperties(notesToEdit, args, singingClip);
    a->execute();
    historyManager->record(a);
}

void ClipControllerPrivate::removeNotes(const QList<Note *> &notes) const {
    auto singingClip = reinterpret_cast<SingingClip *>(m_clip);
    auto a = new NoteActions;
    a->removeNotes(notes, singingClip);
    a->execute();
    historyManager->record(a);
    // updateAndNotifyCanSelectAll();
}

NotesParamsInfo ClipControllerPrivate::buildNoteParamsInfo() const {
    auto singingClip = reinterpret_cast<SingingClip *>(m_clip);
    auto notes = selectedNotesFromId(appStatus->selectedNotes, singingClip);
    NotesParamsInfo info;
    for (const auto &note : notes)
        info.selectedNotes.append(note);
    return info;
}

QList<Note *> ClipControllerPrivate::selectedNotesFromId(const QList<int> &notesId,
                                                         SingingClip *clip) {
    QList<Note *> notes;
    for (const auto &id : notesId)
        notes.append(clip->findNoteById(id));
    return notes;
}