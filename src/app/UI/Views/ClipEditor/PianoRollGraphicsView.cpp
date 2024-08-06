//
// Created by fluty on 2024/1/23.
//

#include "PianoRollGraphicsView.h"

#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsView_p.h"
#include "Controller/ClipEditorViewController.h"
#include "Controller/PlaybackController.h"
#include "Global/AppGlobal.h"
#include "GraphicsItem/NoteView.h"
#include "GraphicsItem/PianoRollBackgroundGraphicsItem.h"
#include "GraphicsItem/PitchEditorGraphicsItem.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Curve.h"
#include "Model/AppModel/Note.h"
#include "Model/AppOptions/AppOptions.h"
#include "UI/Dialogs/Note/NotePropertyDialog.h"
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
    // QScroller::grabGesture(this, QScroller::TouchGesture);

    d->m_currentDrawingNote = new NoteView(-1);
    d->m_currentDrawingNote->setPronunciation("", false);
    d->m_currentDrawingNote->setSelected(true);

    auto gridItem = new PianoRollBackgroundGraphicsItem;
    gridItem->setPixelsPerQuarterNote(pixelsPerQuarterNote);
    setGridItem(gridItem);

    d->m_pitchItem = new PitchEditorGraphicsItem;
    d->m_pitchItem->setZValue(2);
    connect(d->m_pitchItem, &PitchEditorGraphicsItem::editCompleted, this,
            &PianoRollGraphicsView::onPitchEditorEditCompleted);
    scene->addCommonItem(d->m_pitchItem);
    d->m_pitchItem->setTransparentForMouseEvents(true);

    connect(scene, &QGraphicsScene::selectionChanged, this,
            &PianoRollGraphicsView::onSceneSelectionChanged);

    connect(playbackController, &PlaybackController::positionChanged, this,
            &PianoRollGraphicsView::setPlaybackPosition);
    connect(playbackController, &PlaybackController::lastPositionChanged, this,
            &PianoRollGraphicsView::setLastPlaybackPosition);

    connect(this, &CommonGraphicsView::scaleChanged, this,
            [=] { emit keyIndexRangeChanged(topKeyIndex(), bottomKeyIndex()); });
    connect(this, &CommonGraphicsView::visibleRectChanged, this,
            [=] { emit keyIndexRangeChanged(topKeyIndex(), bottomKeyIndex()); });
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
void PianoRollGraphicsView::onEditModeChanged(PianoRollEditMode mode) {
    Q_D(PianoRollGraphicsView);
    d->m_editMode = mode;
    setEditMode(mode);
}
void PianoRollGraphicsView::onSceneSelectionChanged() const {
    Q_D(const PianoRollGraphicsView);
    if (d->m_canNotifySelectedNoteChanged) {
        auto notes = selectedNotesId();
        clipController->onNoteSelectionChanged(notes, true);
    }
}
void PianoRollGraphicsView::onPitchEditorEditCompleted() {
    Q_D(PianoRollGraphicsView);
    qDebug() << "PianoRollGraphicsView::onPitchEditorEditCompleted";
    OverlappableSerialList<Curve> curves;
    auto newCurves = d->m_pitchItem->editedCurves();
    for (auto curve : newCurves) {
        curves.add(curve);
        qDebug() << "curve:"
                 << "#" << curve->id() << curve->start() << curve->endTick();
    }
    qDebug() << "curve count" << curves.count();
    // TODO: Add anchor curves
    clipController->onPitchEdited(curves);
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
void PianoRollGraphicsView::paintEvent(QPaintEvent *event) {
    Q_D(PianoRollGraphicsView);
    CommonGraphicsView::paintEvent(event);

    if (d->m_clip)
        return;

    QPainter painter(viewport());
    painter.setPen(QColor(160, 160, 160));
    painter.drawText(viewport()->rect(), tr("Select a singing clip to edit"),
                     QTextOption(Qt::AlignCenter));
}
void PianoRollGraphicsView::mousePressEvent(QMouseEvent *event) {
    Q_D(PianoRollGraphicsView);
    d->m_selecting = true;
    if (event->button() != Qt::LeftButton) {
        d->m_mouseMoveBehavior = PianoRollGraphicsViewPrivate::None;
        if (auto noteView = d->noteViewAt(event->pos())) {
            if (d->selectedNoteItems().count() <= 1 || !d->selectedNoteItems().contains(noteView))
                clearNoteSelections();
            noteView->setSelected(true);
        } else {
            clearNoteSelections();
            TimeGraphicsView::mousePressEvent(event);
        }
        event->ignore();
        return;
    }

    // event->accept();
    auto scenePos = mapToScene(event->position().toPoint());
    auto tick = static_cast<int>(sceneXToTick(scenePos.x()) + d->m_offset);
    auto keyIndex = d->sceneYToKeyIndexInt(scenePos.y());

    if (d->m_editMode == Select) {
        if (auto noteView = d->noteViewAt(event->pos())) {
            auto rPos = noteView->mapFromScene(scenePos);
            auto ry = rPos.y();
            auto mouseInFilledRect =
                ry < noteView->rect().height() - noteView->pronunciationTextHeight();
            if (mouseInFilledRect)
                d->prepareForEditingNotes(event, scenePos, keyIndex, noteView);
            else
                TimeGraphicsView::mousePressEvent(event);
        } else
            TimeGraphicsView::mousePressEvent(event);
    } else if (d->m_editMode == DrawNote) {
        clearNoteSelections();
        if (auto noteView = d->noteViewAt(event->pos())) {
            qDebug() << "DrawNote mode, move or resize note";
            auto rPos = noteView->mapFromScene(scenePos);
            auto ry = rPos.y();
            auto mouseInFilledRect =
                ry < noteView->rect().height() - noteView->pronunciationTextHeight();
            if (mouseInFilledRect)
                d->prepareForEditingNotes(event, scenePos, keyIndex, noteView);
            else
                d->PrepareForDrawingNote(tick, keyIndex);
        } else {
            d->PrepareForDrawingNote(tick, keyIndex);
        }
    } else
        TimeGraphicsView::mousePressEvent(event);
    event->ignore();
}
void PianoRollGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (event->modifiers() == Qt::AltModifier)
        d->m_tempQuantizeOff = true;
    else
        d->m_tempQuantizeOff = false;

    auto scenePos = mapToScene(event->position().toPoint());
    auto tick = static_cast<int>(sceneXToTick(scenePos.x()) + d->m_offset);
    auto quantizedTickLength = d->m_tempQuantizeOff ? 1 : 1920 / appModel->quantize();
    auto snappedTick = MathUtils::roundDown(tick, quantizedTickLength);
    auto keyIndex = d->sceneYToKeyIndexInt(scenePos.y());
    auto deltaX = static_cast<int>(sceneXToTick(scenePos.x() - d->m_mouseDownPos.x()));

    // TODO: 优化移动和调整音符
    if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::Move) {
        auto deltaX = static_cast<int>(sceneXToTick(scenePos.x() - d->m_mouseDownPos.x()));
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
        // qDebug() << "deltaLength" << deltaLength;
        d->m_deltaTick = deltaLength;
        d->resizeRightSelectedNote(d->m_deltaTick);
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::UpdateDrawingNote) {
        auto targetLength = snappedTick - d->m_offset - d->m_currentDrawingNote->rStart();
        if (targetLength >= quantizedTickLength)
            d->m_currentDrawingNote->setLength(targetLength);
    } else
        TimeGraphicsView::mouseMoveEvent(event);
}
void PianoRollGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    Q_D(PianoRollGraphicsView);
    bool ctrlDown = event->modifiers() == Qt::ControlModifier;

    if (scene()->items().contains(d->m_currentDrawingNote)) {
        scene()->removeCommonItem(d->m_currentDrawingNote);
        qDebug() << "fake note removed from scene";
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
        d->handleNoteDrew(d->m_currentDrawingNote->rStart(), d->m_currentDrawingNote->length(),
                          d->m_currentDrawingNote->keyIndex());
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
    qDebug() << "keyIndexStart" << keyIndexStart;
    auto vBarValue = qRound(d->keyIndexToSceneY(keyIndexStart));
    // verticalScrollBar()->setValue(vBarValue);
    vBarAnimateTo(vBarValue);
}
void PianoRollGraphicsView::setEditMode(PianoRollEditMode mode) {
    Q_D(PianoRollGraphicsView);
    d->m_editMode = mode;
    switch (d->m_editMode) {
        case Select:
            setDragMode(RubberBandDrag);
            d->setPitchEditMode(false);
            break;
        case DrawNote:
            setDragMode(NoDrag);
            d->setPitchEditMode(false);
            break;
        case DrawPitch:
        case EditPitchAnchor:
            setDragMode(NoDrag);
            d->setPitchEditMode(true);
            break;
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
    qDebug() << "PianoRollGraphicsView::onDeleteSelectedNotes";
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
    // TODO: 移动到控制器？
    connect(actionProperties, &QAction::triggered, this,
            [=] { onOpenNotePropertyDialog(noteView->id()); });
    return menu;
}
void PianoRollGraphicsViewPrivate::moveToNullClipState() {
    Q_Q(PianoRollGraphicsView);
    q->setSceneVisibility(false);
    q->setEnabled(false);
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
    auto snappedTick = MathUtils::roundDown(tick, 1920 / appModel->quantize());
    qDebug() << "Draw note at" << snappedTick;
    m_currentDrawingNote->setLyric(appOptions->general()->defaultLyric);
    m_currentDrawingNote->setRStart(snappedTick - m_offset);
    m_currentDrawingNote->setLength(1920 / appModel->quantize());
    m_currentDrawingNote->setKeyIndex(keyIndex);
    q->scene()->addCommonItem(m_currentDrawingNote);
    qDebug() << "fake note added to scene";
    m_mouseMoveBehavior = UpdateDrawingNote;
}
void PianoRollGraphicsViewPrivate::handleNoteDrew(int rStart, int length, int keyIndex) const {
    qDebug() << "PianoRollGraphicsView::handleDrawNoteCompleted" << rStart << length << keyIndex;
    auto note = new Note;
    note->setRStart(rStart);
    note->setLength(length);
    note->setKeyIndex(keyIndex);
    note->setLanguage(languageKeyFromType(m_clip->defaultLanguage()));
    note->setLyric(appOptions->general()->defaultLyric);
    note->setPronunciation(Pronunciation("", ""));
    note->setSelected(true);
    clipController->onInsertNote(note);
}
void PianoRollGraphicsViewPrivate::handleNotesMoved(int deltaTick, int deltaKey) const {
    Q_Q(const PianoRollGraphicsView);
    qDebug() << "PianoRollGraphicsView::handleMoveNotesCompleted"
             << "deltaTick:" << deltaTick << "deltaKey:" << deltaKey;
    clipController->onMoveNotes(q->selectedNotesId(), deltaTick, deltaKey);
}
void PianoRollGraphicsViewPrivate::handleNoteLeftResized(int noteId, int deltaTick) {
    qDebug() << "PianoRollGraphicsView::handleResizeNoteLeftCompleted"
             << "noteId:" << noteId << "deltaTick:" << deltaTick;
    QList<int> notes;
    notes.append(noteId);
    clipController->onResizeNotesLeft(notes, deltaTick);
}
void PianoRollGraphicsViewPrivate::handleNoteRightResized(int noteId, int deltaTick) {
    qDebug() << "PianoRollGraphicsView::handleResizeNoteRightCompleted"
             << "noteId:" << noteId << "deltaTick:" << deltaTick;
    QList<int> notes;
    notes.append(noteId);
    clipController->onResizeNotesRight(notes, deltaTick);
}
void PianoRollGraphicsViewPrivate::updateSelectionState() {
    Q_Q(PianoRollGraphicsView);
    m_canNotifySelectedNoteChanged = false;
    q->clearNoteSelections();

    for (const auto note : m_cachedSelectedNotes) {
        auto noteItem = m_noteLayer.findNoteById(note->id());
        noteItem->setSelected(note->selected());
    }
    m_cachedSelectedNotes.clear();
    m_canNotifySelectedNoteChanged = true;
}
void PianoRollGraphicsViewPrivate::updateOverlappedState() {
    Q_Q(PianoRollGraphicsView);
    for (const auto note : m_notes) {
        auto noteItem = m_noteLayer.findNoteById(note->id());
        noteItem->setOverlapped(note->overlapped());
    }
    q->update();
}
void PianoRollGraphicsViewPrivate::updateNoteTimeAndKey(Note *note) const {
    // qDebug() << "PianoRollGraphicsView::updateNoteTimeAndKey" << note->id() << note->start()
             // << note->length() << note->keyIndex();
    auto noteItem = m_noteLayer.findNoteById(note->id());
    noteItem->setRStart(note->rStart());
    noteItem->setLength(note->length());
    noteItem->setKeyIndex(note->keyIndex());
}
void PianoRollGraphicsViewPrivate::updateNoteWord(Note *note) const {
    // qDebug() << "PianoRollGraphicsView::updateNoteWord" << note->id() << note->lyric()
             // << note->pronunciation().original << note->pronunciation().edited;
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
    qDebug() << "PianoRollGraphicsView::updateMoveDeltaKeyRange" << m_moveMaxDeltaKey
             << m_moveMinDeltaKey;
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
void PianoRollGraphicsViewPrivate::setPitchEditMode(bool on) {
    Q_Q(PianoRollGraphicsView);
    for (auto note : m_noteLayer.noteItems())
        note->setEditingPitch(on);
    if (on)
        q->clearNoteSelections();
    m_pitchItem->setTransparentForMouseEvents(!on);
}
NoteView *PianoRollGraphicsViewPrivate::noteViewAt(const QPoint &pos) {
    Q_Q(PianoRollGraphicsView);
    for (const auto item : q->items(pos))
        if (auto noteItem = dynamic_cast<NoteView *>(item))
            return noteItem;
    return nullptr;
}
void PianoRollGraphicsViewPrivate::handleNoteInserted(Note *note) {
    m_canNotifySelectedNoteChanged = false;
    // qDebug() << "PianoRollGraphicsView::insertNote" << note->id() << note->lyric()
    // << note->pronunciation().original << note->pronunciation().edited;
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
    auto noteItem = m_noteLayer.findNoteById(note->id());
    m_layerManager->removeItem(noteItem, &m_noteLayer);
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
    // qDebug() << "PianoRollGraphicsViewPrivate::onClipPropertyChanged()" << m_offset;
    q->setOffset(m_offset);

    for (auto note : m_notes) {
        updateNoteTimeAndKey(note);
    }
}
void PianoRollGraphicsViewPrivate::updatePitch(Param::ParamType paramType,
                                               const Param &param) const {
    qDebug() << "PianoRollGraphicsView::updatePitch";
    OverlappableSerialList<DrawCurve> drawCurves;
    if (paramType == Param::Original) {
        for (const auto curve : param.curves(Param::Original))
            if (curve->type() == Curve::Draw)
                drawCurves.add(dynamic_cast<DrawCurve *>(curve));
        m_pitchItem->loadOriginal(drawCurves);
    } else {
        for (const auto curve : param.curves(Param::Edited))
            if (curve->type() == Curve::Draw)
                drawCurves.add(dynamic_cast<DrawCurve *>(curve));
        m_pitchItem->loadEdited(drawCurves);
    }
}