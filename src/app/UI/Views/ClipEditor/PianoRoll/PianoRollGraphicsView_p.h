//
// Created by fluty on 24-8-4.
//

#ifndef PIANOROLLGRAPHICSVIEW_P_H
#define PIANOROLLGRAPHICSVIEW_P_H

#include "NoteLayer.h"
#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Params.h"

#include <QObject>
#include <QPointF>

class PitchEditorView;
class QMouseEvent;
class QPaintEvent;
class CMenu;
class QContextMenuEvent;
class NoteView;
class Note;
class PianoRollGraphicsView;

using namespace ClipEditorGlobal;

class PianoRollGraphicsViewPrivate : public QObject {
    Q_OBJECT
    Q_DECLARE_PUBLIC(PianoRollGraphicsView)

public:
    explicit PianoRollGraphicsViewPrivate(PianoRollGraphicsView *p) : q_ptr(p){};

    SingingClip *m_clip = nullptr;
    int m_offset = 0; // Clip 's "start" property
    QList<Note *> m_notes;

    enum MouseMoveBehavior { ResizeLeft, Move, ResizeRight, UpdateDrawingNote, EraseNotes, None };

    NoteView *m_currentEditingNote = nullptr;

    // Layers
    GraphicsLayerManager *m_layerManager = nullptr;
    NoteLayer m_noteLayer;
    PitchEditorView *m_pitchEditor = nullptr;

    bool m_selecting = false;
    bool m_selectionChangeBarrier = false;
    bool m_isEditPitchMode = false;

    // resize and move
    bool m_tempQuantizeOff = false;
    QPointF m_mouseDownPos;
    int m_mouseDownRStart = 0;
    int m_mouseDownLength = 0;
    int m_mouseDownKeyIndex = 0;
    int m_deltaTick = 0;
    int m_deltaKey = 0;
    bool m_movedBeforeMouseUp = false;
    int m_moveMaxDeltaKey = 127;
    int m_moveMinDeltaKey = 0;

    QList<int> m_notesToErase;

    PianoRollEditMode m_editMode = Select;
    MouseMoveBehavior m_mouseMoveBehavior = None;
    NoteView *m_currentDrawingNote = nullptr; // a fake note for drawing

    CMenu *buildNoteContextMenu(NoteView *noteView);

    void moveToNullClipState();
    void moveToSingingClipState(SingingClip *clip);

    void prepareForEditingNotes(QMouseEvent *event, QPointF scenePos, int keyIndex,
                                NoteView *noteItem);
    void PrepareForDrawingNote(int tick, int keyIndex);

    void handleNoteDrawn(int rStart, int length, int keyIndex) const;
    void handleNotesMoved(int deltaTick, int deltaKey) const;
    static void handleNoteLeftResized(int noteId, int deltaTick);
    static void handleNoteRightResized(int noteId, int deltaTick);

    void handleNotesErased();
    void eraseNoteFromView(NoteView *noteView);

    void updateSceneSelectionState();
    void updateOverlappedState();
    void updateNoteTimeAndKey(Note *note) const;
    void updateNoteWord(Note *note) const;
    void moveSelectedNotes(int startOffset, int keyOffset) const;
    void resetSelectedNotesOffset() const;
    void updateMoveDeltaKeyRange();
    void resetMoveDeltaKeyRange();
    void resizeLeftSelectedNote(int offset) const;
    void resizeRightSelectedNote(int offset) const;

    void updatePitch(Param::ParamType paramType, const Param &param) const;

    [[nodiscard]] double keyIndexToSceneY(double index) const;
    [[nodiscard]] double sceneYToKeyIndexDouble(double y) const;
    [[nodiscard]] int sceneYToKeyIndexInt(double y) const;
    [[nodiscard]] QList<NoteView *> selectedNoteItems() const;
    void setPitchEditMode(bool on, bool isErase);
    NoteView *noteViewAt(const QPoint &pos);
    static bool mouseInFilledRect(QPointF scenePos, NoteView* view);

    void handleNoteInserted(Note *note);
    void handleNoteRemoved(Note *note);

public slots:
    void onClipPropertyChanged();
    void onNoteChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes);
    void onNoteSelectionChanged();
    void onParamChanged(ParamBundle::ParamName name, Param::ParamType type) const;

    void onDeleteSelectedNotes() const;
    void onOpenNotePropertyDialog(int noteId);

private:
    PianoRollGraphicsView *q_ptr;
};

#endif // PIANOROLLGRAPHICSVIEW_P_H
