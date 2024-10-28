//
// Created by fluty on 24-10-28.
//

#ifndef PIANOROLLGRAPHICSVIEWHELPER_H
#define PIANOROLLGRAPHICSVIEWHELPER_H

#include "Model/AppModel/Params.h"

#include <QList>

class PitchEditorView;
class DrawCurve;
class QWidget;
class CMenu;
class Note;
class NoteView;
class SingingClip;

namespace PianoRollGraphicsViewHelper {
    void drawNote(int rStart, int length, int keyIndex);
    void editPitch(const QList<DrawCurve *> &curves);
    NoteView *buildNoteView(const Note &note);
    void updateNoteTimeAndKey(NoteView &noteView, const Note &note);
    void updateNoteWord(NoteView &noteView, const Note &note);
    void updatePitch(Param::Type paramType, const Param &param, PitchEditorView &pitchEditor);
}

#endif // PIANOROLLGRAPHICSVIEWHELPER_H
