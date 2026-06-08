//
// Created by fluty on 24-10-28.
//

#include "PianoRollGraphicsViewHelper.h"

#include "NoteView.h"
#include "PitchEditorView.h"
#include "EditPitchAnchorHandler.h"
#include "PronunciationView.h"
#include "Controller/ClipController.h"
#include "Global/AppGlobal.h"
#include "Model/AppModel/DrawCurve.h"
#include "Model/AppModel/AnchorCurve.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"
#include "Utils/TimelineSnapUtils.h"

void PianoRollGraphicsViewHelper::drawNote(const int rStart, const int length, const int keyIndex) {
    qDebug() << "Note drawn rStart:" << rStart << "len:" << length << "key:" << keyIndex;
    const auto singingClip = dynamic_cast<SingingClip *>(clipController->clip());
    Q_ASSERT(singingClip);
    const auto note = new Note;
    note->setLocalStart(rStart);
    note->setLength(length);
    note->setKeyIndex(keyIndex);
    note->setLanguage(singingClip->defaultLanguage());
    note->setLyric(appOptions->general()->defaultLyricForLanguage(singingClip->defaultLanguage()));
    note->setPronunciation(Pronunciation("", ""));
    clipController->onInsertNote(note);
    clipController->selectNotes(QList({note->id()}), true);
}

void PianoRollGraphicsViewHelper::splitNote(const int noteId, const int tick) {
    const auto singingClip = dynamic_cast<SingingClip *>(clipController->clip());
    if (!singingClip)
        return;

    const auto note = singingClip->findNoteById(noteId);
    if (!note)
        return;

    const auto quantizedTickLength =
        TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
    const auto snappedTick = TimelineSnapUtils::snapNearest(tick, quantizedTickLength);
    const auto splitPos = snappedTick - singingClip->start();
    const auto noteLocalStart = note->localStart();
    const auto noteLocalEnd = noteLocalStart + note->length();

    if (splitPos <= noteLocalStart || splitPos >= noteLocalEnd)
        return;

    editSessionManager->beginTransaction(AppStatus::EditObjectType::Note, singingClip->id(), {},
                                         {noteId}, {}, {}, true);
    appStatus->currentEditObject = AppStatus::EditObjectType::Note;

    const auto firstPartLength = splitPos - noteLocalStart;
    const auto secondPartLength = noteLocalEnd - splitPos;

    const auto newNote = new Note(singingClip);
    newNote->setLocalStart(splitPos);
    newNote->setLength(secondPartLength);
    newNote->setKeyIndex(note->keyIndex());
    newNote->setCentShift(note->centShift());
    newNote->setLanguage(note->language());
    newNote->setLyric("-");
    newNote->setPronunciation(note->pronunciation());

    clipController->onSplitNote(note->id(), newNote, firstPartLength);
    clipController->selectNotes(QList({newNote->id()}), true);

    editSessionManager->endActiveTransaction(EditSessionEndReason::Commit);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void PianoRollGraphicsViewHelper::editPitch(const QList<DrawCurve *> &curves) {
    QList<Curve *> list;
    for (const auto curve : curves)
        list.append(curve);

    auto *clip = dynamic_cast<SingingClip *>(clipController->clip());
    if (clip) {
        const auto &existing = clip->params.getParamByName(ParamInfo::Pitch)->curves(Param::Edited);
        for (auto *curve : existing) {
            if (curve->type() == Curve::Anchor)
                list.append(new AnchorCurve(*dynamic_cast<AnchorCurve *>(curve)));
        }
    }

    clipController->onParamEdited(ParamInfo::Pitch, list);
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
            anchorCurves.append(new AnchorCurve(*dynamic_cast<AnchorCurve *>(curve)));
    }
    handler.loadFromModel(anchorCurves);
}