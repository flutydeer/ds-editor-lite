#include "NoteInteractionController.h"
#include "NoteAdjacencyUtils.h"
#include "NoteHandleGeometry.h"
#include "PianoRollSelectionModel.h"
#include "PianoRollGraphicsView.h"
#include "NoteView.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/EditorPointerUtils.h"
#include "UI/Views/Common/EditorResizeUtils.h"
#include "Model/AppStatus/AppStatus.h"
#include "Controller/ClipController.h"
#include "Modules/Inference/EditSessionManager.h"
#include <lite/MusicBase/TimelineSnapUtils.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include "Global/AppGlobal.h"

#include <QDebug>
#include <QMouseEvent>

NoteInteractionController::NoteInteractionController(PianoRollSelectionModel *selectionModel,
                                                     PianoRollGraphicsView *view, QObject *parent)
    : QObject(parent), m_selectionModel(selectionModel), m_view(view) {
}

void NoteInteractionController::setMouseDown(bool down, Qt::MouseButton button) {
    m_mouseDown = down;
    m_mouseDownButton = button;
}

void NoteInteractionController::setDataContext(SingingClip *clip) {
    m_clip = clip;
}

NoteView *NoteInteractionController::jointNeighborView() const {
    return m_jointNeighborId < 0 ? nullptr : m_view->findNoteViewById(m_jointNeighborId);
}

void NoteInteractionController::clearJointResize() {
    m_jointNeighborId = -1;
}

void NoteInteractionController::setMouseDownNoteParams(int rStart, int length, int keyIndex) {
    m_mouseDownRStart = rStart;
    m_mouseDownLength = length;
    m_mouseDownKeyIndex = keyIndex;
}

void NoteInteractionController::setMoveDeltaKeyRange(int max, int min) {
    m_moveMaxDeltaKey = max;
    m_moveMinDeltaKey = min;
}

void NoteInteractionController::resetMoveDeltaKeyRange() {
    m_moveMaxDeltaKey = 127;
    m_moveMinDeltaKey = 0;
}

void NoteInteractionController::prepareForEditingNotes(const QMouseEvent *event,
                                                       const QPointF scenePos, const int keyIndex,
                                                       NoteView *noteItem) {
    // If note is editing lyric, don't allow moving or resizing
    if (noteItem->isEditingLyric()) {
        m_mouseMoveBehavior = None;
        return;
    }

    clearJointResize();

    const auto rPos = noteItem->mapFromScene(scenePos);
    // Provisional edge for the Shift decision: the joint branch may change the
    // selection, and the behavior edge below is resolved against the settled
    // handle-frame state like any plain press
    const auto provisionalEdge = NoteHandleGeometry::resizeEdgeAt(
        rPos, noteItem->rect(), EditorPointer::resizeTolerance(),
        noteItem->id() == m_handleFramedNoteId);

    // Shift on a resize edge asks for the joint boundary drag: the timeline
    // neighbor on that side has to move with the boundary. A gapped or overlapped
    // neighbor cannot, so the press is refused entirely instead of resizing one
    // note apart from the other. Without a neighbor the plain path applies.
    auto selectionModifiers = event->modifiers();
    if (m_clip && selectionModifiers.testFlag(Qt::ShiftModifier) &&
        provisionalEdge != EditorResizeUtils::HorizontalEdge::None) {
        const auto neighbor = NoteAdjacencyUtils::neighborForEdge(
            m_clip, m_clip->findNoteById(noteItem->id()), provisionalEdge);
        if (neighbor.neighbor) {
            if (!neighbor.exactlyAdjacent) {
                m_mouseMoveBehavior = None;
                m_currentEditingNote = nullptr;
                return;
            }
            // Plain single-note press: the joint drag takes over the edge, so the
            // Shift range-select semantics do not apply here
            m_jointNeighborId = neighbor.neighbor->id();
            selectionModifiers = Qt::NoModifier;
        }
    }

    (void) m_selectionModel->applyNoteSelection(noteItem, selectionModifiers);

    if (!noteItem->isSelected()) {
        m_mouseMoveBehavior = None;
        m_currentEditingNote = nullptr;
        return;
    }

    const auto edge = NoteHandleGeometry::resizeEdgeAt(rPos, noteItem->rect(),
                                                       EditorPointer::resizeTolerance(),
                                                       noteItem->id() == m_handleFramedNoteId);
    if (edge == EditorResizeUtils::HorizontalEdge::Left) {
        m_mouseMoveBehavior = ResizeLeft;
    } else if (edge == EditorResizeUtils::HorizontalEdge::Right) {
        m_mouseMoveBehavior = ResizeRight;
    } else {
        m_mouseMoveBehavior = Move;
    }

    m_currentEditingNote = noteItem;
    m_mouseDownPos = scenePos;
    m_mouseDownRStart = m_currentEditingNote->rStart();
    m_mouseDownLength = m_currentEditingNote->length();
    m_mouseDownKeyIndex = keyIndex;
    updateMoveDeltaKeyRange();
}

void NoteInteractionController::finalizeClickSelection() const {
    m_selectionModel->finalizePressSelection(m_movedBeforeMouseUp);
}

void NoteInteractionController::handleNotesMoved(const int deltaTick, const int deltaKey) const {
    qDebug() << "Notes moved dt:" << deltaTick << "dk:" << deltaKey;
    QList<int> noteIds;
    for (const auto note : m_selectionModel->selectedNoteItems())
        noteIds.append(note->id());
    clipController->onMoveNotes(noteIds, deltaTick, deltaKey);
}

void NoteInteractionController::handleNoteLeftResized(const int noteId, const int deltaTick,
                                                      const int minimumLength) {
    qDebug() << "Note left resized id:" << noteId << "dt:" << deltaTick;
    QList<int> notes;
    notes.append(noteId);
    clipController->onResizeNotesLeft(notes, deltaTick, minimumLength);
}

void NoteInteractionController::handleNoteRightResized(const int noteId, const int deltaTick,
                                                       const int minimumLength) {
    qDebug() << "Note right resized id:" << noteId << "dt:" << deltaTick;
    QList<int> notes;
    notes.append(noteId);
    clipController->onResizeNotesRight(notes, deltaTick, minimumLength);
}

void NoteInteractionController::handleNoteSharedBoundaryResized(const int leftNoteId,
                                                                const int rightNoteId,
                                                                const int deltaTick,
                                                                const int minimumLength) {
    qDebug() << "Note shared boundary resized" << leftNoteId << rightNoteId << "dt:" << deltaTick;
    clipController->onResizeNotesSharedBoundary(leftNoteId, rightNoteId, deltaTick, minimumLength);
}

void NoteInteractionController::moveSelectedNotes(const int startOffset,
                                                  const int keyOffset) const {
    for (const auto note : m_selectionModel->selectedNoteItems()) {
        note->setStartOffset(startOffset);
        note->setKeyOffset(keyOffset);
    }
}

void NoteInteractionController::resetSelectedNotesOffset() const {
    for (const auto note : m_selectionModel->selectedNoteItems())
        note->resetOffset();
}

void NoteInteractionController::resizeLeftSelectedNote(const int offset) const {
    // TODO: resize all selected notes
    m_currentEditingNote->setStartOffset(offset);
    m_currentEditingNote->setLengthOffset(-offset);
    // Joint drag: the left neighbor lengthens by the same boundary shift
    if (auto *neighbor = jointNeighborView())
        neighbor->setLengthOffset(offset);
}

void NoteInteractionController::resizeRightSelectedNote(const int offset) const {
    m_currentEditingNote->setLengthOffset(offset);
    // Joint drag: the right neighbor shifts its start and gives up the length
    if (auto *neighbor = jointNeighborView()) {
        neighbor->setStartOffset(offset);
        neighbor->setLengthOffset(-offset);
    }
}

void NoteInteractionController::updateMoveDeltaKeyRange() {
    auto selectedNotes = m_selectionModel->selectedNoteItems();
    int highestKey = 0;
    int lowestKey = 127;
    for (const auto note : selectedNotes) {
        const auto key = note->keyIndex();
        if (key > highestKey)
            highestKey = key;
        if (key < lowestKey)
            lowestKey = key;
    }
    m_moveMaxDeltaKey = 127 - highestKey;
    m_moveMinDeltaKey = -lowestKey;
}

void NoteInteractionController::reset() {
    m_mouseDown = false;
    m_mouseDownButton = Qt::NoButton;
    m_tempQuantizeOff = false;
    m_mouseDownPos = {};
    m_mouseDownRStart = 0;
    m_mouseDownLength = 0;
    m_mouseDownKeyIndex = 0;
    m_deltaTick = 0;
    m_deltaKey = 0;
    m_movedBeforeMouseUp = false;
    m_moveMaxDeltaKey = 127;
    m_moveMinDeltaKey = 0;
    m_currentEditingNote = nullptr;
    m_mouseMoveBehavior = None;
    m_jointNeighborId = -1;
}
