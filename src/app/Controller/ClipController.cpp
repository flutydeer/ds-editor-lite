#include "ClipController.h"
#include <lite/ProjectModel/AppModel/AppModel.h>

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "ClipController_p.h"
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"
#include "PianoRollNoteCommit.h"

#include "EditorViewController.h"
#include "TrackController.h"
#include "Global/ControllerGlobal.h"
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/Utils/NotePasteUtils.h>
#include <lite/ProjectModel/Utils/NoteResizeUtils.h>
#include "Model/AppStatus/AppStatus.h"
#include <lite/History/HistoryFocus.h>
#include <lite/GUI/Controls/Toast.h>
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Dialogs/FillLyric/LyricDialog.h"
#include "UI/Dialogs/Search/SearchDialog.h"
#include <lite/MusicBase/TimelineSnapUtils.h>

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QMimeData>
#include <QPair>
#include <QSet>

#include <algorithm>
#include <limits>

namespace {
    Automation::CoreRuntime *automationRuntime() {
        return AppContext::instance<Automation::CoreRuntime>();
    }

    Automation::CommandContext commandContext(const Automation::CoreRuntime &runtime) {
        return {.expected = runtime.documentVersion(),
                .source = Automation::InvocationSource::TrustedGui};
    }

    // Returns the canonical (time-ordered) indices of the selected notes when
    // the selection forms a single contiguous run, otherwise empty. An empty
    // result also covers a selection with gaps, duplicates, or notes that no
    // longer exist in the clip.
    QList<int> contiguousSelectionIndices(const SingingClip *clip,
                                          const QList<int> &selectedNoteIds) {
        if (!clip || selectedNoteIds.isEmpty())
            return {};

        const auto ordered = clip->notes().toList();
        QList<int> indices;
        indices.reserve(selectedNoteIds.size());
        for (const auto id : selectedNoteIds) {
            const auto it = std::find_if(ordered.cbegin(), ordered.cend(),
                                         [id](const Note *note) { return note->id() == id; });
            if (it == ordered.cend())
                return {};
            indices.append(static_cast<int>(std::distance(ordered.cbegin(), it)));
        }

        std::sort(indices.begin(), indices.end());
        for (int i = 1; i < indices.size(); ++i) {
            if (indices.at(i) != indices.at(i - 1) + 1)
                return {};
        }
        return indices;
    }

    Automation::GuiDocumentCommandContext
        guiDocumentContext(const Automation::CoreRuntime &runtime) {
        return {.expected = runtime.documentVersion(),
                .windowId = runtime.windowId(),
                .source = Automation::InvocationSource::TrustedGui};
    }

    Automation::NoteWordEditDto wordEditDto(const Note &note) {
        const auto properties = Note::WordProperties::fromNote(note);
        return {
            .noteId = Automation::NoteId(note.id()),
            .lyric = properties.lyric,
            .language = properties.language,
            .pronunciation = properties.pronunciation,
            .pronunciationCandidates = properties.pronCandidates,
            .phonemes = properties.phonemes,
        };
    }

    void revealInsertedNotes(SingingClip *clip, const Automation::MutationResult &mutation) {
        if (!clip)
            return;
        HistoryFocus focus;
        focus.kind = HistoryFocusKind::PianoRollNotes;
        focus.containerId = clip->id();
        focus.ticksAreLocal = true;
        focus.tickStart = std::numeric_limits<double>::max();
        focus.tickEnd = std::numeric_limits<double>::lowest();
        focus.valueStart = std::numeric_limits<double>::max();
        focus.valueEnd = std::numeric_limits<double>::lowest();
        for (const auto &object : mutation.affectedObjects) {
            if (object.kind != Automation::ObjectKind::Note)
                continue;
            const auto *note = clip->findNoteById(object.value);
            if (!note)
                continue;
            focus.objectIds.append(note->id());
            focus.tickStart = qMin(focus.tickStart, static_cast<double>(note->localStart()));
            focus.tickEnd =
                qMax(focus.tickEnd, static_cast<double>(note->localStart() + note->length()));
            focus.valueStart = qMin(focus.valueStart, static_cast<double>(note->keyIndex()));
            focus.valueEnd = qMax(focus.valueEnd, static_cast<double>(note->keyIndex()));
        }
        if (!focus.objectIds.isEmpty())
            editorViewController->revealFocus(focus);
    }
}

ClipController::ClipController(QObject *parent)
    : QObject(parent), d_ptr(new ClipControllerPrivate(this)) {
}

ClipController::~ClipController() {
    delete d_ptr;
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(ClipController)

Clip *ClipController::clip() {
    Q_D(ClipController);
    return d->m_clip;
}

void ClipController::setClip(Clip *clip) {
    Q_D(ClipController);
    d->m_clip = clip;
    emit canSelectAllChanged(canSelectAll());
    emit hasSelectedNotesChanged(hasSelectedNotes());
}

void ClipController::copySelectedNotesWithParams() const {
    Q_D(const ClipController);
    const auto info = d->buildNoteParamsInfo();
    if (info.selectedNotes.isEmpty())
        return;

    const auto jObj = NotesParamsInfo::serializeToJson(info);
    QJsonDocument jDoc(jObj);
    const auto array = jDoc.toJson(QJsonDocument::Compact);

    const auto data = new QMimeData;
    data->setData(ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams), array);
    QGuiApplication::clipboard()->setMimeData(data);
    qDebug() << QString("Copied %1 notes").arg(info.selectedNotes.count());
}

void ClipController::cutSelectedNotesWithParams() {
    copySelectedNotesWithParams();
    onDeleteSelectedNotes();
}

void ClipController::pasteNotesWithParams(const NotesParamsInfo &info, int tick) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    Track *targetTrack = nullptr;
    auto *targetClip = appModel->findClipById(appStatus->activeClipId.get(), targetTrack);
    if (!targetClip || targetClip->clipType() != Clip::Singing || !targetTrack)
        return;

    const auto &srcNotes = info.selectedNotes;
    if (srcNotes.isEmpty())
        return;

    auto *singingClip = static_cast<SingingClip *>(targetClip);

    const auto quantize = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
    const auto snappedTick = TimelineSnapUtils::snapNearest(tick, quantize, appModel->timeline());

    int minStart = srcNotes.first()->localStart();
    int maxEnd = minStart + srcNotes.first()->length();
    for (const auto note : srcNotes) {
        minStart = qMin(minStart, note->localStart());
        maxEnd = qMax(maxEnd, note->localStart() + note->length());
    }

    auto clipProperties = Clip::ClipCommonProperties(*singingClip);
    const auto pastePlan =
        NotePasteUtils::plan(clipProperties, tick, snappedTick, {.start = minStart, .end = maxEnd});

    QList<Automation::NoteDraftDto> newNotes;
    for (const auto srcNote : srcNotes) {
        if (!srcNote)
            continue;
        auto note = Automation::noteDraftDto(*srcNote);
        note.localStart += pastePlan.offset;
        newNotes.append(std::move(note));
    }

    const auto result = runtime->notes().insertNotes(
        commandContext(*runtime), Automation::ClipId(singingClip->id()), newNotes);
    if (result && result.get().changed)
        revealInsertedNotes(singingClip, result.get());
    emit hasSelectedNotesChanged(hasSelectedNotes());
}

bool ClipController::canSelectAll() const {
    Q_D(const ClipController);
    if (!d->m_clip)
        return false;
    if (d->m_clip->clipType() != Clip::Singing)
        return false;
    const auto singingClip = static_cast<SingingClip *>(d->m_clip);
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
    const auto singingClip = static_cast<SingingClip *>(d->m_clip);
    if (singingClip->notes().count() == 0)
        return false;
    const auto selectedNotes = appStatus->selectedNotes;
    return !selectedNotes.get().isEmpty();
}

void ClipController::onClipPropertyChanged(const Clip::ClipCommonProperties &args) {
    trackController->onClipPropertyChanged(args);
}

void ClipController::onRemoveNotes(const QList<int> &notesId) {
    Q_D(ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;
    QList<Automation::NoteId> ids;
    ids.reserve(notesId.size());
    for (const auto id : notesId)
        ids.append(Automation::NoteId(id));
    runtime->notes().removeNotes(commandContext(*runtime), Automation::ClipId(d->m_clip->id()),
                                 ids);
}

std::optional<Automation::NoteId> ClipController::onInsertNote(Automation::NoteDraftDto note) {
    Q_D(ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return std::nullopt;
    return PianoRollNoteCommit::insert(*runtime, Automation::ClipId(d->m_clip->id()),
                                       std::move(note));
}

std::optional<Automation::NoteId> ClipController::onSplitNote(const Automation::NoteId noteId,
                                                              Automation::NoteDraftDto newNote,
                                                              const int newLength) const {
    Q_D(const ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return std::nullopt;
    return PianoRollNoteCommit::split(*runtime, Automation::ClipId(d->m_clip->id()), noteId,
                                      std::move(newNote), newLength);
}

void ClipController::onMoveNotes(const QList<int> &notesId, const int deltaTick,
                                 const int deltaKey) {
    Q_D(ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;
    QList<Automation::NoteId> ids;
    for (const auto id : notesId)
        ids.append(Automation::NoteId(id));
    runtime->notes().moveNotes(commandContext(*runtime), Automation::ClipId(d->m_clip->id()), ids,
                               deltaTick, deltaKey);
}

void ClipController::onResizeNotesLeft(const QList<int> &notesId, const int deltaTick,
                                       const int minimumLength) const {
    Q_D(const ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;
    QList<Automation::NoteId> ids;
    for (const auto id : notesId)
        ids.append(Automation::NoteId(id));
    runtime->notes().resizeNotesLeft(commandContext(*runtime), Automation::ClipId(d->m_clip->id()),
                                     ids, deltaTick, minimumLength);
}

void ClipController::onResizeNotesRight(const QList<int> &notesId, const int deltaTick,
                                        const int minimumLength) const {
    Q_D(const ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;
    QList<Automation::NoteId> ids;
    for (const auto id : notesId)
        ids.append(Automation::NoteId(id));
    runtime->notes().resizeNotesRight(commandContext(*runtime), Automation::ClipId(d->m_clip->id()),
                                      ids, deltaTick, minimumLength);
}

void ClipController::onAdjustPhonemeOffset(const int noteId, const QList<int> &offsets) const {
    Q_D(const ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;
    runtime->notes().setPhonemeOffsets(commandContext(*runtime),
                                       Automation::ClipId(d->m_clip->id()),
                                       Automation::NoteId(noteId), offsets);
}

void ClipController::onResetPhonemeOffsets(QWidget *parent) const {
    Q_D(const ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;
    auto *singingClip = static_cast<SingingClip *>(d->m_clip);
    const auto selectedNotes =
        ClipControllerPrivate::selectedNotesFromId(appStatus->selectedNotes, singingClip);
    if (selectedNotes.isEmpty())
        return;

    // Compute the full cascade reset closure up front (pure function of current
    // edited offsets + baseline, so the order of reset does not matter).
    const auto closure =
        SingingClipPhonemeNormalizer::collectCascadeResetRoots(*singingClip, selectedNotes);

    // Spillover: words reset beyond the user's selection, so ask before executing.
    QSet<const Note *> selectedSet;
    for (const auto note : selectedNotes) {
        if (note)
            selectedSet.insert(note);
    }
    int spilloverCount = 0;
    QStringList spilloverNames;
    for (const auto root : closure) {
        if (!root || selectedSet.contains(root))
            continue;
        spilloverCount++;
        spilloverNames.append(tr("%1 (%2)").arg(
            root->lyric().trimmed().isEmpty() ? QStringLiteral("?") : root->lyric().trimmed(),
            appModel->getBarBeatTickTime(root->globalStart())));
    }
    if (spilloverCount > 0) {
        constexpr int cancelButtonId = 0;
        constexpr int resetButtonId = 1;
        MessageDialog dialog(tr("Reset phoneme durations"),
                             tr("To avoid phoneme overlap, %1 adjacent word(s) will also be "
                                "reset:\n%2\n\nReset them?")
                                 .arg(QString::number(spilloverCount), spilloverNames.join('\n')),
                             parent);
        dialog.addAccentButton(tr("Reset"), resetButtonId);
        dialog.addButton(tr("Cancel"), cancelButtonId);
        if (dialog.exec() != resetButtonId)
            return;
    }

    QList<Automation::NoteId> ids;
    for (const auto root : closure)
        ids.append(Automation::NoteId(root->id()));
    runtime->notes().resetPhonemeOffsets(commandContext(*runtime),
                                         Automation::ClipId(singingClip->id()), ids);
}

//

void ClipController::selectNotes(const QList<int> &notesId, const bool unselectOther) {
    Q_D(ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;
    auto selectedNotes = appStatus->selectedNotes.get();
    if (unselectOther)
        selectedNotes.clear();

    selectedNotes.append(notesId);
    QList<Automation::NoteId> ids;
    ids.reserve(selectedNotes.size());
    for (const auto id : selectedNotes)
        ids.append(Automation::NoteId(id));
    const auto result = runtime->facade().setSelectedNotes(
        guiDocumentContext(*runtime), Automation::ClipId(d->m_clip->id()), ids);
    if (result)
        emit hasSelectedNotesChanged(hasSelectedNotes());
}

void ClipController::unselectNotes(const QList<int> &notesId) {
    Q_D(ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;
    auto selectedNotes = appStatus->selectedNotes.get();
    for (const auto id : notesId)
        selectedNotes.removeIf([=](const int note) { return note == id; });
    QList<Automation::NoteId> ids;
    ids.reserve(selectedNotes.size());
    for (const auto id : selectedNotes)
        ids.append(Automation::NoteId(id));
    const auto result = runtime->facade().setSelectedNotes(
        guiDocumentContext(*runtime), Automation::ClipId(d->m_clip->id()), ids);
    if (result)
        emit hasSelectedNotesChanged(hasSelectedNotes());
}

void ClipController::onParamEdited(const ParamInfo::Name name, const QList<Curve *> &curves) const {
    Q_D(const ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;
    QList<Automation::CurveDraftDto> drafts;
    drafts.reserve(curves.size());
    for (const auto *curve : curves) {
        if (curve && (curve->type() == Curve::Draw || curve->type() == Curve::Anchor))
            drafts.append(Automation::curveDraftDto(*curve));
    }
    runtime->parameters().replaceParameter(
        commandContext(*runtime), Automation::ClipId(d->m_clip->id()), name, Param::Edited, drafts);
}

void ClipController::onQuantizeNotes(const int quantize, const bool quantizeStart,
                                     const bool quantizeLength) const {
    Q_D(const ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;
    const auto *singingClip = static_cast<SingingClip *>(d->m_clip);
    QList<Automation::NoteId> noteIds;
    const auto selectedIds = appStatus->selectedNotes.get();
    if (!selectedIds.isEmpty()) {
        noteIds.reserve(selectedIds.size());
        for (const auto id : selectedIds)
            noteIds.append(Automation::NoteId(id));
    } else {
        const auto &allNotes = singingClip->notes();
        noteIds.reserve(allNotes.count());
        for (const auto &note : allNotes)
            noteIds.append(Automation::NoteId(note->id()));
    }
    runtime->notes().quantizeNotes(commandContext(*runtime), Automation::ClipId(singingClip->id()),
                                   noteIds, quantize, quantizeStart, quantizeLength);
}

void ClipController::onNoteLanguagesEdited(const QList<int> &noteIds, const QString &language) {
    Q_D(ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;

    auto *singingClip = static_cast<SingingClip *>(d->m_clip);
    QList<Automation::NoteWordEditDto> edits;
    for (const auto noteId : noteIds) {
        auto *note = singingClip->findNoteById(noteId);
        if (!note)
            continue;
        auto edit = wordEditDto(*note);
        edit.language = language;
        edits.append(std::move(edit));
    }
    runtime->notes().setWordProperties(commandContext(*runtime),
                                       Automation::ClipId(singingClip->id()), edits);
}

void ClipController::onNoteLyricEdited(const int noteId, const QString &lyric) {
    Q_D(ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;

    auto *singingClip = static_cast<SingingClip *>(d->m_clip);
    auto *note = singingClip->findNoteById(noteId);
    if (!note)
        return;

    auto edit = wordEditDto(*note);
    edit.lyric = lyric;
    if (Note::isSlurLyric(lyric) || Note::isSyllabificationLyric(lyric))
        edit.phonemes = {};
    runtime->notes().setWordProperties(commandContext(*runtime),
                                       Automation::ClipId(singingClip->id()), {edit});
}

bool ClipController::canShiftWordProperties(const QList<int> &selectedNoteIds) const {
    Q_D(const ClipController);
    if (!d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return false;
    return !contiguousSelectionIndices(static_cast<const SingingClip *>(d->m_clip), selectedNoteIds)
                .isEmpty();
}

// "Move Lyrics Backward": carry the whole word bundle (lyric, language,
// pronunciation, pronunciation candidates) from each source note to the note
// `count` positions later, and collapse the selection into "-" slurs. Because
// every affected note's word input changes, the facade resets its manual
// phoneme edits (name sequence and duration offsets) back to the model
// baseline; lyrics that fall past the last note are dropped.
void ClipController::onShiftWordPropertiesBackward(const QList<int> &selectedNoteIds) {
    Q_D(ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;

    auto *singingClip = static_cast<SingingClip *>(d->m_clip);
    const auto indices = contiguousSelectionIndices(singingClip, selectedNoteIds);
    if (indices.isEmpty())
        return;

    const auto ordered = singingClip->notes().toList();
    const int start = indices.first();
    const int count = indices.size();

    QList<Automation::NoteWordEditDto> edits;
    edits.reserve(ordered.size() - start);
    for (int p = start; p < ordered.size(); ++p) {
        auto *note = ordered.at(p);
        Automation::NoteWordEditDto edit;
        if (p < start + count) {
            // Selection notes collapse into "-" (slur) lyrics.
            edit = wordEditDto(*note);
            edit.lyric = QStringLiteral("-");
        } else {
            // Carry the whole word bundle from `count` positions back.
            const auto *source = ordered.at(p - count);
            edit = wordEditDto(*source);
            edit.noteId = Automation::NoteId(note->id());
            edit.replacePronunciation = true;
            edit.replacePronunciationCandidates = true;
        }
        edits.append(std::move(edit));
    }
    runtime->notes().setWordProperties(commandContext(*runtime),
                                       Automation::ClipId(singingClip->id()), edits);
}

void ClipController::onNotePronunciationEdited(const int noteId, const QString &pronunciation) {
    Q_D(ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;

    auto *singingClip = static_cast<SingingClip *>(d->m_clip);
    auto *note = singingClip->findNoteById(noteId);
    if (!note)
        return;

    auto edit = wordEditDto(*note);
    edit.pronunciation.edited = pronunciation;
    edit.replacePronunciation = true;
    runtime->notes().setWordProperties(commandContext(*runtime),
                                       Automation::ClipId(singingClip->id()), {edit});
}

void ClipController::onNotePhonemesEdited(const int noteId,
                                          const QList<PhonemeName> &phonemeNames) {
    Q_D(ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;

    auto *singingClip = static_cast<SingingClip *>(d->m_clip);
    auto *note = singingClip->findNoteById(noteId);
    if (!note || !note->canEditPhonemes())
        return;

    auto edit = wordEditDto(*note);
    edit.phonemes.nameSeq.edited = phonemeNames;
    edit.phonemes.offsetSeq.clear();
    runtime->notes().setWordProperties(commandContext(*runtime),
                                       Automation::ClipId(singingClip->id()), {edit});
}

void ClipController::onDeleteSelectedNotes() {
    Q_D(const ClipController);
    if (!d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;
    const auto ids = appStatus->selectedNotes.get();
    onRemoveNotes(ids);
    if (!ids.isEmpty())
        selectNotes({}, true);
}

void ClipController::onSelectAllNotes() {
    Q_D(const ClipController);
    auto *runtime = automationRuntime();
    if (!runtime || !d->m_clip || d->m_clip->clipType() != Clip::Singing)
        return;
    const auto singingClip = static_cast<SingingClip *>(d->m_clip);
    QList<Automation::NoteId> noteIds;
    for (const auto note : singingClip->notes())
        noteIds.append(Automation::NoteId(note->id()));
    const auto result = runtime->facade().setSelectedNotes(
        guiDocumentContext(*runtime), Automation::ClipId(singingClip->id()), noteIds);
    if (result)
        emit hasSelectedNotesChanged(!noteIds.isEmpty());
}

void ClipController::onFillLyric(QWidget *parent) {
    Q_D(const ClipController);

    auto singingClip = static_cast<SingingClip *>(d->m_clip);
    auto selectedNotes =
        ClipControllerPrivate::selectedNotesFromId(appStatus->selectedNotes, singingClip);

    int slurCount = 0;
    QList<Note *> inputNotes;
    for (const auto &note : selectedNotes) {
        if (note->isSlur())
            slurCount++;
        const auto inputNote = new Note();
        inputNote->setLyric(note->lyric());
        inputNote->setPronunciation(note->pronunciation());
        inputNote->setPronCandidates(note->pronCandidates());
        inputNote->setLanguage(note->language());
        inputNotes.append(inputNote);
    }

    const auto singerInfo = singingClip->singerInfo();
    QStringList priorityLanguage = {singingClip->defaultLanguage()};
    if (!singerInfo.isEmpty()) {
        priorityLanguage.append(singerInfo.defaultLanguage());
        for (const auto &lang : singerInfo.languages()) {
            priorityLanguage.append(lang.id());
        }
    }

    const auto singer = singerInfo.resolutionState() == ResolutionState::Resolved
                            ? singerInfo.identifier()
                            : SingerIdentifier{};
    LyricDialog lyricDialog(singingClip, inputNotes, singer, priorityLanguage, parent);
    lyricDialog.show();
    lyricDialog.setLangNotes();
    lyricDialog.exec();

    if (lyricDialog.result() != QDialog::Accepted)
        return;

    const auto lyricRes = lyricDialog.noteResult();
    auto noteRes = lyricRes.langNotes;
    auto notesToEdit = selectedNotes;
    if (!lyricRes.skipSlur)
        slurCount = 0;
    if (noteRes.count() + slurCount > selectedNotes.count()) {
    } else if (noteRes.count() + slurCount < selectedNotes.count()) {
        auto i = noteRes.count() + slurCount;
        auto n = selectedNotes.count() - noteRes.count() - slurCount;
        notesToEdit.remove(i, n);
    }

    QList<Automation::NoteWordEditDto> edits;
    int skipCount = 0;
    for (int i = 0; i < notesToEdit.size(); i++) {
        auto arg = Note::WordProperties::fromNote(*selectedNotes[i]);
        if (lyricRes.skipSlur && selectedNotes[i]->isSlur()) {
            auto edit = wordEditDto(*selectedNotes[i]);
            edits.append(std::move(edit));
            skipCount++;
            continue;
        }
        if (i - skipCount >= noteRes.count()) {
            // 如果输出的音符数量小于输入的音符数量，则跳过剩余的音符
            break;
        }
        arg.lyric = noteRes[i - skipCount].lyric;
        if (Note::isSlurLyric(arg.lyric) || Note::isSyllabificationLyric(arg.lyric))
            arg.phonemes = {};
        const auto &noteResult = noteRes[i - skipCount];
        arg.language = noteResult.language;
        arg.pronunciation = Pronunciation(noteResult.syllable, noteResult.syllableRevised);
        arg.pronCandidates = noteResult.candidates;
        auto edit = wordEditDto(*selectedNotes[i]);
        edit.lyric = arg.lyric;
        edit.language = arg.language;
        edit.pronunciation = arg.pronunciation;
        edit.pronunciationCandidates = arg.pronCandidates;
        edit.phonemes = arg.phonemes;
        edit.replacePronunciation = noteResult.revised;
        edit.replacePronunciationCandidates = true;
        edits.append(std::move(edit));
    }
    if (auto *runtime = automationRuntime()) {
        runtime->notes().setWordProperties(commandContext(*runtime),
                                           Automation::ClipId(singingClip->id()), edits);
    }
}

void ClipController::onSearchLyric(QWidget *parent) {
    Q_D(const ClipController);
    const auto singingClip = static_cast<SingingClip *>(d->m_clip);
    SearchDialog searchDialog(singingClip, parent);
    searchDialog.show();
    searchDialog.exec();
}

NotesParamsInfo ClipControllerPrivate::buildNoteParamsInfo() const {
    const auto singingClip = static_cast<SingingClip *>(m_clip);
    auto notes = selectedNotesFromId(appStatus->selectedNotes, singingClip);
    NotesParamsInfo info;
    for (const auto &note : notes)
        info.selectedNotes.append(note);
    return info;
}

QList<Note *> ClipControllerPrivate::selectedNotesFromId(const QList<int> &notesId,
                                                         const SingingClip *clip) {
    QSet<int> selectedIds;
    selectedIds.reserve(notesId.size());
    for (const auto id : notesId)
        selectedIds.insert(id);

    QList<Note *> notes;
    notes.reserve(selectedIds.size());
    for (auto *note : clip->notes()) {
        if (selectedIds.contains(note->id()))
            notes.append(note);
    }
    return notes;
}
