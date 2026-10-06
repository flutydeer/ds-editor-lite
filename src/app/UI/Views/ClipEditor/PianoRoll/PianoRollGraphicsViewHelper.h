#ifndef PIANOROLLGRAPHICSVIEWHELPER_H
#define PIANOROLLGRAPHICSVIEWHELPER_H

#include <lite/ProjectModel/AppModel/Params.h>
#include <lite/ProjectModel/SingingClipSlicer/Models/SliceResult.h>

#include <QList>
#include <QRectF>
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

    // Size of the inference-error badge drawn on silenced notes, in logical pixels
    [[nodiscard]] QSizeF noteErrorBadgeSize();
    // The inference-error badge rect inside a note rect, in the same coordinate
    // system and units as the given note rect
    [[nodiscard]] QRectF noteErrorBadgeRect(const QRectF &noteRect);
    // Short category label for a note excluded from inference, for tooltip titles
    [[nodiscard]] QString noteInferenceErrorTitle(SliceExclusionReason reason);
    // Human-readable one-line explanation for a note excluded from inference,
    // for tooltip bodies when the task reports no more specific detail
    [[nodiscard]] QString noteInferenceErrorText(const NoteInferenceErrorInfo &error);
}

#endif // PIANOROLLGRAPHICSVIEWHELPER_H
