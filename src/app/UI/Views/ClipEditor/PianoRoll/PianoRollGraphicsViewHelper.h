#ifndef PIANOROLLGRAPHICSVIEWHELPER_H
#define PIANOROLLGRAPHICSVIEWHELPER_H

#include <lite/ProjectModel/AppModel/Params.h>

#include <QList>
#include <QString>

class PitchEditorView;
class EditPitchAnchorHandler;
class DrawCurve;
class AnchorCurve;
class QWidget;
class CMenu;
class Note;
class NoteView;
class SingingClip;

namespace PianoRollGraphicsViewHelper {
    QString defaultLyricForNewNote(const SingingClip *clip);
    [[nodiscard]] bool drawNote(int rStart, int length, int keyIndex);
    [[nodiscard]] bool splitNote(int noteId, int tick);
    void editPitch(const QList<DrawCurve *> &curves);
    NoteView *buildNoteView(const Note &note);
    void updateNoteTimeAndKey(NoteView &noteView, const Note &note);
    void updateNoteWord(NoteView &noteView, const Note &note);
    void updatePitch(Param::Type paramType, const Param &param, PitchEditorView &pitchEditor);
    void updateAnchorPitch(const Param &param, EditPitchAnchorHandler &handler);
}

#endif // PIANOROLLGRAPHICSVIEWHELPER_H
