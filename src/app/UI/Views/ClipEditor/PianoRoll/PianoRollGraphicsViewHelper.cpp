#include "PianoRollGraphicsViewHelper.h"
#include <lite/ProjectModel/AppModel/AppModel.h>

#include "NoteView.h"
#include "PitchEditorView.h"
#include "EditPitchAnchorHandler.h"
#include "PronunciationView.h"
#include "Controller/ClipController.h"
#include "Global/AppGlobal.h"
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/AnchorEditor/AnchorEditUtils.h"
#include <lite/Support/Linq.h>
#include <lite/Support/MathUtils.h>
#include <lite/MusicBase/TimelineSnapUtils.h>

QString PianoRollGraphicsViewHelper::defaultLyricForNewNote(const SingingClip *clip) {
    const auto language =
        clip ? clip->effectiveDefaultLanguage() : appOptions->general()->defaultSingingLanguage;
    return appOptions->general()->defaultLyricForLanguage(language);
}

bool PianoRollGraphicsViewHelper::drawNote(const int rStart, const int length, const int keyIndex) {
    qDebug() << "Note drawn rStart:" << rStart << "len:" << length << "key:" << keyIndex;
    const auto singingClip = dynamic_cast<SingingClip *>(clipController->clip());
    if (!singingClip)
        return false;
    Automation::NoteDraftDto note;
    note.localStart = rStart;
    note.length = length;
    note.keyIndex = keyIndex;
    note.lyric = defaultLyricForNewNote(singingClip);
    note.pronunciation = Pronunciation("", "");
    const auto createdId = clipController->onInsertNote(std::move(note));
    if (!createdId)
        return false;
    clipController->selectNotes({createdId->value()}, true);
    return true;
}

bool PianoRollGraphicsViewHelper::splitNote(const int noteId, const int tick) {
    const auto singingClip = dynamic_cast<SingingClip *>(clipController->clip());
    if (!singingClip)
        return false;

    const auto note = singingClip->findNoteById(noteId);
    if (!note)
        return false;

    const auto quantizedTickLength = TimelineSnapUtils::quantizeStep(
        appStatus->pianoRollQuantize, !appStatus->pianoRollQuantizeEnabled);
    const auto snappedTick =
        TimelineSnapUtils::snapNearest(tick, quantizedTickLength, appModel->timeline());
    const auto splitPos = snappedTick - singingClip->start();
    const auto noteLocalStart = note->localStart();
    const auto noteLocalEnd = noteLocalStart + note->length();

    if (splitPos <= noteLocalStart || splitPos >= noteLocalEnd)
        return false;

    editSessionManager->beginTransaction(AppStatus::EditObjectType::Note, singingClip->id(), {},
                                         {noteId}, {}, {}, true);
    appStatus->currentEditObject = AppStatus::EditObjectType::Note;

    const auto firstPartLength = splitPos - noteLocalStart;
    const auto secondPartLength = noteLocalEnd - splitPos;

    Automation::NoteDraftDto newNote;
    newNote.localStart = splitPos;
    newNote.length = secondPartLength;
    newNote.keyIndex = note->keyIndex();
    newNote.centShift = note->centShift();
    newNote.language = note->language();
    newNote.lyric = QStringLiteral("-");
    newNote.pronunciation = note->pronunciation();

    const auto createdId = clipController->onSplitNote(Automation::NoteId(note->id()),
                                                       std::move(newNote), firstPartLength);
    if (createdId)
        clipController->selectNotes({createdId->value()}, true);

    editSessionManager->endActiveTransaction(createdId ? EditSessionEndReason::Commit
                                                       : EditSessionEndReason::Discard);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    return createdId.has_value();
}

void PianoRollGraphicsViewHelper::editPitch(const QList<DrawCurve *> &curves) {
    auto *clip = dynamic_cast<SingingClip *>(clipController->clip());
    if (!clip)
        return;

    const auto &existing = clip->params.getParamByName(ParamInfo::Pitch)->curves(Param::Edited);
    auto list = AnchorEditor::replaceDrawCurves(existing, curves);
    clipController->onParamEdited(ParamInfo::Pitch, list);
    qDeleteAll(list);
}

NoteView *PianoRollGraphicsViewHelper::buildNoteView(const Note &note) {
    const auto noteView = new NoteView(note.id());
    noteView->setPronunciationView(new PronunciationView(note.id()));
    noteView->setRStart(note.localStart());
    noteView->setLength(note.length());
    noteView->setKeyIndex(note.keyIndex());
    noteView->setLyric(note.lyric());
    const auto original = note.pronunciation().original;
    const auto edited = note.pronunciation().edited;
    const auto isEdited = note.pronunciation().isEdited();
    noteView->setPronunciation(isEdited ? edited : original, isEdited);
    noteView->setOverlapped(note.overlapped());
    return noteView;
}

void PianoRollGraphicsViewHelper::updateNoteTimeAndKey(NoteView &noteView, const Note &note) {
    noteView.setRStart(note.localStart());
    noteView.setLength(note.length());
    noteView.setKeyIndex(note.keyIndex());
}

void PianoRollGraphicsViewHelper::updateNoteWord(NoteView &noteView, const Note &note) {
    noteView.setLyric(note.lyric());
    const auto original = note.pronunciation().original;
    const auto edited = note.pronunciation().edited;
    const auto isEdited = note.pronunciation().isEdited();
    noteView.setPronunciation(isEdited ? edited : original, isEdited);
}

void PianoRollGraphicsViewHelper::updatePitch(const Param::Type paramType, const Param &param,
                                              PitchEditorView &pitchEditor) {
    QList<DrawCurve *> drawCurves;
    if (paramType == Param::Original) {
        for (const auto curve : param.curves(Param::Original))
            if (curve->type() == Curve::Draw) {
                MathUtils::binaryInsert(drawCurves, static_cast<DrawCurve *>(curve));
            }
        pitchEditor.loadOriginal(drawCurves);
    } else {
        for (const auto curve : param.curves(Param::Edited))
            if (curve->type() == Curve::Draw)
                MathUtils::binaryInsert(drawCurves, static_cast<DrawCurve *>(curve));
        pitchEditor.loadEdited(drawCurves);
    }
}

void PianoRollGraphicsViewHelper::updateAnchorPitch(const Param &param,
                                                    EditPitchAnchorHandler &handler) {
    QList<AnchorCurve *> anchorCurves;
    for (const auto curve : param.curves(Param::Edited)) {
        if (curve->type() == Curve::Anchor)
            anchorCurves.append(static_cast<AnchorCurve *>(curve));
    }
    handler.loadFromModel(anchorCurves);
}
