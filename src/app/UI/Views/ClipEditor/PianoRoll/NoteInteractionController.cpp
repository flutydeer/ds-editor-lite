#include "NoteInteractionController.h"
#include "PianoRollSelectionModel.h"
#include "PianoRollGraphicsView.h"
#include "NoteView.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Controller/ClipController.h"
#include "Modules/Inference/EditSessionManager.h"
#include "Utils/TimelineSnapUtils.h"
#include "Global/AppGlobal.h"

#include <QDebug>
#include <QMouseEvent>

NoteInteractionController::NoteInteractionController(PianoRollSelectionModel *selectionModel,
                                                     PianoRollGraphicsView *view,
                                                     QObject *parent)
    : QObject(parent)
    , m_selectionModel(selectionModel)
    , m_view(view) {
}

void NoteInteractionController::setMouseDown(bool down, Qt::MouseButton button) {
    m_mouseDown = down;
    m_mouseDownButton = button;
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
                                                       const QPointF scenePos,
                                                       const int keyIndex, NoteView *noteItem) {
    const auto resizeTolerance = AppGlobal::resizeTolerance;

    // If note is editing lyric, don't allow moving or resizing
    if (noteItem->isEditingLyric()) {
        m_mouseMoveBehavior = None;
        return;
    }

    const bool ctrlDown = event->modifiers() == Qt::ControlModifier;
    if (!ctrlDown) {
        if (m_selectionModel->selectedNoteItems().count() <= 1
            || !m_selectionModel->selectedNoteItems().contains(noteItem))
            m_view->clearNoteSelections();
        noteItem->setSelected(true);
    } else {
        noteItem->setSelected(!noteItem->isSelected());
    }

    const auto rPos = noteItem->mapFromScene(scenePos);
    const auto rx = rPos.x();
    if (rx >= 0 && rx <= resizeTolerance) {
        m_mouseMoveBehavior = ResizeLeft;
        noteItem->setSelected(true);
    } else if (rx >= noteItem->rect().width() - resizeTolerance
               && rx <= noteItem->rect().width()) {
        m_mouseMoveBehavior = ResizeRight;
        noteItem->setSelected(true);
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

void NoteInteractionController::handleNotesMoved(const int deltaTick,
                                                 const int deltaKey) const {
    qDebug() << "Notes moved dt:" << deltaTick << "dk:" << deltaKey;
    QList<int> noteIds;
    for (const auto note : m_selectionModel->selectedNoteItems())
        noteIds.append(note->id());
    clipController->onMoveNotes(noteIds, deltaTick, deltaKey);
}

void NoteInteractionController::handleNoteLeftResized(const int noteId, const int deltaTick) {
    qDebug() << "Note left resized id:" << noteId << "dt:" << deltaTick;
    QList<int> notes;
    notes.append(noteId);
    clipController->onResizeNotesLeft(notes, deltaTick);
}

void NoteInteractionController::handleNoteRightResized(const int noteId, const int deltaTick) {
    qDebug() << "Note right resized id:" << noteId << "dt:" << deltaTick;
    QList<int> notes;
    notes.append(noteId);
    clipController->onResizeNotesRight(notes, deltaTick);
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
}

void NoteInteractionController::resizeRightSelectedNote(const int offset) const {
    m_currentEditingNote->setLengthOffset(offset);
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
}