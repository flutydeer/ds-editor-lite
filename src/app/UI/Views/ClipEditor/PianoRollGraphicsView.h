//
// Created by fluty on 2024/1/23.
//

#ifndef PIANOROLLGRAPHICSVIEW_H
#define PIANOROLLGRAPHICSVIEW_H

#include "Global/ClipEditorGlobal.h"
#include "Layers/NoteLayer.h"
#include "Model/Clip.h"
#include "Model/Params.h"
#include "UI/Views/Common/GraphicsLayerManager.h"
#include "UI/Views/Common/TimeGraphicsView.h"

class Note;
class PianoRollGraphicsScene;
class PitchEditorGraphicsItem;
class NoteGraphicsItem;

using namespace ClipEditorGlobal;

class PianoRollGraphicsView final : public TimeGraphicsView {
    Q_OBJECT

public:
    explicit PianoRollGraphicsView(PianoRollGraphicsScene *scene, QWidget *parent = nullptr);
    void setDataContext(SingingClip *clip);
    void setEditMode(PianoRollEditMode mode);
    void reset();
    [[nodiscard]] QList<int> selectedNotesId() const;
    void clearNoteSelections(NoteGraphicsItem *except = nullptr);
    void updatePitch(Param::ParamType paramType, const Param &param);

    [[nodiscard]] double topKeyIndex() const;
    [[nodiscard]] double bottomKeyIndex() const;
    void setViewportTopKey(double key);
    void setViewportCenterAt(double tick, double keyIndex);
    void setViewportCenterAtKeyIndex(double keyIndex);

signals:
    void keyIndexRangeChanged(double start, double end);

public slots:
    void onEditModeChanged(ClipEditorGlobal::PianoRollEditMode mode);
    void onSceneSelectionChanged() const;
    void onPitchEditorEditCompleted();

private slots:
    void onNoteChanged(SingingClip::NoteChangeType type, Note *note);
    void onNoteSelectionChanged();
    void onParamChanged(ParamBundle::ParamName name, Param::ParamType type);

    void onRemoveSelectedNotes() const;
    void onEditSelectedNotesLyrics() const;

private:
    SingingClip *m_clip = nullptr;
    QList<Note *> m_notes;
    enum MouseMoveBehavior { ResizeLeft, Move, ResizeRight, UpdateDrawingNote, None };
    NoteGraphicsItem *m_currentEditingNote = nullptr;

    // Layers
    GraphicsLayerManager m_layerManager;
    NoteLayer m_noteLayer;
    PitchEditorGraphicsItem *m_pitchItem;

    bool m_selecting = false;
    QList<Note *> m_cachedSelectedNotes;

    bool m_canNotifySelectedNoteChanged = true;

    // resize and move
    bool m_tempQuantizeOff = false;
    QPointF m_mouseDownPos;
    int m_mouseDownStart{};
    int m_mouseDownLength{};
    int m_mouseDownKeyIndex{};
    int m_deltaTick = 0;
    int m_deltaKey = 0;
    bool m_movedBeforeMouseUp = false;
    int m_moveMaxDeltaKey = 127;
    int m_moveMinDeltaKey = 0;

    PianoRollEditMode m_mode = Select;
    MouseMoveBehavior m_mouseMoveBehavior = None;
    NoteGraphicsItem *m_currentDrawingNote; // a fake note for drawing

    void paintEvent(QPaintEvent *event) override;
    void prepareForMovingOrResizingNotes(QMouseEvent *event, QPointF scenePos, int keyIndex,
                                         NoteGraphicsItem *noteItem);
    void PrepareForDrawingNote(int tick, int keyIndex);
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    static void handleDrawNoteCompleted(int start, int length, int keyIndex);
    void handleMoveNotesCompleted(int deltaTick, int deltaKey) const;
    static void handleResizeNoteLeftCompleted(int noteId, int deltaTick);
    static void handleResizeNoteRightCompleted(int noteId, int deltaTick);

    void updateSelectionState();
    void updateOverlappedState();
    void updateNoteTimeAndKey(Note *note);
    void updateNoteWord(Note *note);
    void moveSelectedNotes(int startOffset, int keyOffset);
    void resetSelectedNotesOffset();
    void updateMoveDeltaKeyRange();
    void resetMoveDeltaKeyRange();
    void resizeLeftSelectedNote(int offset);
    void resizeRightSelectedNote(int offset);

    [[nodiscard]] double keyIndexToSceneY(double index) const;
    [[nodiscard]] double sceneYToKeyIndexDouble(double y) const;
    [[nodiscard]] int sceneYToKeyIndexInt(double y) const;
    [[nodiscard]] QList<NoteGraphicsItem *> selectedNoteItems() const;
    void setPitchEditMode(bool on);
    NoteGraphicsItem *noteItemAt(const QPoint &pos);
    // OverlapableSerialList<Curve> buildCurves();

    void handleNoteInserted(Note *note);
    void handleNoteRemoved(Note *note);
    void handleNotePropertyChanged(Note::NotePropertyType type, Note *note);
};

#endif // PIANOROLLGRAPHICSVIEW_H
