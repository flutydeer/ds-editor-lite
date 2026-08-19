#include "PianoRollGraphicsView.h"

#include "ClipRangeOverlay.h"
#include "NoteEditUtils.h"
#include "NoteView.h"
#include "PianoRollBackground.h"
#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsViewHelper.h"
#include "PianoRollCoord.h"
#include "PianoRollContextMenuController.h"
#include "PitchDisplayStrategy.h"
#include "NoteInteractionController.h"
#include "PianoRollSelectionModel.h"
#include "PianoRollGraphicsView_p.h"
#include "UI/Views/ClipEditor/AnchorEditor/AnchorOverlayView.h"
#include "UI/Views/Common/EditorResizeUtils.h"
#include "PitchEditorView.h"
#include "PronunciationView.h"
#include "PianoRollEditHandler.h"

#include "DrawNoteHandler.h"
#include "EditPitchAnchorHandler.h"
#include "EraseNoteHandler.h"

#include "SelectNoteHandler.h"
#include "SplitNoteHandler.h"
#include "Controller/ClipController.h"
#include "Controller/PlaybackController.h"
#include "Controller/Actions/AppModel/Note/NoteActions.h"
#include "Global/AppGlobal.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include <lite/GUI/Controls/InlineTextEditOverlay.h>
#include <lite/GUI/Controls/ToolTip.h>
#include <lite/Support/Linq.h>
#include <lite/Support/MathUtils.h>
#include <lite/MusicBase/TimelineSnapUtils.h>
#include <climits>
#include <cmath>
#include <limits>

#include <QDebug>
#include <QCursor>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QHideEvent>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextDocument>

namespace Helper = PianoRollGraphicsViewHelper;

namespace {
    void logMissingNoteView(const char *context, const int noteId) {
        qWarning() << "Ignore note update because note view is missing"
                   << "context:" << context << "noteId:" << noteId;
    }
}

PianoRollGraphicsView::PianoRollGraphicsView(PianoRollGraphicsScene *scene, QWidget *parent)
    : TimeGraphicsView(scene, true, parent), d_ptr(new PianoRollGraphicsViewPrivate(this)) {
    Q_D(PianoRollGraphicsView);
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("PianoRollGraphicsView");
    setScaleXMax(5);
    setPixelsPerQuarterNote(pixelsPerQuarterNote);
    setSceneVisibility(false);
    setDragBehavior(DragBehavior::RectSelect);
    setMinimumHeight(0);

    d->m_inlineEditor = new InlineTextEditOverlay(viewport());
    connect(d->m_inlineEditor, &InlineTextEditOverlay::textSubmitted, d,
            &PianoRollGraphicsViewPrivate::onInlineTextSubmitted);
    connect(d->m_inlineEditor, &InlineTextEditOverlay::navigationRequested, d,
            &PianoRollGraphicsViewPrivate::onInlineNavigationRequested);
    connect(d->m_inlineEditor, &InlineTextEditOverlay::editCancelled, d,
            &PianoRollGraphicsViewPrivate::onInlineEditCancelled);
    d->m_lyricToolTip = new ToolTip(QString(), viewport());
    d->m_lyricToolTip->setAnimationEnabled(false);

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

    d->m_anchorEditor = new AnchorOverlayView(
        [d](const double value) { return d->m_pitchEditor->valueToSceneY(value); },
        [d](const double y) { return d->m_pitchEditor->sceneYToValue(y); });
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
    d->m_pitchEditor->setAnchorOverlayState(&editPitchAnchorHandler->overlayState());
    editPitchAnchorHandler->setAlwaysVisible(true);

    connect(scene, &QGraphicsScene::selectionChanged, this,
            &PianoRollGraphicsView::onSceneSelectionChanged);

    connect(this, &TimeGraphicsView::scaleChanged, this,
            &PianoRollGraphicsView::notifyKeyRangeChanged);
    connect(this, &TimeGraphicsView::visibleRectChanged, this,
            &PianoRollGraphicsView::notifyKeyRangeChanged);
    connect(this, &TimeGraphicsView::scaleChanged, d,
            &PianoRollGraphicsViewPrivate::finishInlineEditing);
    connect(this, &TimeGraphicsView::visibleRectChanged, d,
            &PianoRollGraphicsViewPrivate::finishInlineEditing);
    connect(this, &TimeGraphicsView::sizeChanged, d,
            &PianoRollGraphicsViewPrivate::finishInlineEditing);
    connect(this, &TimeGraphicsView::scaleChanged, d,
            &PianoRollGraphicsViewPrivate::hideLyricToolTip);
    connect(this, &TimeGraphicsView::visibleRectChanged, d,
            &PianoRollGraphicsViewPrivate::hideLyricToolTip);
    connect(this, &TimeGraphicsView::sizeChanged, d,
            &PianoRollGraphicsViewPrivate::hideLyricToolTip);
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
    d->hideLyricToolTip();
    // 切换 clip 时清空编辑预览，避免残留到新 clip
    appStatus->pianoRollNoteEditPreview = {};
    appStatus->pianoRollNoteErasePreview = {};
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

bool PianoRollGraphicsView::event(QEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
        const auto key = dynamic_cast<QKeyEvent *>(event)->key();
        if (d->m_editMode == EditPitchAnchor &&
            AnchorEditor::AnchorEditController::handlesKey(key)) {
            if (event->type() == QEvent::ShortcutOverride) {
                event->accept();
                return true;
            }
        } else if (key == Qt::Key_Escape) {
            discardAction();
        }
    } else if (event->type() == QEvent::WindowDeactivate) {
        d->hideLyricToolTip();
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
    if (!d->m_clip)
        return;

    PianoRollMenuContext context;
    context.globalPos = event->globalPos();
    if (!EditorViewGlobal::isPitchEditMode(d->m_editMode)) {
        const auto scenePos = mapToScene(event->pos());
        context.globalTick = qRound(sceneXToTick(scenePos.x())) + d->m_offset;
        context.keyIndex = 127 - qFloor(scenePos.y() / noteHeight);
        // The pronunciation glyph sits below the note rect, outside notes():
        // hit-test it first and drive the menu entirely from its own id.
        if (auto *pronView = d->pronViewAt(event->pos()); pronView && pronView->id() >= 0) {
            const auto noteId = pronView->id();
            auto *noteView = d->findNoteViewById(noteId);
            if (noteView && !noteView->isSelected())
                d->m_selectionModel->selectOnly(noteView);
            context.target = PianoRollMenuContext::Target::Note;
            context.noteId = noteId;
            context.selectedNoteIds = selectedNotesId();
            if (const auto *note = d->m_clip->findNoteById(noteId)) {
                context.noteLanguage = note->language();
                context.phonemeEditorEnabled =
                    context.selectedNoteIds.size() == 1 && note->canEditPhonemes();
            }
            context.pronunciationTarget = true;
            emit contextMenuRequested(context);
            event->accept();
            return;
        }
        if (auto *noteView = d->noteViewAt(event->pos())) {
            if (!noteView->isSelected())
                d->m_selectionModel->selectOnly(noteView);
            const auto *note = d->m_clip->findNoteById(noteView->id());
            Q_ASSERT(note);
            context.target = PianoRollMenuContext::Target::Note;
            context.noteId = noteView->id();
            context.selectedNoteIds = selectedNotesId();
            context.noteLanguage = note->language();
            context.phonemeEditorEnabled =
                context.selectedNoteIds.size() == 1 && note->canEditPhonemes();
        } else {
            context.target = PianoRollMenuContext::Target::Background;
        }
        emit contextMenuRequested(context);
        event->accept();
    } else if (d->m_editMode == EditPitchAnchor) {
        auto *handler = dynamic_cast<EditPitchAnchorHandler *>(d->m_currentHandler);
        if (handler && handler->prepareMenuContext(event, context))
            emit contextMenuRequested(context);
        event->accept();
    } else {
        TimeGraphicsView::contextMenuEvent(event);
    }
}

void PianoRollGraphicsView::showPianoRollPastePreview(const PianoRollPastePreviewData &data,
                                                      const int globalTick) {
    Q_D(PianoRollGraphicsView);
    if (!d->m_clip || !d->m_selectionModel->pastePreviewViews().isEmpty())
        return;

    const auto localStart = globalTick - d->m_offset;
    for (const auto &note : data.notes) {
        auto *noteView = new NoteView(-1);
        noteView->setPronunciationView(new PronunciationView(-1));
        noteView->setRStart(note.relativeStart + localStart);
        noteView->setLength(note.length);
        noteView->setKeyIndex(note.key);
        noteView->setLyric(note.lyric);
        noteView->setPronunciation(note.pronunciation, note.pronunciationEdited);
        noteView->setOverlapped(note.overlapped);
        noteView->setOpacity(0.35);
        noteView->setAcceptedMouseButtons(Qt::NoButton);
        noteView->setAcceptHoverEvents(false);
        noteView->setFlag(QGraphicsItem::ItemIsSelectable, false);
        if (noteView->pronunciationView())
            noteView->pronunciationView()->setOpacity(0.35);

        scene()->addCommonItem(noteView);
        if (noteView->pronunciationView())
            scene()->addCommonItem(noteView->pronunciationView());
        d->m_selectionModel->appendPastePreviewView(noteView);
    }
}

void PianoRollGraphicsView::clearPianoRollPastePreview() {
    Q_D(PianoRollGraphicsView);
    d->m_selectionModel->clearPastePreviewViews();
}

void PianoRollGraphicsView::setSelectedAnchorInterpolation(const PianoRollAnchorMode mode) {
    Q_D(PianoRollGraphicsView);
    if (auto *handler =
            dynamic_cast<EditPitchAnchorHandler *>(d->m_handlers.value(EditPitchAnchor))) {
        handler->setSelectedInterpolation(mode);
    }
}

void PianoRollGraphicsView::deleteSelectedAnchors() {
    Q_D(PianoRollGraphicsView);
    if (auto *handler =
            dynamic_cast<EditPitchAnchorHandler *>(d->m_handlers.value(EditPitchAnchor))) {
        handler->deleteSelectedNodesFromMenu();
    }
}

void PianoRollGraphicsView::mousePressEvent(QMouseEvent *event) {
    Q_D(PianoRollGraphicsView);
    d->hideLyricToolTip();
    if (d->m_interactionController->isMouseDown()) {
        qWarning() << "Ignored mousePressEvent" << event
                   << "because there is already one mouse button pressed";
        return;
    }
    d->m_interactionController->setMouseDown(true, event->button());

    cancelRequested = false;
    d->m_selectionModel->setSelecting(true);
    d->m_selectionModel->setSelectionChangeBarrier(true);
    if (event->button() != Qt::LeftButton &&
        !EditorViewGlobal::isPitchEditMode(d->m_editMode)) {
        d->m_interactionController->setMouseMoveBehavior(NoteInteractionController::None);
        auto *noteView = d->noteViewAt(event->pos());
        if (!noteView) {
            if (const auto *pronView = d->pronViewAt(event->pos()))
                noteView = d->findNoteViewById(pronView->id());
        }
        (void) d->m_selectionModel->applyPressSelection(noteView, false);
        if (!noteView)
            TimeGraphicsView::mousePressEvent(event);
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
        if (!noteView) {
            d->m_selectionModel->clearSelectionAnchor();
            TimeGraphicsView::mousePressEvent(event);
        }
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
        if (!noteView) {
            d->m_selectionModel->clearSelectionAnchor();
            TimeGraphicsView::mousePressEvent(event);
        }
    } else if (d->m_editMode == EditPitchAnchor) {
        if (d->m_currentHandler)
            d->m_currentHandler->mousePressEvent(event);
    } else
        TimeGraphicsView::mousePressEvent(event);

    if (event->button() == Qt::LeftButton) {
        Qt::Orientations axes;
        const auto behavior = d->m_interactionController->mouseMoveBehavior();
        if (behavior != NoteInteractionController::None) {
            axes = behavior == NoteInteractionController::Move
                       ? (Qt::Horizontal | Qt::Vertical)
                       : Qt::Orientations(Qt::Horizontal);
        } else if (d->m_currentHandler) {
            axes = d->m_currentHandler->edgeAutoScrollAxes();
            if (!axes && d->m_editMode == EditPitchAnchor)
                axes = Qt::Horizontal | Qt::Vertical;
        }
        if (axes)
            armEdgeAutoScroll(axes, event->pos());
    }
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
    if (!d->m_interactionController->isMouseDown()) {
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
        if (d->m_currentHandler->mouseMoveEvent(event)) {
            // Arm edge auto scroll for handler-driven drags (draw/erase/anchor)
            if (const auto axes = d->m_currentHandler->edgeAutoScrollAxes())
                armEdgeAutoScroll(axes);
            return;
        }
    }
    if (d->m_interactionController->mouseMoveBehavior() == NoteInteractionController::None) {
        TimeGraphicsView::mouseMoveEvent(event);
        return;
    }
    if (cancelRequested || d->m_interactionController->isMouseDown() == false)
        return;

    // Moving notes may scroll on both axes; resizing is horizontal only
    armEdgeAutoScroll(d->m_interactionController->mouseMoveBehavior() ==
                              NoteInteractionController::Move
                          ? (Qt::Horizontal | Qt::Vertical)
                          : Qt::Orientations(Qt::Horizontal));

    updateNoteDragAt(event->position().toPoint(), event->modifiers());
}

void PianoRollGraphicsView::updateNoteDragAt(const QPoint &viewportPos,
                                             const Qt::KeyboardModifiers modifiers) {
    Q_D(PianoRollGraphicsView);

    if (modifiers == Qt::AltModifier)
        d->m_interactionController->setTempQuantizeOff(true);
    else
        d->m_interactionController->setTempQuantizeOff(false);

    const auto scenePos = mapToScene(viewportPos);
    const auto quantizedTickLength = TimelineSnapUtils::quantizeStep(
        appStatus->pianoRollQuantize, d->m_interactionController->tempQuantizeOff());
    const auto globalTick = sceneXToTick(scenePos.x()) + d->m_offset;
    const auto snappedTick = NoteEditUtils::snapLocalDown(
        globalTick, d->m_offset, quantizedTickLength, appModel->timeline());
    const auto snappedTickNearest = NoteEditUtils::snapLocalNearest(
        globalTick, d->m_offset, quantizedTickLength, appModel->timeline());
    const auto keyIndex = PianoRollCoord::sceneYToKeyIndexInt(scenePos.y(), scaleY() * noteHeight);
    const auto deltaX = sceneXToTick(scenePos.x() - d->m_interactionController->mouseDownPos().x());

    if (!d->m_interactionController->movedBeforeMouseUp()) {
        QList<int> noteIds;
        for (const auto *note : d->m_selectionModel->selectedNoteItems())
            noteIds.append(note->id());
        editSessionManager->beginTransaction(AppStatus::EditObjectType::Note,
                                             d->m_clip ? d->m_clip->id() : -1, {}, noteIds);
        appStatus->currentEditObject = AppStatus::EditObjectType::Note;
        d->m_interactionController->setMovedBeforeMouseUp(true);
    }

    // TODO: Optimize note moving and resizing
    if (d->m_interactionController->mouseMoveBehavior() == NoteInteractionController::Move) {
        auto startOffset = NoteEditUtils::moveDelta(deltaX, quantizedTickLength);
        int minLocalStart = std::numeric_limits<int>::max();
        for (const auto *note : d->m_selectionModel->selectedNoteItems())
            minLocalStart = std::min(minLocalStart, note->rStart());
        if (minLocalStart != std::numeric_limits<int>::max())
            startOffset = NoteResizeUtils::clampLeftMoveDelta(startOffset, minLocalStart);
        auto keyOffset = keyIndex - d->m_interactionController->mouseDownKeyIndex();
        if (keyOffset > d->m_interactionController->moveMaxDeltaKey())
            keyOffset = d->m_interactionController->moveMaxDeltaKey();
        if (keyOffset < d->m_interactionController->moveMinDeltaKey())
            keyOffset = d->m_interactionController->moveMinDeltaKey();
        d->m_interactionController->setDeltaTick(startOffset);
        d->m_interactionController->setDeltaKey(keyOffset);
        d->m_interactionController->moveSelectedNotes(d->m_interactionController->deltaTick(),
                                                      d->m_interactionController->deltaKey());
    } else if (d->m_interactionController->mouseMoveBehavior() ==
               NoteInteractionController::ResizeLeft) {
        const auto deltaStart = NoteEditUtils::leftResizeDelta(
            d->m_interactionController->mouseDownRStart(),
            d->m_interactionController->mouseDownLength(), snappedTick, quantizedTickLength);
        d->m_interactionController->setDeltaTick(deltaStart);
        d->m_interactionController->resizeLeftSelectedNote(d->m_interactionController->deltaTick());
    } else if (d->m_interactionController->mouseMoveBehavior() ==
               NoteInteractionController::ResizeRight) {
        const auto deltaLength = NoteEditUtils::rightResizeDelta(
            d->m_interactionController->mouseDownRStart(),
            d->m_interactionController->mouseDownLength(), snappedTickNearest, quantizedTickLength);
        d->m_interactionController->setDeltaTick(deltaLength);
        d->m_interactionController->resizeRightSelectedNote(
            d->m_interactionController->deltaTick());
    }
    publishNoteEditPreview();
}

void PianoRollGraphicsView::onEdgeAutoScrollFrame(const QPoint &clampedViewportPos,
                                                  const Qt::KeyboardModifiers modifiers) {
    Q_D(PianoRollGraphicsView);
    if (cancelRequested)
        return;

    if (d->m_interactionController->mouseMoveBehavior() != NoteInteractionController::None &&
        d->m_interactionController->isMouseDown()) {
        updateNoteDragAt(clampedViewportPos, modifiers);
        return;
    }
    if (d->m_currentHandler && d->m_currentHandler->edgeAutoScrollAxes()) {
        d->m_currentHandler->continueDragAt(clampedViewportPos);
        return;
    }
    // Rubber band selection is handled by the base class
    TimeGraphicsView::onEdgeAutoScrollFrame(clampedViewportPos, modifiers);
}

void PianoRollGraphicsView::publishNoteEditPreview() const {
    Q_D(const PianoRollGraphicsView);
    QVector<AppStatus::NoteEditPreview> preview;
    const auto behavior = d->m_interactionController->mouseMoveBehavior();
    if (behavior == NoteInteractionController::Move) {
        for (const auto note : d->m_selectionModel->selectedNoteItems()) {
            preview.append({note->id(), note->rStart() + note->startOffset(),
                            note->length() + note->lengthOffset(),
                            note->keyIndex() + note->keyOffset()});
        }
    } else if (behavior == NoteInteractionController::ResizeLeft ||
               behavior == NoteInteractionController::ResizeRight) {
        if (const auto note = d->m_interactionController->currentEditingNote()) {
            preview.append({note->id(), note->rStart() + note->startOffset(),
                            note->length() + note->lengthOffset(),
                            note->keyIndex() + note->keyOffset()});
        }
    }
    appStatus->pianoRollNoteEditPreview = preview;
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
    if (!cancelRequested) {
        d->m_interactionController->finalizeClickSelection();
        commitAction();
    }
    cancelRequested = false;
    TimeGraphicsView::mouseReleaseEvent(event);
}

void PianoRollGraphicsView::mouseDoubleClickEvent(QMouseEvent *event) {
    // Disable double-click event to prevent deselecting notes when double-clicking on scrollbar
    Q_D(PianoRollGraphicsView);
    d->hideLyricToolTip();
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
            armEdgeAutoScroll(Qt::Horizontal, event->pos());
        }
    }
}

void PianoRollGraphicsView::keyPressEvent(QKeyEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (d->m_currentHandler && d->m_currentHandler->keyPressEvent(event))
        return;
    TimeGraphicsView::keyPressEvent(event);
}

void PianoRollGraphicsView::showEvent(QShowEvent *event) {
    Q_D(PianoRollGraphicsView);
    TimeGraphicsView::showEvent(event);
    if (!d->m_initialViewportPositionPending)
        return;

    d->positionViewportAtClipContent();
    d->m_initialViewportPositionPending = false;
}

void PianoRollGraphicsView::hideEvent(QHideEvent *event) {
    Q_D(PianoRollGraphicsView);
    d->hideLyricToolTip();
    TimeGraphicsView::hideEvent(event);
}

int PianoRollGraphicsView::noteFontPixelSize() const {
    return m_noteFontPixelSize;
}

void PianoRollGraphicsView::setNoteFontPixelSize(const int size) {
    Q_D(PianoRollGraphicsView);
    d->hideLyricToolTip();
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

QColor PianoRollGraphicsView::noteSelectedBorderColor() const {
    return NoteView::selectedBorderColor();
}

void PianoRollGraphicsView::setNoteSelectedBorderColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    if (NoteView::selectedBorderColor() == color)
        return;
    NoteView::setSelectedBorderColor(color);
    for (const auto noteView : d->noteViews)
        noteView->update();
}

QColor PianoRollGraphicsView::pronunciationTextColor() const {
    return PronunciationView::textColor();
}

void PianoRollGraphicsView::setPronunciationTextColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    if (PronunciationView::textColor() == color)
        return;
    PronunciationView::setTextColor(color);
    for (const auto noteView : d->noteViews)
        if (const auto pronView = noteView->pronunciationView())
            pronView->update();
}

QColor PianoRollGraphicsView::anchorColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_anchorEditor->anchorColor();
}

void PianoRollGraphicsView::setAnchorColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_anchorEditor->setAnchorColor(color);
}

QColor PianoRollGraphicsView::anchorSelectedColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_anchorEditor->anchorSelectedColor();
}

void PianoRollGraphicsView::setAnchorSelectedColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_anchorEditor->setAnchorSelectedColor(color);
}

QColor PianoRollGraphicsView::anchorCurveColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_anchorEditor->anchorCurveColor();
}

void PianoRollGraphicsView::setAnchorCurveColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_anchorEditor->setAnchorCurveColor(color);
}

QColor PianoRollGraphicsView::anchorPreviewColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_anchorEditor->anchorPreviewColor();
}

void PianoRollGraphicsView::setAnchorPreviewColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_anchorEditor->setAnchorPreviewColor(color);
}

QColor PianoRollGraphicsView::clipRangeOverlayColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_clipRangeOverlay->fillColor();
}

void PianoRollGraphicsView::setClipRangeOverlayColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_clipRangeOverlay->setFillColor(color);
}

QColor PianoRollGraphicsView::splitLineColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_splitLineColor;
}

void PianoRollGraphicsView::setSplitLineColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    if (d->m_splitLineColor == color)
        return;
    d->m_splitLineColor = color;
    if (const auto handler = dynamic_cast<SplitNoteHandler *>(d->m_handlers.value(SplitNote)))
        handler->applySplitLineColor(color);
}

QColor PianoRollGraphicsView::paramGraduateColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_pitchEditor->graduateColor();
}

void PianoRollGraphicsView::setParamGraduateColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_pitchEditor->setGraduateColor(color);
}

QColor PianoRollGraphicsView::paramOriginalCurveColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_pitchEditor->originalCurveColor();
}

void PianoRollGraphicsView::setParamOriginalCurveColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_pitchEditor->setOriginalCurveColor(color);
}

QColor PianoRollGraphicsView::paramEditedCurveColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_pitchEditor->editedCurveColor();
}

void PianoRollGraphicsView::setParamEditedCurveColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_pitchEditor->setEditedCurveColor(color);
}

QColor PianoRollGraphicsView::paramBackgroundLayerColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_pitchEditor->backgroundLayerColor();
}

void PianoRollGraphicsView::setParamBackgroundLayerColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_pitchEditor->setBackgroundLayerColor(color);
}

void PianoRollGraphicsView::reset() {
    Q_D(PianoRollGraphicsView);
    d->m_selectionModel->clearSelectionAnchor();
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

HistoryFocusVisibility PianoRollGraphicsView::focusVisibility(const HistoryFocus &focus) const {
    Q_D(const PianoRollGraphicsView);
    if (focus.kind != HistoryFocusKind::PianoRollNotes || !focus.isValid() || !d->m_clip ||
        d->m_clip->id() != focus.containerId) {
        return HistoryFocusVisibility::Unavailable;
    }

    QRectF itemBounds;
    for (const auto id : focus.objectIds) {
        if (const auto item = d->findNoteViewById(id))
            itemBounds = itemBounds.isNull() ? item->sceneBoundingRect()
                                             : itemBounds.united(item->sceneBoundingRect());
    }
    if (!itemBounds.isNull())
        return logicalVisibleRect().intersects(itemBounds) ? HistoryFocusVisibility::Visible
                                                           : HistoryFocusVisibility::ScrollRequired;

    const auto globalStart =
        focus.ticksAreLocal ? d->m_clip->start() + focus.tickStart : focus.tickStart;
    const auto globalEnd = focus.ticksAreLocal ? d->m_clip->start() + focus.tickEnd : focus.tickEnd;
    const auto logicalRect = logicalVisibleRect();
    const auto visibleStartTick = sceneXToTick(logicalRect.left()) + d->m_offset;
    const auto visibleEndTick = sceneXToTick(logicalRect.right()) + d->m_offset;
    const auto tickVisible = globalEnd >= visibleStartTick && globalStart <= visibleEndTick;
    const auto logicalTopKey =
        PianoRollCoord::sceneYToKeyIndexDouble(logicalRect.top(), scaleY() * noteHeight);
    const auto logicalBottomKey =
        PianoRollCoord::sceneYToKeyIndexDouble(logicalRect.bottom(), scaleY() * noteHeight);
    const auto keyVisible = focus.valueEnd >= logicalBottomKey && focus.valueStart <= logicalTopKey;
    return tickVisible && keyVisible ? HistoryFocusVisibility::Visible
                                     : HistoryFocusVisibility::ScrollRequired;
}

bool PianoRollGraphicsView::revealFocus(const HistoryFocus &focus) {
    return revealFocus(focus, true);
}

bool PianoRollGraphicsView::revealFocus(const HistoryFocus &focus, const bool animated) {
    Q_D(PianoRollGraphicsView);
    if (focus.kind != HistoryFocusKind::PianoRollNotes || !focus.isValid() || !d->m_clip ||
        d->m_clip->id() != focus.containerId) {
        return false;
    }

    QList<int> selectedIds;
    QRectF itemBounds;
    for (const auto id : focus.objectIds) {
        if (const auto item = d->findNoteViewById(id)) {
            selectedIds.append(id);
            itemBounds = itemBounds.isNull() ? item->sceneBoundingRect()
                                             : itemBounds.united(item->sceneBoundingRect());
        }
    }
    clipController->selectNotes(selectedIds, true);
    if (!itemBounds.isNull()) {
        ensureSceneRectVisible(itemBounds, 24, 24, animated);
        return true;
    }

    const auto localStart =
        focus.ticksAreLocal ? focus.tickStart : focus.tickStart - d->m_clip->start();
    const auto localEnd = focus.ticksAreLocal ? focus.tickEnd : focus.tickEnd - d->m_clip->start();
    const auto left = tickToSceneX(localStart);
    const auto right = tickToSceneX(localEnd);
    const auto keyHeight = scaleY() * noteHeight;
    const auto top = PianoRollCoord::keyIndexToSceneY(focus.valueEnd, keyHeight);
    const auto bottom = PianoRollCoord::keyIndexToSceneY(focus.valueStart, keyHeight) + keyHeight;
    ensureSceneRectVisible(
        QRectF(left, top, qMax(1.0, right - left), qMax(keyHeight, bottom - top)), 24, 24,
        animated);
    return true;
}

void PianoRollGraphicsView::discardAction() {
    Q_D(PianoRollGraphicsView);
    d->m_selectionModel->cancelPressSelection();
    appStatus->pianoRollNoteEditPreview = {};
    appStatus->pianoRollNoteErasePreview = {};
    d->m_pitchEditor->discardAction();
    cancelRequested = true;
    disarmEdgeAutoScroll();
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
            const auto minimumLength = TimelineSnapUtils::quantizeStep(
                appStatus->pianoRollQuantize, d->m_interactionController->tempQuantizeOff());
            NoteInteractionController::handleNoteLeftResized(
                d->m_interactionController->currentEditingNote()->id(),
                d->m_interactionController->deltaTick(), minimumLength);
        }
    } else if (d->m_interactionController->mouseMoveBehavior() ==
               NoteInteractionController::ResizeRight) {
        if (d->m_interactionController->movedBeforeMouseUp() &&
            d->m_interactionController->currentEditingNote()) {
            d->m_interactionController->resetSelectedNotesOffset();
            const auto minimumLength = TimelineSnapUtils::quantizeStep(
                appStatus->pianoRollQuantize, d->m_interactionController->tempQuantizeOff());
            NoteInteractionController::handleNoteRightResized(
                d->m_interactionController->currentEditingNote()->id(),
                d->m_interactionController->deltaTick(), minimumLength);
        }
    }
    // model 写入完成后才清空预览，避免轨道先画旧几何再跳变
    appStatus->pianoRollNoteEditPreview = {};
    appStatus->pianoRollNoteErasePreview = {};
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

double PianoRollGraphicsView::centerKeyIndex() const {
    return PianoRollCoord::centerYToKeyIndex(visibleRect().center().y(),
                                             scaleY() * noteHeight);
}

void PianoRollGraphicsView::setViewportCenterAt(const double tick, const double keyIndex,
                                                const bool animated) {
    setViewportCenterAtTick(tick);
    setViewportCenterAtKeyIndex(keyIndex, animated);
}

void PianoRollGraphicsView::setViewportCenterAtKeyIndex(const double keyIndex,
                                                        const bool animated) {
    const auto centerY =
        PianoRollCoord::keyIndexToCenterY(keyIndex, scaleY() * noteHeight);
    const auto vBarValue = qRound(centerY - viewport()->height() * 0.5);
    if (animated)
        verticalBarAnimateTo(vBarValue);
    else
        setVerticalBarValue(vBarValue);
}

void PianoRollGraphicsView::setEditMode(const PianoRollEditMode mode) {
    Q_D(PianoRollGraphicsView);
    d->hideLyricToolTip();
    if (d->m_editMode != mode) {
        d->finishInlineEditing();
        discardAction();
    }

    if (d->m_currentHandler)
        d->m_currentHandler->deactivate();

    d->m_editMode = mode;
    d->m_currentHandler = d->m_handlers.value(mode, nullptr);

    const auto displayMode = PitchDisplayStrategy::displayModeForEditMode(mode);
    d->m_pitchEditor->setDisplayMode(displayMode);
    d->m_anchorEditor->setDisplayMode(displayMode);

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
    hideLyricToolTip();
    finishInlineEditing();
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
               type == SingingClip::EditedWordPropertyChange ||
               type == SingingClip::EditedPronunciationOnly) {
        for (const auto &note : notes)
            updateNoteWord(note);
    }

    m_selectionModel->updateOverlappedState();
}

void PianoRollGraphicsViewPrivate::onNoteSelectionChanged() {
    Q_Q(PianoRollGraphicsView);
    finishInlineEditing();
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

void PianoRollGraphicsViewPrivate::onStartEditingNoteLyric(NoteView *noteView) {
    Q_Q(PianoRollGraphicsView);
    if (!noteView || noteView->id() < 0 || !m_clip)
        return;
    if (m_inlineEditField == InlineEditField::Lyric && m_inlineEditingNoteId == noteView->id() &&
        m_inlineEditor->isEditing())
        return;

    hideLyricToolTip();
    finishInlineEditing();

    m_inlineEditField = InlineEditField::Lyric;
    m_inlineEditingNoteId = noteView->id();
    noteView->setEditingLyric(true);

    auto anchorRect = q->mapFromScene(noteView->sceneBoundingRect()).boundingRect();
    const auto viewportRect = q->viewport()->rect();
    const int width = qMin(viewportRect.width(), qMax(40, anchorRect.width()));
    const int height = qMin(viewportRect.height(), qMax(20, anchorRect.height()));
    anchorRect.setSize({width, height});
    anchorRect.moveLeft(qBound(0, anchorRect.left(), viewportRect.width() - width));
    anchorRect.moveTop(qBound(0, anchorRect.top(), viewportRect.height() - height));

    QFont font;
    font.setPixelSize(noteView->fontPixelSize);
    const QVariantMap properties = {
        {QStringLiteral("editRole"),          QStringLiteral("Lyric")},
        {QStringLiteral("navigationEnabled"), true                   },
    };
    m_inlineEditor->showAt(anchorRect, noteView->lyric(), font, properties);
}

void PianoRollGraphicsViewPrivate::finishInlineEditing() {
    if (m_inlineEditor && m_inlineEditor->isEditing())
        m_inlineEditor->submit();
}

void PianoRollGraphicsViewPrivate::onInlineTextSubmitted(const QString &text) {
    const auto field = m_inlineEditField;
    const int noteId = m_inlineEditingNoteId;
    onInlineEditCancelled();
    if (field == InlineEditField::Lyric)
        applyLyricEdit(noteId, text);
    else if (field == InlineEditField::Pronunciation)
        applyPronunciationEdit(noteId, text);
}

void PianoRollGraphicsViewPrivate::applyLyricEdit(const int noteId, const QString &text) {
    if (!m_clip)
        return;
    const auto note = m_clip->findNoteById(noteId);
    if (!note)
        return;

    auto lyric = text.trimmed();
    if (lyric.isEmpty())
        lyric = appOptions->general()->defaultLyricForLanguage(note->language());
    if (lyric == note->lyric())
        return;

    clipController->onNoteLyricEdited(noteId, lyric);
}

void PianoRollGraphicsViewPrivate::onInlineNavigationRequested(const QString &text,
                                                               const bool backwards) {
    Q_Q(PianoRollGraphicsView);
    const int noteId = m_inlineEditingNoteId;
    auto *currentNoteView = findNoteViewById(noteId);
    onInlineTextSubmitted(text);
    const auto nextNoteView = findAdjacentNoteView(currentNoteView, backwards);
    if (nextNoteView) {
        m_selectionModel->selectOnly(nextNoteView);
        const auto notes = q->selectedNotesId();
        clipController->selectNotes(notes, true);
        onStartEditingNoteLyric(nextNoteView);
    }
}

void PianoRollGraphicsViewPrivate::onInlineEditCancelled() {
    const auto field = m_inlineEditField;
    const int noteId = m_inlineEditingNoteId;
    m_inlineEditField = InlineEditField::None;
    m_inlineEditingNoteId = -1;
    if (const auto noteView = findNoteViewById(noteId)) {
        if (field == InlineEditField::Lyric)
            noteView->setEditingLyric(false);
        else if (field == InlineEditField::Pronunciation && noteView->pronunciationView())
            noteView->pronunciationView()->setEditingPronunciation(false);
    }
}

NoteView *PianoRollGraphicsViewPrivate::findAdjacentNoteView(NoteView *currentNoteView,
                                                             const bool backwards) const {
    if (!currentNoteView)
        return nullptr;
    const auto orderedViews = m_selectionModel->orderedNoteItems();
    const auto index = orderedViews.indexOf(currentNoteView);
    const auto targetIndex = backwards ? index - 1 : index + 1;
    return targetIndex >= 0 && targetIndex < orderedViews.size() ? orderedViews.at(targetIndex)
                                                                 : nullptr;
}

void PianoRollGraphicsViewPrivate::onStartEditingPronunciation(PronunciationView *pronView) {
    Q_Q(PianoRollGraphicsView);
    if (!pronView || pronView->id() < 0 || !m_clip)
        return;
    if (m_inlineEditField == InlineEditField::Pronunciation &&
        m_inlineEditingNoteId == pronView->id() && m_inlineEditor->isEditing())
        return;

    hideLyricToolTip();
    finishInlineEditing();
    const auto note = m_clip->findNoteById(pronView->id());
    if (!note)
        return;

    m_inlineEditField = InlineEditField::Pronunciation;
    m_inlineEditingNoteId = pronView->id();
    pronView->setEditingPronunciation(true);

    auto anchorRect = q->mapFromScene(pronView->sceneBoundingRect()).boundingRect();
    const auto viewportRect = q->viewport()->rect();
    const int width = qMin(viewportRect.width(), qMax(40, anchorRect.width()));
    const int height = qMin(viewportRect.height(), qMax(20, anchorRect.height()));
    anchorRect.setSize({width, height});
    anchorRect.moveLeft(qBound(0, anchorRect.left(), viewportRect.width() - width));
    anchorRect.moveTop(qBound(0, anchorRect.top(), viewportRect.height() - height));

    QFont font;
    if (const auto noteView = findNoteViewById(pronView->id()))
        font.setPixelSize(noteView->fontPixelSize);
    const auto pronunciation = note->pronunciation();
    const auto displayText =
        pronunciation.isEdited() ? pronunciation.edited : pronunciation.original;
    const QVariantMap properties = {
        {QStringLiteral("editRole"), QStringLiteral("Pronunciation")},
    };
    m_inlineEditor->showAt(anchorRect, displayText, font, properties);
}

void PianoRollGraphicsViewPrivate::applyPronunciationEdit(const int noteId, const QString &text) {
    if (!m_clip)
        return;
    const auto note = m_clip->findNoteById(noteId);
    if (!note)
        return;
    const auto pronunciation = note->pronunciation();
    const auto displayText =
        pronunciation.isEdited() ? pronunciation.edited : pronunciation.original;
    const auto editedText = text.trimmed();
    if (editedText == displayText || editedText == pronunciation.edited)
        return;

    clipController->onNotePronunciationEdited(noteId, editedText);
}

void PianoRollGraphicsViewPrivate::moveToNullClipState() {
    Q_Q(PianoRollGraphicsView);
    m_selectionModel->clearSelectionAnchor();
    finishInlineEditing();
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
    m_initialViewportPositionPending = false;
}

void PianoRollGraphicsViewPrivate::moveToSingingClipState(SingingClip *clip) {
    Q_Q(PianoRollGraphicsView);
    m_selectionModel->clearSelectionAnchor();
    finishInlineEditing();
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

    for (const auto note : clip->notes())
        handleNoteInserted(note);
    positionViewportAtClipContent();
    m_initialViewportPositionPending = !q->isVisible();

    updatePitch(Param::Original, *m_clip->params.getParamByName(ParamInfo::Pitch));
    updatePitch(Param::Edited, *m_clip->params.getParamByName(ParamInfo::Pitch));

    connect(clip, &SingingClip::propertyChanged, this,
            &PianoRollGraphicsViewPrivate::onClipPropertyChanged);
    connect(clip, &SingingClip::noteChanged, this, &PianoRollGraphicsViewPrivate::onNoteChanged);
    connect(clip, &SingingClip::paramChanged, this, &PianoRollGraphicsViewPrivate::onParamChanged);
    m_selectionModel->setSelectionChangeBarrier(false);
}

void PianoRollGraphicsViewPrivate::positionViewportAtClipContent() {
    Q_Q(PianoRollGraphicsView);
    if (!m_clip)
        return;

    if (m_clip->notes().count() > 0) {
        const auto firstNote = *m_clip->notes().begin();
        auto tickRange = q->endTick() - q->startTick();
        auto targetStart = firstNote->globalStart() - tickRange * 0.3;
        q->setViewportStartTick(targetStart);
        q->setViewportCenterAtKeyIndex(firstNote->keyIndex());
    } else
        q->setViewportStartTick(m_clip->start());
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
    m_selectionModel->invalidateSelectionAnchor(note->id());
    if (m_inlineEditingNoteId == note->id())
        finishInlineEditing();
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
    hideLyricToolTip();
    emit q->keyHoverCleared();
}

void PianoRollGraphicsViewPrivate::onHoverMove(const QHoverEvent *event) {
    Q_Q(PianoRollGraphicsView);
    if (m_interactionController->isMouseDown()) {
        hideLyricToolTip();
        return;
    }

    updateLyricToolTip(event->position().toPoint());
    if (m_isEditPitchMode)
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

    if (m_editMode == EraseNote) {
        q->setCursor(Qt::ArrowCursor);
        return;
    }

    const auto noteView = noteViewAt(event->position().toPoint());
    if (!noteView || noteView->id() < 0) {
        q->setCursor(Qt::ArrowCursor);
        return;
    }

    const auto rPos = noteView->mapFromScene(scenePos);
    const auto rx = rPos.x();
    const auto edge = EditorResizeUtils::horizontalEdgeAt(
        rx, noteView->rect().width(), AppGlobal::resizeTolerance);
    q->setCursor(edge == EditorResizeUtils::HorizontalEdge::None ? Qt::ArrowCursor
                                                                : Qt::SizeHorCursor);
}

void PianoRollGraphicsViewPrivate::updateLyricToolTip(const QPoint &position) {
    Q_Q(PianoRollGraphicsView);
    auto *noteView = noteViewAt(position);
    if (!noteView || noteView->id() < 0 || !noteView->isLyricElided(q->visibleRect()) ||
        (m_inlineEditor && m_inlineEditor->isEditing())) {
        hideLyricToolTip();
        return;
    }

    const auto lyric = noteView->lyric();
    if (m_lyricToolTipNoteId == noteView->id() && m_lyricToolTipText == lyric &&
        m_lyricToolTip->isVisible()) {
        return;
    }

    m_lyricToolTipNoteId = noteView->id();
    m_lyricToolTipText = lyric;
    m_lyricToolTip->setTitle(Qt::convertFromPlainText(lyric));
    const auto noteRect = q->mapFromScene(noteView->sceneBoundingRect())
                              .boundingRect()
                              .intersected(q->viewport()->rect());
    m_lyricToolTip->showAbove({q->viewport()->mapToGlobal(noteRect.topLeft()), noteRect.size()});
}

void PianoRollGraphicsViewPrivate::hideLyricToolTip() {
    m_lyricToolTipNoteId = -1;
    m_lyricToolTipText.clear();
    if (m_lyricToolTip && m_lyricToolTip->isVisible())
        m_lyricToolTip->hideWithAnimation();
}

void PianoRollGraphicsViewPrivate::onClipPropertyChanged() {
    Q_Q(PianoRollGraphicsView);
    hideLyricToolTip();
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
