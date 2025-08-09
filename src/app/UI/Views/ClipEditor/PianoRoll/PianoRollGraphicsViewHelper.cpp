//
// Created by fluty on 24-10-28.
//

#include "PianoRollGraphicsViewHelper.h"

#include "NoteView.h"
#include "PitchEditorView.h"
#include "PronunciationView.h"
#include "Controller/ClipController.h"
#include "Global/AppGlobal.h"
#include "Model/AppModel/DrawCurve.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppOptions/AppOptions.h"
#include "Utils/Linq.h"
#include "Utils/Log.h"
#include "Utils/MathUtils.h"

void PianoRollGraphicsViewHelper::drawNote(const int rStart, const int length, const int keyIndex) {
    qDebug() << QString("Note drawn rStart:%1 len:%2 key:%3")
                    .arg(qStrNum(rStart), qStrNum(length), qStrNum(keyIndex));
    const auto singingClip = dynamic_cast<SingingClip *>(clipController->clip());
    Q_ASSERT(singingClip);
    const auto note = new Note;
    note->setLocalStart(rStart);
    note->setLength(length);
    note->setKeyIndex(keyIndex);
    note->setLanguage(singingClip->defaultLanguage);
    note->setG2pId(singingClip->defaultG2pId);
    note->setLyric(appOptions->general()->defaultLyric);
    note->setPronunciation(Pronunciation("", ""));
    clipController->onInsertNote(note);
    clipController->selectNotes(QList({note->id()}), true);
}

void PianoRollGraphicsViewHelper::editPitch(const QList<DrawCurve *> &curves) {
    QList<Curve *> list;
    for (const auto curve : curves)
        list.append(curve);
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
        // Log::d(CLASS_NAME, "Update original pitch ");
        for (const auto curve : param.curves(Param::Original))
            if (curve->type() == Curve::Draw) {
                MathUtils::binaryInsert(drawCurves, reinterpret_cast<DrawCurve *>(curve));
            }
        pitchEditor.loadOriginal(drawCurves);
    } else {
        // Log::d(CLASS_NAME, "Update edited pitch ");
        for (const auto curve : param.curves(Param::Edited))
            if (curve->type() == Curve::Draw)
                MathUtils::binaryInsert(drawCurves, reinterpret_cast<DrawCurve *>(curve));
        pitchEditor.loadEdited(drawCurves);
    }
}