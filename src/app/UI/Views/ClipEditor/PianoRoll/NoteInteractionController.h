#ifndef NOTEINTERACTIONCONTROLLER_H
#define NOTEINTERACTIONCONTROLLER_H

#include <QObject>
#include <QPointF>

class NoteView;
class Note;
class QMouseEvent;
class PianoRollSelectionModel;
class PianoRollGraphicsView;

class NoteInteractionController : public QObject {
    Q_OBJECT

public:
    enum MouseMoveBehavior { ResizeLeft, Move, ResizeRight, None };

    explicit NoteInteractionController(PianoRollSelectionModel *selectionModel,
                                       PianoRollGraphicsView *view, QObject *parent = nullptr);

    // State access
    [[nodiscard]] bool isMouseDown() const {
        return m_mouseDown;
    }

    [[nodiscard]] Qt::MouseButton mouseDownButton() const {
        return m_mouseDownButton;
    }

    [[nodiscard]] MouseMoveBehavior mouseMoveBehavior() const {
        return m_mouseMoveBehavior;
    }

    [[nodiscard]] bool isEditPitchMode() const {
        return m_isEditPitchMode;
    }

    [[nodiscard]] bool movedBeforeMouseUp() const {
        return m_movedBeforeMouseUp;
    }

    [[nodiscard]] NoteView *currentEditingNote() const {
        return m_currentEditingNote;
    }

    // State setters
    void setMouseDown(bool down, Qt::MouseButton button = Qt::NoButton);

    void setMouseDownPos(const QPointF &pos) {
        m_mouseDownPos = pos;
    }

    void setMouseDownNoteParams(int rStart, int length, int keyIndex);

    void setTempQuantizeOff(bool off) {
        m_tempQuantizeOff = off;
    }

    [[nodiscard]] bool tempQuantizeOff() const {
        return m_tempQuantizeOff;
    }

    void setMouseMoveBehavior(MouseMoveBehavior behavior) {
        m_mouseMoveBehavior = behavior;
    }

    void setEditPitchMode(bool on) {
        m_isEditPitchMode = on;
    }

    void setMovedBeforeMouseUp(bool moved) {
        m_movedBeforeMouseUp = moved;
    }

    void setCurrentEditingNote(NoteView *view) {
        m_currentEditingNote = view;
    }

    // Down state
    [[nodiscard]] int mouseDownRStart() const {
        return m_mouseDownRStart;
    }

    [[nodiscard]] int mouseDownLength() const {
        return m_mouseDownLength;
    }

    [[nodiscard]] int mouseDownKeyIndex() const {
        return m_mouseDownKeyIndex;
    }

    [[nodiscard]] const QPointF &mouseDownPos() const {
        return m_mouseDownPos;
    }

    // Delta state
    [[nodiscard]] int deltaTick() const {
        return m_deltaTick;
    }

    [[nodiscard]] int deltaKey() const {
        return m_deltaKey;
    }

    void setDeltaTick(int tick) {
        m_deltaTick = tick;
    }

    void setDeltaKey(int key) {
        m_deltaKey = key;
    }

    // Move constraints
    [[nodiscard]] int moveMaxDeltaKey() const {
        return m_moveMaxDeltaKey;
    }

    [[nodiscard]] int moveMinDeltaKey() const {
        return m_moveMinDeltaKey;
    }

    void setMoveDeltaKeyRange(int max, int min);
    void resetMoveDeltaKeyRange();

    // Interaction preparation
    void prepareForEditingNotes(const QMouseEvent *event, QPointF scenePos, int keyIndex,
                                NoteView *noteItem);
    void finalizeClickSelection() const;

    // Action handlers
    void handleNotesMoved(int deltaTick, int deltaKey) const;
    static void handleNoteLeftResized(int noteId, int deltaTick);
    static void handleNoteRightResized(int noteId, int deltaTick);

    // Note manipulation
    void moveSelectedNotes(int startOffset, int keyOffset) const;
    void resetSelectedNotesOffset() const;
    void resizeLeftSelectedNote(int offset) const;
    void resizeRightSelectedNote(int offset) const;

    // Reset all interaction state
    void reset();

signals:
    void interactionStarted();
    void interactionEnded();

private:
    void updateMoveDeltaKeyRange();

    bool m_mouseDown = false;
    Qt::MouseButton m_mouseDownButton = Qt::NoButton;
    bool m_isEditPitchMode = false;

    bool m_tempQuantizeOff = false;
    QPointF m_mouseDownPos;
    int m_mouseDownRStart = 0;
    int m_mouseDownLength = 0;
    int m_mouseDownKeyIndex = 0;
    int m_deltaTick = 0;
    int m_deltaKey = 0;
    bool m_movedBeforeMouseUp = false;
    bool m_preserveSelectionOnClickRelease = false;
    int m_moveMaxDeltaKey = 127;
    int m_moveMinDeltaKey = 0;

    NoteView *m_currentEditingNote = nullptr;
    MouseMoveBehavior m_mouseMoveBehavior = None;

    PianoRollSelectionModel *m_selectionModel = nullptr;
    PianoRollGraphicsView *m_view = nullptr;
};

#endif // NOTEINTERACTIONCONTROLLER_H
