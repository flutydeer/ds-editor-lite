//
// Created by fluty on 2024/1/23.
//

#define CLASS_NAME "PianoRollGraphicsView"

#include "PianoRollGraphicsView.h"

#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsView_p.h"
#include "Controller/ClipEditorViewController.h"
#include "Controller/PlaybackController.h"
#include "Global/AppGlobal.h"
#include "NoteView.h"
#include "PianoRollBackground.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Curve.h"
#include "Model/AppModel/DrawCurve.h"
#include "Model/AppModel/Note.h"
#include "Model/AppOptions/AppOptions.h"
#include "UI/Dialogs/Note/NotePropertyDialog.h"
#include "PitchEditorView.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Views/Common/ScrollBarGraphicsItem.h"
#include "Utils/Log.h"
#include "Utils/MathUtils.h"

#include <QMouseEvent>
#include <QScrollBar>
#include <QMWidgets/cmenu.h>

PianoRollGraphicsView::PianoRollGraphicsView(PianoRollGraphicsScene *scene, QWidget *parent)
    : TimeGraphicsView(scene, parent), d_ptr(new PianoRollGraphicsViewPrivate(this)) {
    Q_D(PianoRollGraphicsView);
    d->m_layerManager = new GraphicsLayerManager(scene);
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("PianoRollGraphicsView");
    setScaleXMax(5);
    setPixelsPerQuarterNote(pixelsPerQuarterNote);
    setSceneVisibility(false);
    setDragBehavior(DragBehavior::RectSelect);
    setMinimumHeight(0);
    // QScroller::grabGesture(this, QScroller::TouchGesture);

    d->m_currentDrawingNote = new NoteView(-1);
    d->m_currentDrawingNote->setPronunciation("", false);
    d->m_currentDrawingNote->setSelected(true);

    auto gridItem = new PianoRollBackground;
    gridItem->setPixelsPerQuarterNote(pixelsPerQuarterNote);
    setGridItem(gridItem);

    d->m_pitchEditor = new PitchEditorView;
    d->m_pitchEditor->setZValue(2);
    connect(d->m_pitchEditor, &CommonParamEditorView::editCompleted, this,
            &PianoRollGraphicsView::onPitchEditorEditCompleted);
    scene->addCommonItem(d->m_pitchEditor);
    d->m_pitchEditor->setTransparentForMouseEvents(true);

    connect(scene, &QGraphicsScene::selectionChanged, this,
            &PianoRollGraphicsView::onSceneSelectionChanged);

    connect(playbackController, &PlaybackController::positionChanged, this,
            &PianoRollGraphicsView::setPlaybackPosition);
    connect(playbackController, &PlaybackController::lastPositionChanged, this,
            &PianoRollGraphicsView::setLastPlaybackPosition);

    connect(this, &CommonGraphicsView::scaleChanged, this,
            &PianoRollGraphicsView::notifyKeyRangeChanged);
    connect(this, &CommonGraphicsView::visibleRectChanged, this,
            &PianoRollGraphicsView::notifyKeyRangeChanged);
}

PianoRollGraphicsView::~PianoRollGraphicsView() {
    delete d_ptr;
}

void PianoRollGraphicsView::setDataContext(SingingClip *clip) {
    Q_D(PianoRollGraphicsView);
    if (clip == nullptr)
        d->moveToNullClipState();
    else
        d->moveToSingingClipState(clip);
}

void PianoRollGraphicsView::onSceneSelectionChanged() const {
    Q_D(const PianoRollGraphicsView);
    if (d->m_canNotifySelectedNoteChanged) {
        auto notes = selectedNotesId();
        clipController->onNoteSelectionChanged(notes, true);
    }
}

void PianoRollGraphicsView::onPitchEditorEditCompleted(const QList<DrawCurve *> &curves) {
    Q_D(PianoRollGraphicsView);
    QList<Curve *> list;
    for (auto curve : curves) {
        list.append(curve);
    }
    clipController->onPitchEdited(list);
}

void PianoRollGraphicsView::notifyKeyRangeChanged() {
    emit keyRangeChanged(topKeyIndex(), bottomKeyIndex());
}

void PianoRollGraphicsView::contextMenuEvent(QContextMenuEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (d->m_editMode == Select || d->m_editMode == DrawNote) {
        if (auto noteView = d->noteViewAt(event->pos())) {
            auto menu = d->buildNoteContextMenu(noteView);
            menu->exec(event->globalPos());
        }
    }

    TimeGraphicsView::contextMenuEvent(event);
}

void PianoRollGraphicsView::mousePressEvent(QMouseEvent *event) {
    Q_D(PianoRollGraphicsView);
    // 在滚动条上按下时，交还给基类处理
    if (dynamic_cast<ScrollBarGraphicsItem *>(itemAt(event->pos()))) {
        CommonGraphicsView::mousePressEvent(event);
        event->ignore();
        return;
    }

    d->m_selecting = true;
    if (event->button() != Qt::LeftButton &&
        (d->m_editMode == Select || d->m_editMode == DrawNote || d->m_editMode == EraseNote)) {
        d->m_mouseMoveBehavior = PianoRollGraphicsViewPrivate::None;
        if (auto noteView = d->noteViewAt(event->pos())) {
            if (d->selectedNoteItems().count() <= 1 || !d->selectedNoteItems().contains(noteView))
                clearNoteSelections();
            noteView->setSelected(true);
        } else {
            clearNoteSelections();
            TimeGraphicsView::mousePressEvent(event);
        }
        // TimeGraphicsView::mousePressEvent(event);
        event->ignore();
        return;
    }

    // event->accept();
    auto scenePos = mapToScene(event->position().toPoint());
    auto tick = static_cast<int>(sceneXToTick(scenePos.x()) + d->m_offset);
    auto keyIndex = d->sceneYToKeyIndexInt(scenePos.y());
    auto noteView = d->noteViewAt(event->pos());
    auto mouseInNoteView =
        noteView && PianoRollGraphicsViewPrivate::mouseInFilledRect(scenePos, noteView);

    if (d->m_editMode == Select) {
        if (mouseInNoteView) {
            d->prepareForEditingNotes(event, scenePos, keyIndex, noteView);
        } else
            TimeGraphicsView::mousePressEvent(event);
    } else if (d->m_editMode == DrawNote) {
        clearNoteSelections();
        if (mouseInNoteView) {
            d->prepareForEditingNotes(event, scenePos, keyIndex, noteView);
        } else
            d->PrepareForDrawingNote(tick, keyIndex);
    } else if (d->m_editMode == EraseNote) {
        d->m_mouseMoveBehavior = PianoRollGraphicsViewPrivate::EraseNotes;
        if (mouseInNoteView) {
            d->eraseNoteFromView(noteView);
        } else
            TimeGraphicsView::mousePressEvent(event);
    } else
        TimeGraphicsView::mousePressEvent(event);
    event->ignore();
}

void PianoRollGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::None) {
        TimeGraphicsView::mouseMoveEvent(event);
        return;
    }

    if (event->modifiers() == Qt::AltModifier)
        d->m_tempQuantizeOff = true;
    else
        d->m_tempQuantizeOff = false;

    auto scenePos = mapToScene(event->position().toPoint());
    auto tick = static_cast<int>(sceneXToTick(scenePos.x()) + d->m_offset);
    auto quantizedTickLength = d->m_tempQuantizeOff ? 1 : 1920 / appStatus->quantize;
    auto snappedTick = MathUtils::roundDown(tick, quantizedTickLength);
    auto keyIndex = d->sceneYToKeyIndexInt(scenePos.y());
    auto deltaX = static_cast<int>(sceneXToTick(scenePos.x() - d->m_mouseDownPos.x()));

    auto noteView = d->noteViewAt(event->pos());
    auto mouseInNoteView =
        noteView && PianoRollGraphicsViewPrivate::mouseInFilledRect(scenePos, noteView);

    // TODO: 优化移动和调整音符
    if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::Move) {
        auto startOffset = MathUtils::round(deltaX, quantizedTickLength);
        auto keyOffset = keyIndex - d->m_mouseDownKeyIndex;
        if (keyOffset > d->m_moveMaxDeltaKey)
            keyOffset = d->m_moveMaxDeltaKey;
        if (keyOffset < d->m_moveMinDeltaKey)
            keyOffset = d->m_moveMinDeltaKey;
        d->m_deltaTick = startOffset;
        d->m_deltaKey = keyOffset;
        d->moveSelectedNotes(d->m_deltaTick, d->m_deltaKey);
        d->m_movedBeforeMouseUp = true;
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::ResizeLeft) {
        auto rStart = snappedTick - d->m_offset;
        auto deltaStart = rStart - d->m_mouseDownRStart;
        auto length = d->m_mouseDownLength - deltaStart;
        if (length < quantizedTickLength)
            deltaStart = d->m_mouseDownLength - quantizedTickLength;
        d->m_deltaTick = deltaStart;
        d->resizeLeftSelectedNote(d->m_deltaTick);
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::ResizeRight) {
        auto lengthOffset = MathUtils::round(deltaX, quantizedTickLength);
        auto right = d->m_mouseDownRStart + d->m_mouseDownLength + lengthOffset;
        auto length = right - d->m_mouseDownRStart;
        auto deltaLength = length - d->m_mouseDownLength;
        if (length < quantizedTickLength)
            deltaLength = -(d->m_mouseDownLength - quantizedTickLength);
        d->m_deltaTick = deltaLength;
        d->resizeRightSelectedNote(d->m_deltaTick);
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::UpdateDrawingNote) {
        auto targetLength = snappedTick - d->m_offset - d->m_currentDrawingNote->rStart();
        if (targetLength >= quantizedTickLength)
            d->m_currentDrawingNote->setLength(targetLength);
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::EraseNotes) {
        if (mouseInNoteView)
            d->eraseNoteFromView(noteView);
        else
            TimeGraphicsView::mouseMoveEvent(event);
    } else
        TimeGraphicsView::mouseMoveEvent(event);
}

void PianoRollGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    Q_D(PianoRollGraphicsView);
    bool ctrlDown = event->modifiers() == Qt::ControlModifier;

    if (scene()->items().contains(d->m_currentDrawingNote)) {
        scene()->removeCommonItem(d->m_currentDrawingNote);
    }
    if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::Move) {
        if (d->m_movedBeforeMouseUp) {
            d->resetSelectedNotesOffset();
            d->handleNotesMoved(d->m_deltaTick, d->m_deltaKey);
        } else if (!ctrlDown) {
            clearNoteSelections(d->m_currentEditingNote);
        }
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::ResizeLeft) {
        d->resetSelectedNotesOffset();
        PianoRollGraphicsViewPrivate::handleNoteLeftResized(d->m_currentEditingNote->id(),
                                                            d->m_deltaTick);
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::ResizeRight) {
        d->resetSelectedNotesOffset();
        PianoRollGraphicsViewPrivate::handleNoteRightResized(d->m_currentEditingNote->id(),
                                                             d->m_deltaTick);
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::UpdateDrawingNote) {
        d->handleNoteDrawn(d->m_currentDrawingNote->rStart(), d->m_currentDrawingNote->length(),
                           d->m_currentDrawingNote->keyIndex());
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::EraseNotes) {
        d->handleNotesErased();
    }
    d->m_mouseMoveBehavior = PianoRollGraphicsViewPrivate::None;
    d->m_deltaTick = 0;
    d->m_deltaKey = 0;
    d->m_movedBeforeMouseUp = false;
    d->m_currentEditingNote = nullptr;

    if (!d->m_cachedSelectedNotes.isEmpty())
        d->updateSelectionState();
    d->m_selecting = false;

    TimeGraphicsView::mouseReleaseEvent(event);
}

void PianoRollGraphicsView::mouseDoubleClickEvent(QMouseEvent *event) {
    // 禁用双击事件，防止在滚动条上双击时触发取消选中音符
    // TimeGraphicsView::mouseDoubleClickEvent(event);
}

void PianoRollGraphicsView::reset() {
    Q_D(PianoRollGraphicsView);
    d->m_layerManager->destroyItems(&d->m_noteLayer);
}

QList<int> PianoRollGraphicsView::selectedNotesId() const {
    Q_D(const PianoRollGraphicsView);
    QList<int> list;
    for (const auto noteItem : d->m_noteLayer.noteItems()) {
        if (noteItem->isSelected())
            list.append(noteItem->id());
    }
    return list;
}

void PianoRollGraphicsView::clearNoteSelections(NoteView *except) {
    Q_D(PianoRollGraphicsView);
    for (const auto noteItem : d->m_noteLayer.noteItems()) {
        if (noteItem != except && noteItem->isSelected())
            noteItem->setSelected(false);
    }
}

double PianoRollGraphicsView::topKeyIndex() const {
    Q_D(const PianoRollGraphicsView);
    return d->sceneYToKeyIndexDouble(visibleRect().top());
}

double PianoRollGraphicsView::bottomKeyIndex() const {
    Q_D(const PianoRollGraphicsView);
    return d->sceneYToKeyIndexDouble(visibleRect().bottom());
}

void PianoRollGraphicsView::setViewportCenterAt(double tick, double keyIndex) {
    setViewportCenterAtTick(tick);
    setViewportCenterAtKeyIndex(keyIndex);
}

void PianoRollGraphicsView::setViewportCenterAtKeyIndex(double keyIndex) {
    Q_D(PianoRollGraphicsView);
    auto keyIndexRange = topKeyIndex() - bottomKeyIndex();
    auto keyIndexStart = keyIndex + keyIndexRange / 2 + 0.5;
    auto vBarValue = qRound(d->keyIndexToSceneY(keyIndexStart));
    // verticalScrollBar()->setValue(vBarValue);
    vBarAnimateTo(vBarValue);
}

void PianoRollGraphicsView::setEditMode(PianoRollEditMode mode) {
    Q_D(PianoRollGraphicsView);
    d->m_editMode = mode;
    if (mode == Select) {
        setDragBehavior(DragBehavior::RectSelect);
        d->setPitchEditMode(false, false);
    } else if (mode == IntervalSelect) {
        setDragBehavior(DragBehavior::IntervalSelect);
        d->setPitchEditMode(false, false);
    } else if (mode == DrawNote || mode == EraseNote) {
        setDragBehavior(DragBehavior::None);
        d->setPitchEditMode(false, false);
    } else if (mode == DrawPitch || mode == EditPitchAnchor) {
        setDragBehavior(DragBehavior::None);
        d->setPitchEditMode(true, false);
    } else if (mode == ErasePitch || mode == FreezePitch) { // TODO: 实现冻结自动音高
        setDragBehavior(DragBehavior::None);
        d->setPitchEditMode(true, true);
    }
}

void PianoRollGraphicsViewPrivate::onNoteChanged(SingingClip::NoteChangeType type, Note *note) {
    if (type == SingingClip::Inserted)
        handleNoteInserted(note);
    else if (type == SingingClip::Removed)
        handleNoteRemoved(note);

    updateOverlappedState();
}

void PianoRollGraphicsViewPrivate::onNoteSelectionChanged() {
    auto selectedNotes = m_clip->selectedNotes();
    m_cachedSelectedNotes = selectedNotes;
    if (!m_selecting)
        updateSelectionState();
}

void PianoRollGraphicsViewPrivate::onParamChanged(ParamBundle::ParamName name,
                                                  Param::ParamType type) const {
    if (name == ParamBundle::Pitch) {
        auto pitchParam = m_clip->params.getParamByName(name);
        updatePitch(type, *pitchParam);
    }
}

void PianoRollGraphicsViewPrivate::onDeleteSelectedNotes() const {
    Q_Q(const PianoRollGraphicsView);
    auto notes = q->selectedNotesId();
    clipController->onRemoveNotes(notes);
}

void PianoRollGraphicsViewPrivate::onOpenNotePropertyDialog(int noteId) {
    Q_Q(PianoRollGraphicsView);
    auto note = m_clip->findNoteById(noteId);
    auto dlg = new NotePropertyDialog(note, q);
    connect(dlg, &NotePropertyDialog::accepted, this,
            [=] { clipController->onNotePropertiesEdited(noteId, dlg->result()); });
    dlg->show();
}

CMenu *PianoRollGraphicsViewPrivate::buildNoteContextMenu(NoteView *noteView) {
    Q_Q(PianoRollGraphicsView);
    auto menu = new CMenu(q);

    auto actionEditLyric = menu->addAction(tr("Fill lyrics..."));
    connect(actionEditLyric, &QAction::triggered, clipController,
            [=] { clipController->onFillLyric(q); });

    menu->addSeparator();

    auto actionRemove = menu->addAction(tr("Delete"));
    connect(actionRemove, &QAction::triggered, this,
            &PianoRollGraphicsViewPrivate::onDeleteSelectedNotes);

    menu->addSeparator();

    auto actionProperties = menu->addAction(tr("Properties..."));
    connect(actionProperties, &QAction::triggered, this,
            [=] { onOpenNotePropertyDialog(noteView->id()); });
    return menu;
}

void PianoRollGraphicsViewPrivate::moveToNullClipState() {
    Q_Q(PianoRollGraphicsView);
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

    if (clip->notes().count() > 0) {
        for (const auto note : clip->notes())
            handleNoteInserted(note);
        auto firstNote = *clip->notes().begin();
        q->setViewportCenterAt(firstNote->start(), firstNote->keyIndex());
    } else
        q->setViewportCenterAtKeyIndex(60);

    updatePitch(Param::Original, *m_clip->params.getParamByName(ParamBundle::Pitch));
    updatePitch(Param::Edited, *m_clip->params.getParamByName(ParamBundle::Pitch));

    connect(clip, &SingingClip::propertyChanged, this,
            &PianoRollGraphicsViewPrivate::onClipPropertyChanged);
    connect(clip, &SingingClip::noteChanged, this, &PianoRollGraphicsViewPrivate::onNoteChanged);
    connect(clip, &SingingClip::noteSelectionChanged, this,
            &PianoRollGraphicsViewPrivate::onNoteSelectionChanged);
    connect(clip, &SingingClip::paramChanged, this, &PianoRollGraphicsViewPrivate::onParamChanged);
}

void PianoRollGraphicsViewPrivate::prepareForEditingNotes(QMouseEvent *event, QPointF scenePos,
                                                          int keyIndex, NoteView *noteItem) {
    Q_Q(PianoRollGraphicsView);
    bool ctrlDown = event->modifiers() == Qt::ControlModifier;
    if (!ctrlDown) {
        if (selectedNoteItems().count() <= 1 || !selectedNoteItems().contains(noteItem))
            q->clearNoteSelections();
        noteItem->setSelected(true);
    } else {
        noteItem->setSelected(!noteItem->isSelected());
    }
    auto rPos = noteItem->mapFromScene(scenePos);
    auto rx = rPos.x();
    if (rx >= 0 && rx <= AppGlobal::resizeTolarance) {
        // setCursor(Qt::SizeHorCursor);
        m_mouseMoveBehavior = ResizeLeft;
        q->clearNoteSelections();
        noteItem->setSelected(true);
    } else if (rx >= noteItem->rect().width() - AppGlobal::resizeTolarance &&
               rx <= noteItem->rect().width()) {
        // setCursor(Qt::SizeHorCursor);
        m_mouseMoveBehavior = ResizeRight;
        q->clearNoteSelections();
        noteItem->setSelected(true);
    } else {
        // setCursor(Qt::ArrowCursor);
        m_mouseMoveBehavior = Move;
    }

    m_currentEditingNote = noteItem;
    m_mouseDownPos = scenePos;
    m_mouseDownRStart = m_currentEditingNote->rStart();
    m_mouseDownLength = m_currentEditingNote->length();
    m_mouseDownKeyIndex = keyIndex;
    updateMoveDeltaKeyRange();
}

void PianoRollGraphicsViewPrivate::PrepareForDrawingNote(int tick, int keyIndex) {
    Q_Q(PianoRollGraphicsView);
    auto snappedTick = MathUtils::roundDown(tick, 1920 / appStatus->quantize);
    Log::d(CLASS_NAME, "Draw note at: " + qStrNum(snappedTick));
    m_currentDrawingNote->setLyric(appOptions->general()->defaultLyric);
    m_currentDrawingNote->setRStart(snappedTick - m_offset);
    m_currentDrawingNote->setLength(1920 / appStatus->quantize);
    m_currentDrawingNote->setKeyIndex(keyIndex);
    q->scene()->addCommonItem(m_currentDrawingNote);
    m_mouseMoveBehavior = UpdateDrawingNote;
}

void PianoRollGraphicsViewPrivate::handleNoteDrawn(int rStart, int length, int keyIndex) const {
    Log::d(CLASS_NAME, QString("Note drawn rStart:%1 len:%2 key:%3")
                           .arg(qStrNum(rStart), qStrNum(length), qStrNum(keyIndex)));
    auto note = new Note;
    note->setRStart(rStart);
    note->setLength(length);
    note->setKeyIndex(keyIndex);
    note->setLanguage(languageKeyFromType(m_clip->defaultLanguage));
    note->setLyric(appOptions->general()->defaultLyric);
    note->setPronunciation(Pronunciation("", ""));
    note->setSelected(true);
    clipController->onInsertNote(note);
}

void PianoRollGraphicsViewPrivate::handleNotesMoved(int deltaTick, int deltaKey) const {
    Q_Q(const PianoRollGraphicsView);
    Log::d(CLASS_NAME, QString("Notes moved dt:%1 dk:%2 ").arg(deltaTick).arg(deltaKey));
    clipController->onMoveNotes(q->selectedNotesId(), deltaTick, deltaKey);
}

void PianoRollGraphicsViewPrivate::handleNoteLeftResized(int noteId, int deltaTick) {
    Log::d(CLASS_NAME,
           QString("Note left resized id:%1 dt:%2").arg(qStrNum(noteId), qStrNum(deltaTick)));
    QList<int> notes;
    notes.append(noteId);
    clipController->onResizeNotesLeft(notes, deltaTick);
}

void PianoRollGraphicsViewPrivate::handleNoteRightResized(int noteId, int deltaTick) {
    Log::d(CLASS_NAME,
           QString("Note right resized id:%1 dt:%2").arg(qStrNum(noteId), qStrNum(deltaTick)));
    QList<int> notes;
    notes.append(noteId);
    clipController->onResizeNotesRight(notes, deltaTick);
}

void PianoRollGraphicsViewPrivate::handleNotesErased() {
    qDebug() << "Note erased count:" << m_notesToErase.count();
    clipController->onRemoveNotes(m_notesToErase);
    m_notesToErase.clear();
}

void PianoRollGraphicsViewPrivate::eraseNoteFromView(NoteView *noteView) {
    m_notesToErase.append(noteView->id());
    m_canNotifySelectedNoteChanged = false;
    m_layerManager->removeItem(noteView, &m_noteLayer);
    delete noteView;
    m_canNotifySelectedNoteChanged = true;
}

void PianoRollGraphicsViewPrivate::updateSelectionState() {
    Q_Q(PianoRollGraphicsView);
    m_canNotifySelectedNoteChanged = false;
    q->clearNoteSelections();

    // TODO: 修复状态不一致的 bug
    for (const auto note : m_cachedSelectedNotes) {
        if (auto noteItem = m_noteLayer.findNoteById(note->id()))
            noteItem->setSelected(note->selected());
    }
    m_cachedSelectedNotes.clear();
    m_canNotifySelectedNoteChanged = true;
}

void PianoRollGraphicsViewPrivate::updateOverlappedState() {
    Q_Q(PianoRollGraphicsView);
    for (const auto note : m_notes) {
        if (auto noteItem = m_noteLayer.findNoteById(note->id()))
            noteItem->setOverlapped(note->overlapped());
    }
    q->update();
}

void PianoRollGraphicsViewPrivate::updateNoteTimeAndKey(Note *note) const {
    auto noteItem = m_noteLayer.findNoteById(note->id());
    noteItem->setRStart(note->rStart());
    noteItem->setLength(note->length());
    noteItem->setKeyIndex(note->keyIndex());
}

void PianoRollGraphicsViewPrivate::updateNoteWord(Note *note) const {
    auto noteItem = m_noteLayer.findNoteById(note->id());
    noteItem->setLyric(note->lyric());
    auto original = note->pronunciation().original;
    auto edited = note->pronunciation().edited;
    auto isEdited = note->pronunciation().isEdited();
    noteItem->setPronunciation(isEdited ? edited : original, isEdited);
}

double PianoRollGraphicsViewPrivate::keyIndexToSceneY(double index) const {
    Q_Q(const PianoRollGraphicsView);
    return (127 - index) * q->scaleY() * noteHeight;
}

double PianoRollGraphicsViewPrivate::sceneYToKeyIndexDouble(double y) const {
    Q_Q(const PianoRollGraphicsView);
    return 127 - y / q->scaleY() / noteHeight;
}

int PianoRollGraphicsViewPrivate::sceneYToKeyIndexInt(double y) const {
    auto keyIndexD = sceneYToKeyIndexDouble(y);
    auto keyIndex = static_cast<int>(keyIndexD) + 1;
    if (keyIndex > 127)
        keyIndex = 127;
    return keyIndex;
}

void PianoRollGraphicsViewPrivate::moveSelectedNotes(int startOffset, int keyOffset) const {
    auto notes = selectedNoteItems();
    for (auto note : notes) {
        note->setStartOffset(startOffset);
        note->setKeyOffset(keyOffset);
    }
}

void PianoRollGraphicsViewPrivate::resetSelectedNotesOffset() const {
    auto notes = selectedNoteItems();
    for (auto note : notes)
        note->resetOffset();
}

void PianoRollGraphicsViewPrivate::updateMoveDeltaKeyRange() {
    auto selectedNotes = selectedNoteItems();
    int highestKey = 0;
    int lowestKey = 127;
    for (const auto note : selectedNotes) {
        auto key = note->keyIndex();
        if (key > highestKey)
            highestKey = key;
        if (key < lowestKey)
            lowestKey = key;
    }
    m_moveMaxDeltaKey = 127 - highestKey;
    m_moveMinDeltaKey = -lowestKey;
}

void PianoRollGraphicsViewPrivate::resetMoveDeltaKeyRange() {
    m_moveMaxDeltaKey = 127;
    m_moveMinDeltaKey = 0;
}

void PianoRollGraphicsViewPrivate::resizeLeftSelectedNote(int offset) const {
    // TODO: resize all selected notes
    m_currentEditingNote->setStartOffset(offset);
    m_currentEditingNote->setLengthOffset(-offset);
}

void PianoRollGraphicsViewPrivate::resizeRightSelectedNote(int offset) const {
    m_currentEditingNote->setLengthOffset(offset);
}

QList<NoteView *> PianoRollGraphicsViewPrivate::selectedNoteItems() const {
    QList<NoteView *> list;
    for (const auto noteItem : m_noteLayer.noteItems()) {
        if (noteItem->isSelected())
            list.append(noteItem);
    }
    return list;
}

void PianoRollGraphicsViewPrivate::setPitchEditMode(bool on, bool isErase) {
    Q_Q(PianoRollGraphicsView);
    for (auto note : m_noteLayer.noteItems())
        note->setEditingPitch(on);
    if (on)
        q->clearNoteSelections();
    m_pitchEditor->setTransparentForMouseEvents(!on);
    m_pitchEditor->setEraseMode(isErase);
}

NoteView *PianoRollGraphicsViewPrivate::noteViewAt(const QPoint &pos) {
    Q_Q(PianoRollGraphicsView);
    for (const auto item : q->items(pos))
        if (auto noteItem = dynamic_cast<NoteView *>(item))
            return noteItem;
    return nullptr;
}

bool PianoRollGraphicsViewPrivate::mouseInFilledRect(QPointF scenePos, NoteView *view) {
    if (!view) {
        qCritical() << "Note view is null";
        return false;
    }

    auto rPos = view->mapFromScene(scenePos);
    auto ry = rPos.y();
    return ry < view->rect().height() - view->pronunciationTextHeight();
}

void PianoRollGraphicsViewPrivate::handleNoteInserted(Note *note) {
    m_canNotifySelectedNoteChanged = false;
    auto noteItem = new NoteView(note->id());
    noteItem->setRStart(note->rStart());
    noteItem->setLength(note->length());
    noteItem->setKeyIndex(note->keyIndex());
    noteItem->setLyric(note->lyric());
    auto original = note->pronunciation().original;
    auto edited = note->pronunciation().edited;
    auto isEdited = note->pronunciation().isEdited();
    noteItem->setPronunciation(isEdited ? edited : original, isEdited);
    noteItem->setSelected(note->selected());
    noteItem->setOverlapped(note->overlapped());
    m_layerManager->addItem(noteItem, &m_noteLayer);
    m_canNotifySelectedNoteChanged = true;

    m_notes.append(note);
    connect(note, &Note::propertyChanged, this,
            [=](Note::NotePropertyType type) { handleNotePropertyChanged(type, note); });
}

void PianoRollGraphicsViewPrivate::handleNoteRemoved(Note *note) {
    // qDebug() << "PianoRollGraphicsView::removeNote" << note->id() << note->lyric();
    m_canNotifySelectedNoteChanged = false;
    if (auto noteItem = m_noteLayer.findNoteById(note->id())) {
        m_layerManager->removeItem(noteItem, &m_noteLayer);
        delete noteItem;
    }
    m_canNotifySelectedNoteChanged = true;
    m_notes.removeOne(note);
    disconnect(note, nullptr, this, nullptr);
}

void PianoRollGraphicsViewPrivate::handleNotePropertyChanged(Note::NotePropertyType type,
                                                             Note *note) {
    if (type == Note::TimeAndKey) {
        updateNoteTimeAndKey(note);
        updateOverlappedState();
    } else if (type == Note::Word)
        updateNoteWord(note);
}

void PianoRollGraphicsViewPrivate::onClipPropertyChanged() {
    Q_Q(PianoRollGraphicsView);
    m_offset = m_clip->start();
    q->setOffset(m_offset);

    for (auto note : m_notes) {
        updateNoteTimeAndKey(note);
    }
}

void PianoRollGraphicsViewPrivate::updatePitch(Param::ParamType paramType,
                                               const Param &param) const {
    QList<DrawCurve *> drawCurves;
    if (paramType == Param::Original) {
        Log::d(CLASS_NAME, "Update original pitch ");
        for (const auto curve : param.curves(Param::Original))
            if (curve->type() == Curve::Draw) {
                MathUtils::binaryInsert(drawCurves, reinterpret_cast<DrawCurve *>(curve));
            }
        m_pitchEditor->loadOriginal(drawCurves);
    } else {
        Log::d(CLASS_NAME, "Update edited pitch ");
        for (const auto curve : param.curves(Param::Edited))
            if (curve->type() == Curve::Draw)
                MathUtils::binaryInsert(drawCurves, reinterpret_cast<DrawCurve *>(curve));
        m_pitchEditor->loadEdited(drawCurves);
    }
}