//
// Created by fluty on 2024/1/23.
//

#include "PianoRollGraphicsView.h"

#include "ClipRangeOverlay.h"
#include "NoteView.h"
#include "PianoRollBackground.h"
#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsViewHelper.h"
#include "PianoRollCoord.h"
#include "PianoRollContextMenuBuilder.h"
#include "NoteInteractionController.h"
#include "PianoRollSelectionModel.h"
#include "PianoRollGraphicsView_p.h"
#include "PitchAnchorEditorView.h"
#include "PitchEditorView.h"
#include "PronunciationView.h"
#include "PianoRollEditHandler.h"

#include "DrawNoteHandler.h"
#include "EditPitchAnchorHandler.h"
#include "EraseNoteHandler.h"

#include "SelectNoteHandler.h"
#include "SplitNoteHandler.h"
#include "Controller/ClipController.h"
#include "Controller/ClipboardController.h"
#include "Controller/PlaybackController.h"
#include "Controller/Actions/AppModel/Note/NoteActions.h"
#include "Global/AppGlobal.h"
#include "Global/ControllerGlobal.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/DrawCurve.h"
#include "Model/AppModel/Note.h"
#include "Model/ClipboardDataModel/NotesParamsInfo.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Dialogs/Note/NotePropertyDialog.h"
#include "Model/NoteDialog/NoteDialogResult.h"
#include "UI/Views/Common/ScrollBarView.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"
#include "Utils/TimelineSnapUtils.h"
#include <climits>
#include <cmath>

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QGraphicsLineItem>
#include <QMimeData>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QScrollBar>
#include "UI/Controls/Menu.h"

namespace Helper = PianoRollGraphicsViewHelper;

namespace {
    void logMissingNoteView(const char *context, const int noteId) {
        qWarning() << "Ignore note update because note view is missing"
                   << "context:" << context << "noteId:" << noteId;
    }
}

PianoRollGraphicsView::PianoRollGraphicsView(PianoRollGraphicsScene *scene, const QWidget *parent)
    : TimeGraphicsView(scene, parent), d_ptr(new PianoRollGraphicsViewPrivate(this)) {
    Q_D(PianoRollGraphicsView);
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("PianoRollGraphicsView");
    setScaleXMax(5);
    setPixelsPerQuarterNote(pixelsPerQuarterNote);
    setSceneVisibility(false);
    setDragBehavior(DragBehavior::RectSelect);
    setMinimumHeight(0);

    d->m_selectionModel =
        new PianoRollSelectionModel(this, d->noteViews, d->noteViewIndex, d->m_notes, this);
    d->m_interactionController = new NoteInteractionController(d->m_selectionModel, this, this);

    d->m_gridItem = new PianoRollBackground;
    d->m_gridItem->setPixelsPerQuarterNote(pixelsPerQuarterNote);
    d->m_gridItem->setQuantize(appStatus->pianoRollQuantize);
    setGridItem(d->m_gridItem);
    connect(appStatus, &AppStatus::pianoRollQuantizeChanged, d->m_gridItem,
            &PianoRollBackground::setQuantize);

    d->m_pitchEditor = new PitchEditorView;
    d->m_pitchEditor->setZValue(2);
    connect(d->m_pitchEditor, &CommonParamEditorView::editCompleted, this,
            [](const QList<DrawCurve *> &curves) { Helper::editPitch(curves); });
    connect(d->m_pitchEditor, &CommonParamEditorView::editStarted, this, [d] {
        if (!d->m_clip || d->m_pitchEditSessionActive)
            return;
        d->m_pitchEditSessionId = editSessionManager->beginTransaction(
            AppStatus::EditObjectType::Param, d->m_clip->id(), {}, {}, {}, {ParamInfo::Pitch});
        d->m_pitchEditSessionActive = true;
        appStatus->currentEditObject = AppStatus::EditObjectType::Param;
    });
    connect(d->m_pitchEditor, &CommonParamEditorView::editCommitted, this,
            [d] { d->endPitchEditSession(EditSessionEndReason::Commit); });
    connect(d->m_pitchEditor, &CommonParamEditorView::editDiscarded, this,
            [d] { d->endPitchEditSession(EditSessionEndReason::Discard); });
    scene->addCommonItem(d->m_pitchEditor);
    d->m_pitchEditor->setTransparentMouseEvents(true);

    d->m_anchorEditor = new PitchAnchorEditorView;
    d->m_anchorEditor->setZValue(2.5);
    scene->addCommonItem(d->m_anchorEditor);
    d->m_anchorEditor->setTransparentMouseEvents(true);

    d->m_clipRangeOverlay = new ClipRangeOverlay;
    d->m_clipRangeOverlay->setZValue(3);
    scene->addCommonItem(d->m_clipRangeOverlay);

    auto *splitHandler = new SplitNoteHandler;
    splitHandler->setContext(this, d);
    d->m_handlers.insert(SplitNote, splitHandler);

    auto *eraseHandler = new EraseNoteHandler;
    eraseHandler->setContext(this, d);
    d->m_handlers.insert(EraseNote, eraseHandler);

    auto *drawHandler = new DrawNoteHandler;
    drawHandler->setContext(this, d);
    d->m_handlers.insert(DrawNote, drawHandler);

    auto *selectHandler = new SelectNoteHandler;
    selectHandler->setContext(this, d);
    d->m_handlers.insert(Select, selectHandler);

    auto *intervalSelectHandler = new SelectNoteHandler;
    intervalSelectHandler->setContext(this, d);
    d->m_handlers.insert(IntervalSelect, intervalSelectHandler);

    auto *editPitchAnchorHandler = new EditPitchAnchorHandler;
    editPitchAnchorHandler->setContext(this, d);
    d->m_handlers.insert(EditPitchAnchor, editPitchAnchorHandler);
    d->m_anchorEditor->setOverlayState(&editPitchAnchorHandler->overlayState());
    editPitchAnchorHandler->setAlwaysVisible(true);

    connect(scene, &QGraphicsScene::selectionChanged, this,
            &PianoRollGraphicsView::onSceneSelectionChanged);

    connect(this, &TimeGraphicsView::scaleChanged, this,
            &PianoRollGraphicsView::notifyKeyRangeChanged);
    connect(this, &TimeGraphicsView::visibleRectChanged, this,
            &PianoRollGraphicsView::notifyKeyRangeChanged);

    connect(appStatus, &AppStatus::noteSelectionChanged, d,
            &PianoRollGraphicsViewPrivate::onNoteSelectionChanged);
}

PianoRollGraphicsView::~PianoRollGraphicsView() {
    Q_D(PianoRollGraphicsView);
    delete d->m_pitchEditor;
    qDeleteAll(d->m_handlers);
    delete d_ptr;
}

void PianoRollGraphicsView::setDataContext(SingingClip *clip) {
    Q_D(PianoRollGraphicsView);
    if (clip == nullptr)
        d->moveToNullClipState();
    else
        d->moveToSingingClipState(clip);
}

void PianoRollGraphicsViewPrivate::endPitchEditSession(const EditSessionEndReason reason) {
    if (!m_pitchEditSessionActive)
        return;

    const auto sessionId = m_pitchEditSessionId;
    m_pitchEditSessionActive = false;
    m_pitchEditSessionId = 0;

    if (sessionId != 0 && editSessionManager->hasActiveTransaction() &&
        editSessionManager->activeSession().sessionId == sessionId) {
        editSessionManager->endTransaction(sessionId, reason);
    }
    if (!editSessionManager->hasActiveTransaction())
        appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void PianoRollGraphicsView::onSceneSelectionChanged() const {
    Q_D(const PianoRollGraphicsView);
    if (!d->m_selectionModel->selectionChangeBarrier()) {
        const auto notes = selectedNotesId();
        clipController->selectNotes(notes, true);
    }
}

void PianoRollGraphicsView::notifyKeyRangeChanged() {
    emit keyRangeChanged(topKeyIndex(), bottomKeyIndex());
}

bool PianoRollGraphicsViewPrivate::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Leave)
        m_selectionModel->clearPastePreviewViews();
    return QObject::eventFilter(watched, event);
}

bool PianoRollGraphicsView::event(QEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
        const auto key = dynamic_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Escape) {
            discardAction();
        }
    } else if (event->type() == QEvent::WindowDeactivate) {
        discardAction();
    } else if (event->type() == QEvent::HoverEnter)
        d->onHoverEnter(dynamic_cast<QHoverEvent *>(event));
    else if (event->type() == QEvent::HoverLeave)
        d->onHoverLeave(dynamic_cast<QHoverEvent *>(event));
    else if (event->type() == QEvent::HoverMove)
        d->onHoverMove(dynamic_cast<QHoverEvent *>(event));
    return TimeGraphicsView::event(event);
}

void PianoRollGraphicsView::contextMenuEvent(QContextMenuEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (d->m_editMode == Select || d->m_editMode == IntervalSelect || d->m_editMode == DrawNote ||
        d->m_editMode == EraseNote || d->m_editMode == SplitNote) {
        if (const auto noteView = d->noteViewAt(event->pos())) {
            const auto menu = PianoRollContextMenuBuilder::buildNoteContextMenu(
                this, noteView, [d] { d->onDeleteSelectedNotes(); },
                [d](int noteId) { d->onOpenNotePropertyDialog(noteId, AppGlobal::Lyric); });
            menu->exec(event->globalPos());
        } else {
            const auto menu = PianoRollContextMenuBuilder::buildBackgroundContextMenu(
                this, d->m_selectionModel, event->pos(), d->m_offset);
            menu->exec(event->globalPos());
        }
        return;
    } else if (d->m_editMode == EditPitchAnchor) {
        if (d->m_currentHandler)
            d->m_currentHandler->contextMenuEvent(event);
        return;
    }

    TimeGraphicsView::contextMenuEvent(event);
}

void PianoRollGraphicsView::mousePressEvent(QMouseEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (d->m_interactionController->isMouseDown()) {
        qWarning() << "Ignored mousePressEvent" << event
                   << "because there is already one mouse button pressed";
        return;
    }
    d->m_interactionController->setMouseDown(true, event->button());

    // When pressing on scrollbar, delegate to base class
    if (dynamic_cast<ScrollBarView *>(itemAt(event->pos()))) {
        TimeGraphicsView::mousePressEvent(event);
        event->ignore();
        return;
    }

    cancelRequested = false;
    d->m_selectionModel->setSelecting(true);
    d->m_selectionModel->setSelectionChangeBarrier(true);
    if (event->button() != Qt::LeftButton &&
        (d->m_editMode == Select || d->m_editMode == IntervalSelect || d->m_editMode == DrawNote ||
         d->m_editMode == EraseNote || d->m_editMode == SplitNote)) {
        d->m_interactionController->setMouseMoveBehavior(NoteInteractionController::None);
        if (const auto noteView = d->noteViewAt(event->pos())) {
            if (d->m_selectionModel->selectedNoteItems().count() <= 1 ||
                !d->m_selectionModel->selectedNoteItems().contains(noteView))
                clearNoteSelections();
            noteView->setSelected(true);
        } else {
            clearNoteSelections();
            TimeGraphicsView::mousePressEvent(event);
        }
        event->ignore();
        return;
    }

    const auto scenePos = mapToScene(event->position().toPoint());
    const auto tick = static_cast<int>(sceneXToTick(scenePos.x()) + d->m_offset);
    const auto keyIndex = PianoRollCoord::sceneYToKeyIndexInt(scenePos.y(), scaleY() * noteHeight);
    const auto noteView = d->noteViewAt(event->pos());
    const auto pronView = d->pronViewAt(event->pos());

    if (d->m_editMode == Select) {
        if (d->m_currentHandler)
            d->m_currentHandler->mousePressEvent(event);
        if (!noteView)
            TimeGraphicsView::mousePressEvent(event);
    } else if (d->m_editMode == DrawNote) {
        if (d->m_currentHandler)
            d->m_currentHandler->mousePressEvent(event);
        else
            TimeGraphicsView::mousePressEvent(event);
    } else if (d->m_editMode == EraseNote) {
        if (d->m_currentHandler)
            d->m_currentHandler->mousePressEvent(event);
        else
            TimeGraphicsView::mousePressEvent(event);
    } else if (d->m_editMode == SplitNote) {
        if (d->m_currentHandler)
            d->m_currentHandler->mousePressEvent(event);
        else
            TimeGraphicsView::mousePressEvent(event);
    } else if (d->m_editMode == IntervalSelect) {
        if (d->m_currentHandler)
            d->m_currentHandler->mousePressEvent(event);
        if (!noteView)
            TimeGraphicsView::mousePressEvent(event);
    } else if (d->m_editMode == EditPitchAnchor) {
        if (d->m_currentHandler)
            d->m_currentHandler->mousePressEvent(event);
    } else
        TimeGraphicsView::mousePressEvent(event);
    event->ignore();
}

void PianoRollGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    Q_D(PianoRollGraphicsView);

    // Check if any note is being edited, if so, don't handle mouse move events to avoid affecting
    // focus
    bool hasEditingNote = false;
    for (const auto view : d->noteViews) {
        if (view->isEditingLyric()) {
            hasEditingNote = true;
            break;
        }
        if (view->pronunciationView() && view->pronunciationView()->isEditingPronunciation()) {
            hasEditingNote = true;
            break;
        }
    }
    if (hasEditingNote && !d->m_interactionController->isMouseDown()) {
        // If a note is being edited and mouse is not down, return directly without handling mouse
        // move events
        return;
    }

    // Update hover key index for piano keyboard when mouse is over the grid
    if (!d->m_mouseDown) {
        const auto scenePos = mapToScene(event->position().toPoint());
        const auto keyIndex =
            PianoRollCoord::sceneYToKeyIndexInt(scenePos.y(), scaleY() * noteHeight);
        // Check if mouse is within the view bounds
        const auto viewRect = rect();
        if (viewRect.contains(event->pos())) {
            emit keyHovered(keyIndex);
        }
    }

    if (d->m_currentHandler) {
        if (d->m_currentHandler->mouseMoveEvent(event))
            return;
    }
    if (d->m_interactionController->mouseMoveBehavior() == NoteInteractionController::None) {
        TimeGraphicsView::mouseMoveEvent(event);
        return;
    }
    if (cancelRequested || d->m_interactionController->isMouseDown() == false)
        return;

    if (event->modifiers() == Qt::AltModifier)
        d->m_interactionController->setTempQuantizeOff(true);
    else
        d->m_interactionController->setTempQuantizeOff(false);

    const auto scenePos = mapToScene(event->position().toPoint());
    const auto tick = static_cast<int>(sceneXToTick(scenePos.x()) + d->m_offset);
    const auto quantizedTickLength = TimelineSnapUtils::quantizeStep(
        appStatus->pianoRollQuantize, d->m_interactionController->tempQuantizeOff());
    const auto snappedTick = TimelineSnapUtils::snapDown(tick, quantizedTickLength);
    const auto snappedTickNearest = TimelineSnapUtils::snapNearest(tick, quantizedTickLength);
    const auto keyIndex = PianoRollCoord::sceneYToKeyIndexInt(scenePos.y(), scaleY() * noteHeight);
    const auto deltaX = static_cast<int>(
        sceneXToTick(scenePos.x() - d->m_interactionController->mouseDownPos().x()));

    const auto noteView = d->noteViewAt(event->pos());

    // TODO: Optimize note moving and resizing
    if (d->m_interactionController->mouseMoveBehavior() == NoteInteractionController::Move) {
        const auto startOffset = TimelineSnapUtils::snapNearest(deltaX, quantizedTickLength);
        auto keyOffset = keyIndex - d->m_interactionController->mouseDownKeyIndex();
        if (keyOffset > d->m_interactionController->moveMaxDeltaKey())
            keyOffset = d->m_interactionController->moveMaxDeltaKey();
        if (keyOffset < d->m_interactionController->moveMinDeltaKey())
            keyOffset = d->m_interactionController->moveMinDeltaKey();
        d->m_interactionController->setDeltaTick(startOffset);
        d->m_interactionController->setDeltaKey(keyOffset);
        d->m_interactionController->moveSelectedNotes(d->m_interactionController->deltaTick(),
                                                      d->m_interactionController->deltaKey());
        d->m_interactionController->setMovedBeforeMouseUp(true);
    } else if (d->m_interactionController->mouseMoveBehavior() ==
               NoteInteractionController::ResizeLeft) {
        const auto rStart = snappedTick - d->m_offset;
        auto deltaStart = rStart - d->m_interactionController->mouseDownRStart();
        const auto length = d->m_interactionController->mouseDownLength() - deltaStart;
        if (length < quantizedTickLength)
            deltaStart = d->m_interactionController->mouseDownLength() - quantizedTickLength;
        d->m_interactionController->setDeltaTick(deltaStart);
        d->m_interactionController->resizeLeftSelectedNote(d->m_interactionController->deltaTick());
        d->m_interactionController->setMovedBeforeMouseUp(true);
    } else if (d->m_interactionController->mouseMoveBehavior() ==
               NoteInteractionController::ResizeRight) {
        const auto right = snappedTickNearest - d->m_offset;
        const auto length = right - d->m_interactionController->mouseDownRStart();
        auto deltaLength = length - d->m_interactionController->mouseDownLength();
        if (length < quantizedTickLength)
            deltaLength = -(d->m_interactionController->mouseDownLength() - quantizedTickLength);
        d->m_interactionController->setDeltaTick(deltaLength);
        d->m_interactionController->resizeRightSelectedNote(
            d->m_interactionController->deltaTick());
        d->m_interactionController->setMovedBeforeMouseUp(true);
    } else
        TimeGraphicsView::mouseMoveEvent(event);
}

void PianoRollGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (event->button() != d->m_interactionController->mouseDownButton()) {
        qWarning() << "Ignored mouseReleaseEvent" << event;
        return;
    }
    d->m_interactionController->setMouseDown(false);
    if (d->m_currentHandler) {
        if (!cancelRequested) {
            if (d->m_currentHandler->mouseReleaseEvent(event)) {
                cancelRequested = false;
                TimeGraphicsView::mouseReleaseEvent(event);
                return;
            }
        }
        cancelRequested = false;
    }
    if (!cancelRequested)
        commitAction();
    const bool ctrlDown = event->modifiers() == Qt::ControlModifier;
    if (d->m_interactionController->mouseMoveBehavior() == NoteInteractionController::Move) {
        if (!d->m_interactionController->movedBeforeMouseUp() && !ctrlDown) {
            clearNoteSelections(d->m_interactionController->currentEditingNote());
        }
    }
    cancelRequested = false;
    TimeGraphicsView::mouseReleaseEvent(event);
}

void PianoRollGraphicsView::mouseDoubleClickEvent(QMouseEvent *event) {
    // Disable double-click event to prevent deselecting notes when double-clicking on scrollbar
    Q_D(PianoRollGraphicsView);
    if (!(d->m_editMode == Select || d->m_editMode == IntervalSelect || d->m_editMode == DrawNote ||
          d->m_editMode == EditPitchAnchor))
        return;
    if (event->button() != Qt::LeftButton)
        return;

    if (d->m_editMode == EditPitchAnchor) {
        if (d->m_currentHandler)
            d->m_currentHandler->mouseDoubleClickEvent(event);
        return;
    }

    // Check if double-clicked on a note or pronunciation view
    bool handled = false;
    for (const auto item : items(event->pos())) {
        if (const auto noteView = dynamic_cast<NoteView *>(item)) {
            d->onStartEditingNoteLyric(noteView);
            handled = true;
            break;
        }
        if (const auto pronView = dynamic_cast<PronunciationView *>(item)) {
            d->onStartEditingPronunciation(pronView);
            handled = true;
            break;
        }
    }

    // If double-clicked on empty space in Select mode, create a note with current quantize length
    if (!handled && d->m_editMode == Select) {
        const auto scenePos = mapToScene(event->position().toPoint());
        const auto tick = static_cast<int>(sceneXToTick(scenePos.x()) + d->m_offset);
        const auto keyIndex =
            PianoRollCoord::sceneYToKeyIndexInt(scenePos.y(), scaleY() * noteHeight);

        const int noteLength = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);

        d->m_interactionController->setMouseDown(true, Qt::LeftButton);
        d->m_interactionController->setMouseDownPos(scenePos);
        d->m_interactionController->setMouseDownNoteParams(0, 0, keyIndex);

        if (auto *drawHandler =
                dynamic_cast<DrawNoteHandler *>(d->m_handlers.value(DrawNote, nullptr))) {
            drawHandler->prepareForDrawingNote(tick, keyIndex, noteLength);
            d->m_currentHandler = drawHandler;
        }
    }
}

void PianoRollGraphicsView::keyPressEvent(QKeyEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (d->m_currentHandler && d->m_currentHandler->keyPressEvent(event))
        return;
    TimeGraphicsView::keyPressEvent(event);
}

int PianoRollGraphicsView::noteFontPixelSize() const {
    return m_noteFontPixelSize;
}

void PianoRollGraphicsView::setNoteFontPixelSize(const int size) {
    Q_D(PianoRollGraphicsView);
    m_noteFontPixelSize = size;
    for (const auto noteView : d->noteViews)
        noteView->fontPixelSize = size;
}

QColor PianoRollGraphicsView::whiteKeyColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_gridItem->whiteKeyColor();
}

void PianoRollGraphicsView::setWhiteKeyColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_gridItem->setWhiteKeyColor(color);
}

QColor PianoRollGraphicsView::blackKeyColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_gridItem->blackKeyColor();
}

void PianoRollGraphicsView::setBlackKeyColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_gridItem->setBlackKeyColor(color);
}

QColor PianoRollGraphicsView::octaveDividerColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_gridItem->octaveDividerColor();
}

void PianoRollGraphicsView::setOctaveDividerColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_gridItem->setOctaveDividerColor(color);
}

void PianoRollGraphicsView::reset() {
    Q_D(PianoRollGraphicsView);
    for (const auto &noteView : d->noteViews) {
        d->removeNoteViewFromScene(noteView);
        delete noteView;
    }
}

QList<int> PianoRollGraphicsView::selectedNotesId() const {
    Q_D(const PianoRollGraphicsView);
    QList<int> list;
    for (const auto noteView : d->noteViews) {
        if (noteView->isSelected())
            list.append(noteView->id());
    }
    return list;
}

void PianoRollGraphicsView::clearNoteSelections(const NoteView *except) {
    Q_D(PianoRollGraphicsView);
    for (const auto noteView : d->noteViews) {
        if (noteView != except && noteView->isSelected())
            noteView->setSelected(false);
    }
}

void PianoRollGraphicsView::discardAction() {
    Q_D(PianoRollGraphicsView);
    d->m_pitchEditor->discardAction();
    cancelRequested = true;
    if (d->m_currentHandler) {
        d->m_currentHandler->discard();
    }
    if (d->m_interactionController->mouseMoveBehavior() == NoteInteractionController::Move) {
        if (d->m_interactionController->movedBeforeMouseUp()) {
            d->m_interactionController->resetSelectedNotesOffset();
        }
    } else if (d->m_interactionController->mouseMoveBehavior() ==
               NoteInteractionController::ResizeLeft) {
        d->m_interactionController->resetSelectedNotesOffset();
    } else if (d->m_interactionController->mouseMoveBehavior() ==
               NoteInteractionController::ResizeRight) {
        d->m_interactionController->resetSelectedNotesOffset();
    }
    d->m_interactionController->setMouseMoveBehavior(NoteInteractionController::None);
    d->m_interactionController->setDeltaTick(0);
    d->m_interactionController->setDeltaKey(0);
    d->m_interactionController->setMovedBeforeMouseUp(false);
    d->m_interactionController->setCurrentEditingNote(nullptr);

    d->m_selectionModel->setSelecting(false);
    const auto notes = selectedNotesId();
    clipController->selectNotes(notes, true);

    editSessionManager->endActiveTransaction(EditSessionEndReason::Discard);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void PianoRollGraphicsView::commitAction() {
    Q_D(PianoRollGraphicsView);
    d->m_pitchEditor->commitAction();
    if (d->m_interactionController->mouseMoveBehavior() == NoteInteractionController::Move) {
        if (d->m_interactionController->movedBeforeMouseUp()) {
            d->m_interactionController->resetSelectedNotesOffset();
            d->m_interactionController->handleNotesMoved(d->m_interactionController->deltaTick(),
                                                         d->m_interactionController->deltaKey());
        }
    } else if (d->m_interactionController->mouseMoveBehavior() ==
               NoteInteractionController::ResizeLeft) {
        if (d->m_interactionController->movedBeforeMouseUp() &&
            d->m_interactionController->currentEditingNote()) {
            d->m_interactionController->resetSelectedNotesOffset();
            NoteInteractionController::handleNoteLeftResized(
                d->m_interactionController->currentEditingNote()->id(),
                d->m_interactionController->deltaTick());
        }
    } else if (d->m_interactionController->mouseMoveBehavior() ==
               NoteInteractionController::ResizeRight) {
        if (d->m_interactionController->movedBeforeMouseUp() &&
            d->m_interactionController->currentEditingNote()) {
            d->m_interactionController->resetSelectedNotesOffset();
            NoteInteractionController::handleNoteRightResized(
                d->m_interactionController->currentEditingNote()->id(),
                d->m_interactionController->deltaTick());
        }
    }
    d->m_interactionController->setMouseMoveBehavior(NoteInteractionController::None);
    d->m_interactionController->setDeltaTick(0);
    d->m_interactionController->setDeltaKey(0);
    d->m_interactionController->setMovedBeforeMouseUp(false);
    d->m_interactionController->setCurrentEditingNote(nullptr);

    d->m_selectionModel->setSelecting(false);
    const auto notes = selectedNotesId();
    clipController->selectNotes(notes, true);

    editSessionManager->endActiveTransaction(EditSessionEndReason::Commit);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

double PianoRollGraphicsView::topKeyIndex() const {
    return PianoRollCoord::sceneYToKeyIndexDouble(visibleRect().top(), scaleY() * noteHeight);
}

double PianoRollGraphicsView::bottomKeyIndex() const {
    return PianoRollCoord::sceneYToKeyIndexDouble(visibleRect().bottom(), scaleY() * noteHeight);
}

void PianoRollGraphicsView::setViewportCenterAt(const double tick, const double keyIndex) {
    setViewportCenterAtTick(tick);
    setViewportCenterAtKeyIndex(keyIndex);
}

void PianoRollGraphicsView::setViewportCenterAtKeyIndex(const double keyIndex) {
    const auto keyIndexRange = topKeyIndex() - bottomKeyIndex();
    const auto keyIndexStart = keyIndex + keyIndexRange / 2 + 0.5;
    const auto vBarValue =
        qRound(PianoRollCoord::keyIndexToSceneY(keyIndexStart, scaleY() * noteHeight));
    verticalBarAnimateTo(vBarValue);
}

void PianoRollGraphicsView::setEditMode(const PianoRollEditMode mode) {
    Q_D(PianoRollGraphicsView);
    if (d->m_editMode != mode)
        commitAction();

    if (d->m_currentHandler)
        d->m_currentHandler->deactivate();

    d->m_editMode = mode;
    d->m_currentHandler = d->m_handlers.value(mode, nullptr);

    if (d->m_currentHandler) {
        d->m_currentHandler->activate();
    }

    if (mode == Select) {
        setDragBehavior(DragBehavior::RectSelect);
        d->setPitchEditMode(false, false);
    } else if (mode == IntervalSelect) {
        setDragBehavior(DragBehavior::IntervalSelect);
        d->setPitchEditMode(false, false);
    } else if (mode == DrawNote || mode == EraseNote || mode == SplitNote) {
        setDragBehavior(DragBehavior::None);
        d->setPitchEditMode(false, false);
    } else if (mode == DrawPitch) {
        setDragBehavior(DragBehavior::None);
        d->setPitchEditMode(true, false);
    } else if (mode == EditPitchAnchor) {
        setDragBehavior(DragBehavior::None);
        d->setPitchEditMode(true, false);
        d->m_pitchEditor->setTransparentMouseEvents(true);
    } else if (mode == ErasePitch || mode == FreezePitch) { // TODO: Implement freeze auto pitch
        setDragBehavior(DragBehavior::None);
        d->setPitchEditMode(true, true);
    }
}

void PianoRollGraphicsViewPrivate::restoreHandler() {
    m_currentHandler = m_handlers.value(m_editMode, nullptr);
}

void PianoRollGraphicsViewPrivate::onNoteChanged(const SingingClip::NoteChangeType type,
                                                 const QList<Note *> &notes) {
    if (type == SingingClip::Insert)
        for (const auto &note : notes)
            handleNoteInserted(note);
    else if (type == SingingClip::Remove) {
        for (const auto &note : notes)
            handleNoteRemoved(note);
        QList<int> notesId;
        for (const auto &note : notes)
            notesId.append(note->id());
        clipController->unselectNotes(notesId);
    } else if (type == SingingClip::TimeKeyPropertyChange) {
        for (const auto &note : notes)
            updateNoteTimeAndKey(note);
    } else if (type == SingingClip::OriginalWordPropertyChange ||
               type == SingingClip::EditedWordPropertyChange) {
        for (const auto &note : notes)
            updateNoteWord(note);
    }

    m_selectionModel->updateOverlappedState();
}

void PianoRollGraphicsViewPrivate::onNoteSelectionChanged() {
    Q_Q(PianoRollGraphicsView);
    if (m_clip)
        m_selectionModel->updateSceneSelectionState();
}

void PianoRollGraphicsViewPrivate::onParamChanged(const ParamInfo::Name name,
                                                  const Param::Type type) const {
    if (name == ParamInfo::Pitch) {
        const auto pitchParam = m_clip->params.getParamByName(name);
        updatePitch(type, *pitchParam);
    }
}

void PianoRollGraphicsViewPrivate::onDeleteSelectedNotes() const {
    Q_Q(const PianoRollGraphicsView);
    const auto notes = q->selectedNotesId();
    clipController->onRemoveNotes(notes);
}

void PianoRollGraphicsViewPrivate::onOpenNotePropertyDialog(
    const int noteId, const AppGlobal::NotePropertyType propertyType) {
    Q_Q(PianoRollGraphicsView);
    const auto note = m_clip->findNoteById(noteId);
    Q_ASSERT(note);
    const auto dlg = new NotePropertyDialog(note, propertyType, q);
    connect(dlg, &NotePropertyDialog::accepted, this,
            [=] { clipController->onNotePropertiesEdited(noteId, dlg->result()); });
    dlg->show();
}

void PianoRollGraphicsViewPrivate::onStartEditingNoteLyric(NoteView *noteView) {
    // If another Note is already being edited, finish it first
    for (const auto view : noteViews) {
        if (view != noteView && view->isEditingLyric()) {
            view->finishEditingLyric();
        }
    }

    // Connect signals
    connect(
        noteView, &NoteView::lyricEditingFinished, this,
        [this, noteView](const QString &lyric) { onNoteLyricEditingFinished(noteView, lyric); });
    connect(noteView, &NoteView::tabKeyPressed, this,
            [this, noteView] { onNoteTabKeyPressed(noteView); });

    noteView->startEditingLyric();
}

void PianoRollGraphicsViewPrivate::onNoteLyricEditingFinished(NoteView *noteView,
                                                              const QString &lyric) {
    // Disconnect signals
    disconnect(noteView, &NoteView::lyricEditingFinished, this, nullptr);
    disconnect(noteView, &NoteView::tabKeyPressed, this, nullptr);

    // Save lyric changes
    const int noteId = noteView->id();
    const auto note = m_clip->findNoteById(noteId);
    Q_ASSERT(note);

    // Create NoteDialogResult
    NoteDialogResult result;
    result.lyric = lyric;
    result.language = note->language();
    result.pronunciation = note->pronunciation();
    result.phonemeNameSeq = note->phonemeNameSeq();

    clipController->onNotePropertiesEdited(noteId, result);
}

void PianoRollGraphicsViewPrivate::onNoteTabKeyPressed(NoteView *noteView) {
    Q_Q(PianoRollGraphicsView);
    // Save current Note's lyric first (finishEditingLyric will emit lyricEditingFinished signal)
    if (noteView->isEditingLyric()) {
        noteView->finishEditingLyric();
    }

    // Find next Note
    NoteView *nextNoteView = findNextNoteView(noteView);
    if (nextNoteView) {
        // Clear other selections and select the new note
        q->clearNoteSelections();
        nextNoteView->setSelected(true);
        // Update selection state to controller
        const auto notes = q->selectedNotesId();
        clipController->selectNotes(notes, true);
        // Start editing the new note
        onStartEditingNoteLyric(nextNoteView);
    }
}

NoteView *PianoRollGraphicsViewPrivate::findNextNoteView(NoteView *currentNoteView) const {
    if (!m_clip || !currentNoteView)
        return nullptr;

    const int currentRStart = currentNoteView->rStart();
    NoteView *nextNoteView = nullptr;
    int minRStart = INT_MAX;

    // Find all Notes after current Note, select the one with smallest rStart
    for (const auto view : noteViews) {
        if (view == currentNoteView || view->rStart() <= currentRStart)
            continue;

        if (view->rStart() < minRStart) {
            minRStart = view->rStart();
            nextNoteView = view;
        }
    }

    return nextNoteView;
}

void PianoRollGraphicsViewPrivate::onStartEditingPronunciation(PronunciationView *pronView) {
    for (const auto view : noteViews) {
        if (view->isEditingLyric()) {
            view->finishEditingLyric();
        }
        if (view->pronunciationView() && view->pronunciationView()->isEditingPronunciation()) {
            view->pronunciationView()->finishEditingPronunciation();
        }
    }

    connect(pronView, &PronunciationView::pronunciationEditingFinished, this,
            [this, pronView](const QString &pronunciation) {
                onPronunciationEditingFinished(pronView, pronunciation);
            });

    pronView->startEditingPronunciation();
}

void PianoRollGraphicsViewPrivate::onPronunciationEditingFinished(PronunciationView *pronView,
                                                                  const QString &pronunciation) {
    disconnect(pronView, &PronunciationView::pronunciationEditingFinished, this, nullptr);

    const int noteId = pronView->id();
    const auto note = m_clip->findNoteById(noteId);
    Q_ASSERT(note);

    NoteDialogResult result;
    result.lyric = note->lyric();
    result.language = note->language();
    result.pronunciation = note->pronunciation();
    result.pronunciation.edited = pronunciation;
    result.phonemeNameSeq = note->phonemeNameSeq();

    clipController->onNotePropertiesEdited(noteId, result);
}

void PianoRollGraphicsViewPrivate::moveToNullClipState() {
    Q_Q(PianoRollGraphicsView);
    m_pitchEditor->cancelEdit();
    endPitchEditSession(EditSessionEndReason::Cancel);
    q->setSceneVisibility(false);
    q->setEnabled(false);
    m_pitchEditor->clearParams();
    while (m_notes.count() > 0)
        handleNoteRemoved(m_notes.first());
    if (m_clip) {
        disconnect(m_clip, nullptr, this, nullptr);
    }
    m_clip = nullptr;
}

void PianoRollGraphicsViewPrivate::moveToSingingClipState(SingingClip *clip) {
    Q_Q(PianoRollGraphicsView);
    m_pitchEditor->cancelEdit();
    endPitchEditSession(EditSessionEndReason::Cancel);
    m_selectionModel->setSelectionChangeBarrier(true);
    while (m_notes.count() > 0)
        handleNoteRemoved(m_notes.first());
    if (m_clip) {
        disconnect(m_clip, nullptr, this, nullptr);
    }

    m_clip = clip;
    m_offset = clip->start();
    q->setOffset(m_offset);
    q->setSceneVisibility(true);
    q->setEnabled(true);
    q->setSceneLength(m_clip->length());
    m_clipRangeOverlay->setClipRange(clip->clipStart(), clip->clipLen());

    if (clip->notes().count() > 0) {
        for (const auto note : clip->notes())
            handleNoteInserted(note);
        const auto firstNote = *clip->notes().begin();
        q->setViewportCenterAt(firstNote->globalStart(), firstNote->keyIndex());
    } else
        q->setViewportCenterAt(clip->start(), 60);

    updatePitch(Param::Original, *m_clip->params.getParamByName(ParamInfo::Pitch));
    updatePitch(Param::Edited, *m_clip->params.getParamByName(ParamInfo::Pitch));

    connect(clip, &SingingClip::propertyChanged, this,
            &PianoRollGraphicsViewPrivate::onClipPropertyChanged);
    connect(clip, &SingingClip::noteChanged, this, &PianoRollGraphicsViewPrivate::onNoteChanged);
    connect(clip, &SingingClip::paramChanged, this, &PianoRollGraphicsViewPrivate::onParamChanged);
    m_selectionModel->setSelectionChangeBarrier(false);
}

void PianoRollGraphicsViewPrivate::updateNoteTimeAndKey(const Note *note) const {
    const auto noteView = findNoteViewById(note->id());
    if (!noteView) {
        logMissingNoteView("time-key", note->id());
        return;
    }
    Helper::updateNoteTimeAndKey(*noteView, *note);
}

void PianoRollGraphicsViewPrivate::updateNoteWord(const Note *note) const {
    const auto noteView = findNoteViewById(note->id());
    if (!noteView) {
        logMissingNoteView("word", note->id());
        return;
    }
    Helper::updateNoteWord(*noteView, *note);
}

void PianoRollGraphicsViewPrivate::setPitchEditMode(const bool on, const bool isErase) {
    Q_Q(PianoRollGraphicsView);
    if (on)
        q->setCursor(Qt::ArrowCursor);

    m_isEditPitchMode = on;
    for (const auto note : noteViews)
        note->setEditingPitch(on);
    if (on)
        q->clearNoteSelections();
    m_pitchEditor->setTransparentMouseEvents(!on);
    m_pitchEditor->setEraseMode(isErase);
}

NoteView *PianoRollGraphicsViewPrivate::noteViewAt(const QPoint &pos) {
    Q_Q(PianoRollGraphicsView);
    for (const auto item : q->items(pos))
        if (const auto noteItem = dynamic_cast<NoteView *>(item))
            return noteItem;
    return nullptr;
}

PronunciationView *PianoRollGraphicsViewPrivate::pronViewAt(const QPoint &pos) {
    Q_Q(PianoRollGraphicsView);
    for (const auto item : q->items(pos))
        if (const auto pronView = dynamic_cast<PronunciationView *>(item))
            return pronView;
    return nullptr;
}

// When erasing notes, the operation might be cancelled (e.g., pressing ESC),
// but in some cases (e.g., pronunciation updates) we still need to find and modify their properties
NoteView *PianoRollGraphicsViewPrivate::findNoteViewById(const int id) const {
    if (auto *view = noteViewIndex.value(id, nullptr))
        return view;
    return MathUtils::findItemById<NoteView *>(m_selectionModel->noteViewsToErase(), id);
}

void PianoRollGraphicsViewPrivate::handleNoteInserted(Note *note) {
    Q_Q(PianoRollGraphicsView);
    m_selectionModel->setSelectionChangeBarrier(true);
    const auto noteView = Helper::buildNoteView(*note);
    noteView->fontPixelSize = q->m_noteFontPixelSize;
    noteView->setEditingPitch(m_isEditPitchMode);
    addNoteViewToScene(noteView);
    m_notes.append(note);
    m_selectionModel->setSelectionChangeBarrier(false);
}

void PianoRollGraphicsViewPrivate::handleNoteRemoved(Note *note) {
    m_selectionModel->setSelectionChangeBarrier(true);
    const auto noteView = findNoteViewById(note->id());
    if (!noteView) {
        logMissingNoteView("remove", note->id());
        m_notes.removeOne(note);
        disconnect(note, nullptr, this, nullptr);
        m_selectionModel->setSelectionChangeBarrier(false);
        return;
    }
    removeNoteViewFromScene(noteView);
    delete noteView;
    m_notes.removeOne(note);
    disconnect(note, nullptr, this, nullptr);
    m_selectionModel->setSelectionChangeBarrier(false);
}

void PianoRollGraphicsViewPrivate::addNoteViewToScene(NoteView *view) {
    Q_Q(PianoRollGraphicsView);
    q->scene()->addCommonItem(view);
    q->scene()->addCommonItem(view->pronunciationView());
    noteViews.append(view);
    noteViewIndex.insert(view->id(), view);
}

void PianoRollGraphicsViewPrivate::removeNoteViewFromScene(NoteView *view) {
    Q_Q(PianoRollGraphicsView);
    if (view->scene() == q->scene()) {
        q->scene()->removeCommonItem(view);
        q->scene()->removeCommonItem(view->pronunciationView());
    }
    noteViews.removeOne(view);
    noteViewIndex.remove(view->id());
}

void PianoRollGraphicsViewPrivate::onHoverEnter(QHoverEvent *event) {
    if (m_currentHandler)
        m_currentHandler->hoverEnterEvent(event);
}

void PianoRollGraphicsViewPrivate::onHoverLeave(QHoverEvent *event) {
    Q_Q(PianoRollGraphicsView);
    if (m_currentHandler)
        m_currentHandler->hoverLeaveEvent(event);
    emit q->keyHoverCleared();
}

void PianoRollGraphicsViewPrivate::onHoverMove(const QHoverEvent *event) {
    Q_Q(PianoRollGraphicsView);
    if (m_isEditPitchMode || m_mouseDown)
        return;

    // Update keyboard hover based on mouse position
    const auto scenePos = q->mapToScene(event->position().toPoint());
    const auto keyIndex =
        PianoRollCoord::sceneYToKeyIndexInt(scenePos.y(), q->scaleY() * noteHeight);
    emit q->keyHovered(keyIndex);

    if (m_editMode == SplitNote) {
        if (m_currentHandler) {
            m_currentHandler->hoverMoveEvent(const_cast<QHoverEvent *>(event));
            return;
        }
    }

    // In DrawNote and EraseNote modes, don't show note edge resize cursor
    if (m_editMode == DrawNote || m_editMode == EraseNote) {
        q->setCursor(Qt::ArrowCursor);
        return;
    }

    const auto noteView = noteViewAt(event->position().toPoint());
    if (!noteView) {
        q->setCursor(Qt::ArrowCursor);
        return;
    }

    const auto rPos = noteView->mapFromScene(scenePos);
    const auto rx = rPos.x();
    if (rx >= 0 && rx <= AppGlobal::resizeTolerance) {
        q->setCursor(Qt::SizeHorCursor);
    } else if (rx >= noteView->rect().width() - AppGlobal::resizeTolerance &&
               rx <= noteView->rect().width()) {
        q->setCursor(Qt::SizeHorCursor);
    } else {
        q->setCursor(Qt::ArrowCursor);
    }
}

void PianoRollGraphicsViewPrivate::onClipPropertyChanged() {
    Q_Q(PianoRollGraphicsView);
    m_offset = m_clip->start();
    q->setOffset(m_offset);
    q->setSceneLength(m_clip->length());
    m_clipRangeOverlay->setClipRange(m_clip->clipStart(), m_clip->clipLen());

    for (const auto note : m_notes) {
        updateNoteTimeAndKey(note);
    }
}

void PianoRollGraphicsViewPrivate::updatePitch(const Param::Type paramType,
                                               const Param &param) const {
    Helper::updatePitch(paramType, param, *m_pitchEditor);
    if (paramType == Param::Edited) {
        auto *handler = dynamic_cast<EditPitchAnchorHandler *>(m_handlers.value(EditPitchAnchor));
        if (handler)
            Helper::updateAnchorPitch(param, *handler);
    }
}
