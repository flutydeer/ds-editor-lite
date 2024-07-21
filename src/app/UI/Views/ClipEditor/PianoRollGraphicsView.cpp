//
// Created by fluty on 2024/1/23.
//

#include "PianoRollGraphicsView.h"

#include <QMouseEvent>
#include <QScrollBar>

#include "PianoRollGraphicsScene.h"
#include "Controller/ClipEditorViewController.h"
#include "Controller/PlaybackController.h"
#include "Global/AppGlobal.h"
#include "GraphicsItem/NoteGraphicsItem.h"
#include "GraphicsItem/PitchEditorGraphicsItem.h"
#include "Model/AppModel.h"
#include "Model/Clip.h"
#include "Model/Curve.h"
#include "Model/Note.h"
#include "Utils/MathUtils.h"

PianoRollGraphicsView::PianoRollGraphicsView(PianoRollGraphicsScene *scene, QWidget *parent)
    : TimeGraphicsView(scene, parent), m_layerManager(scene) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("PianoRollGraphicsView");

    setScaleXMax(5);
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
    // QScroller::grabGesture(this, QScroller::TouchGesture);

    m_currentDrawingNote = new NoteGraphicsItem(-1);
    m_currentDrawingNote->setLyric(defaultLyric);
    m_currentDrawingNote->setPronunciation("");
    m_currentDrawingNote->setSelected(true);

    m_pitchItem = new PitchEditorGraphicsItem;
    m_pitchItem->setZValue(2);
    connect(m_pitchItem, &PitchEditorGraphicsItem::editCompleted, this,
            &PianoRollGraphicsView::onPitchEditorEditCompleted);
    scene->addCommonItem(m_pitchItem);
    m_pitchItem->setTransparentForMouseEvents(true);

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
void PianoRollGraphicsView::setDataContext(SingingClip *clip) {
    if (m_clip)
        disconnect(m_clip, nullptr, this, nullptr);

    m_clip = clip;
    if (!clip) {
        setSceneVisibility(false);
        setEnabled(false);
        return;
    }

    setSceneVisibility(true);
    setEnabled(true);

    if (m_clip->notes().count() > 0) {
        for (const auto note : m_clip->notes())
            handleNoteInserted(note);
        auto firstNote = m_clip->notes().at(0);
        setViewportCenterAt(firstNote->start(), firstNote->keyIndex());
    } else
        setViewportCenterAtKeyIndex(60);

    connect(m_clip, &SingingClip::noteChanged, this, &PianoRollGraphicsView::onNoteChanged);
    connect(m_clip, &SingingClip::noteSelectionChanged, this,
            &PianoRollGraphicsView::onNoteSelectionChanged);
    connect(m_clip, &SingingClip::paramChanged, this, &PianoRollGraphicsView::onParamChanged);
}
void PianoRollGraphicsView::onEditModeChanged(ClipEditorGlobal::PianoRollEditMode mode) {
    m_mode = mode;
    setEditMode(mode);
}
void PianoRollGraphicsView::onSceneSelectionChanged() const {
    if (m_canNotifySelectedNoteChanged) {
        auto notes = selectedNotesId();
        clipController->onNoteSelectionChanged(notes, true);
    }
}
void PianoRollGraphicsView::onPitchEditorEditCompleted() {
    qDebug() << "PianoRollGraphicsView::onPitchEditorEditCompleted";
    OverlapableSerialList<Curve> curves;
    auto newCurves = m_pitchItem->editedCurves();
    for (auto curve : newCurves) {
        curves.add(curve);
        qDebug() << "curve:"
                 << "#" << curve->id() << curve->start() << curve->endTick();
    }
    qDebug() << "curve count" << curves.count();
    // TODO: Add anchor curves
    clipController->onPitchEdited(curves);
}
void PianoRollGraphicsView::onNoteChanged(SingingClip::NoteChangeType type, Note *note) {
    if (type == SingingClip::Inserted)
        handleNoteInserted(note);
    else if (type == SingingClip::Removed)
        handleNoteRemoved(note);

    updateOverlappedState();
}
void PianoRollGraphicsView::onNoteSelectionChanged() {
    auto selectedNotes = m_clip->selectedNotes();
    m_cachedSelectedNotes = selectedNotes;
    if (!m_selecting)
        updateSelectionState();
}
void PianoRollGraphicsView::onParamChanged(ParamBundle::ParamName name, Param::ParamType type) {
    if (name == ParamBundle::Pitch) {
        auto pitchParam = m_clip->params.getParamByName(name);
        updatePitch(type, *pitchParam);
    }
}
void PianoRollGraphicsView::onRemoveSelectedNotes() const {
    qDebug() << "PianoRollGraphicsView::onRemoveSelectedNotes";
    auto notes = selectedNotesId();
    clipController->onRemoveNotes(notes);
}
void PianoRollGraphicsView::onEditSelectedNotesLyrics() const {
    qDebug() << "PianoRollGraphicsView::onEditSelectedNotes";
    auto notes = selectedNotesId();
    clipController->onEditNotesLyric(notes);
}
void PianoRollGraphicsView::paintEvent(QPaintEvent *event) {
    CommonGraphicsView::paintEvent(event);

    if (m_clip)
        return;

    QPainter painter(viewport());
    painter.setPen(QColor(160, 160, 160));
    painter.drawText(viewport()->rect(), tr("Select a singing clip to edit"),
                     QTextOption(Qt::AlignCenter));
}
void PianoRollGraphicsView::prepareForMovingOrResizingNotes(QMouseEvent *event, QPointF scenePos,
                                                            int keyIndex,
                                                            NoteGraphicsItem *noteItem) {
    bool ctrlDown = event->modifiers() == Qt::ControlModifier;
    if (!ctrlDown) {
        if (selectedNoteItems().count() <= 1 || !selectedNoteItems().contains(noteItem))
            clearNoteSelections();
        noteItem->setSelected(true);
    } else {
        noteItem->setSelected(!noteItem->isSelected());
    }
    auto rPos = noteItem->mapFromScene(scenePos);
    auto rx = rPos.x();
    if (rx >= 0 && rx <= AppGlobal::resizeTolarance) {
        // setCursor(Qt::SizeHorCursor);
        m_mouseMoveBehavior = ResizeLeft;
        clearNoteSelections();
        noteItem->setSelected(true);
    } else if (rx >= noteItem->rect().width() - AppGlobal::resizeTolarance &&
               rx <= noteItem->rect().width()) {
        // setCursor(Qt::SizeHorCursor);
        m_mouseMoveBehavior = ResizeRight;
        clearNoteSelections();
        noteItem->setSelected(true);
    } else {
        // setCursor(Qt::ArrowCursor);
        m_mouseMoveBehavior = Move;
    }

    m_currentEditingNote = noteItem;
    m_mouseDownPos = scenePos;
    m_mouseDownStart = m_currentEditingNote->start();
    m_mouseDownLength = m_currentEditingNote->length();
    m_mouseDownKeyIndex = keyIndex;
    updateMoveDeltaKeyRange();
}
void PianoRollGraphicsView::PrepareForDrawingNote(int tick, int keyIndex) {
    auto snapedTick = MathUtils::roundDown(tick, 1920 / appModel->quantize());
    qDebug() << "Draw note at" << snapedTick;
    m_currentDrawingNote->setStart(snapedTick);
    m_currentDrawingNote->setLength(1920 / appModel->quantize());
    m_currentDrawingNote->setKeyIndex(keyIndex);
    scene()->addCommonItem(m_currentDrawingNote);
    qDebug() << "fake note added to scene";
    m_mouseMoveBehavior = UpdateDrawingNote;
}
void PianoRollGraphicsView::mousePressEvent(QMouseEvent *event) {
    m_selecting = true;
    if (event->button() != Qt::LeftButton) {
        m_mouseMoveBehavior = None;
        if (auto noteItem = noteItemAt(event->pos())) {
            if (selectedNoteItems().count() <= 1 || !selectedNoteItems().contains(noteItem))
                clearNoteSelections();
            noteItem->setSelected(true);
        } else {
            clearNoteSelections();
            TimeGraphicsView::mousePressEvent(event);
        }
        return;
    }

    // event->accept();
    auto scenePos = mapToScene(event->position().toPoint());
    auto tick = static_cast<int>(sceneXToTick(scenePos.x()));
    auto keyIndex = sceneYToKeyIndexInt(scenePos.y());

    if (m_mode == Select) {
        if (auto noteItem = noteItemAt(event->pos())) {
            auto rPos = noteItem->mapFromScene(scenePos);
            auto ry = rPos.y();
            auto mouseInFilledRect =
                ry < noteItem->rect().height() - noteItem->pronunciationTextHeight();
            if (mouseInFilledRect)
                prepareForMovingOrResizingNotes(event, scenePos, keyIndex, noteItem);
            else
                TimeGraphicsView::mousePressEvent(event);
        } else
            TimeGraphicsView::mousePressEvent(event);
    } else if (m_mode == DrawNote) {
        clearNoteSelections();
        if (auto noteItem = noteItemAt(event->pos())) {
            qDebug() << "DrawNote mode, move or resize note";
            auto rPos = noteItem->mapFromScene(scenePos);
            auto ry = rPos.y();
            auto mouseInFilledRect =
                ry < noteItem->rect().height() - noteItem->pronunciationTextHeight();
            if (mouseInFilledRect)
                prepareForMovingOrResizingNotes(event, scenePos, keyIndex, noteItem);
            else
                PrepareForDrawingNote(tick, keyIndex);
        } else {
            PrepareForDrawingNote(tick, keyIndex);
        }
    } else
        TimeGraphicsView::mousePressEvent(event);
    event->ignore();
}
void PianoRollGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    if (event->modifiers() == Qt::AltModifier)
        m_tempQuantizeOff = true;
    else
        m_tempQuantizeOff = false;

    auto scenePos = mapToScene(event->position().toPoint());
    auto tick = static_cast<int>(sceneXToTick(scenePos.x()));
    auto quantizedTickLength = m_tempQuantizeOff ? 1 : 1920 / appModel->quantize();
    auto snapedTick = MathUtils::roundDown(tick, quantizedTickLength);
    auto keyIndex = sceneYToKeyIndexInt(scenePos.y());

    if (m_mouseMoveBehavior == Move) {
        auto deltaX = static_cast<int>(sceneXToTick(scenePos.x() - m_mouseDownPos.x()));
        auto startOffset = MathUtils::round(deltaX, quantizedTickLength);
        auto keyOffset = keyIndex - m_mouseDownKeyIndex;
        if (keyOffset > m_moveMaxDeltaKey)
            keyOffset = m_moveMaxDeltaKey;
        if (keyOffset < m_moveMinDeltaKey)
            keyOffset = m_moveMinDeltaKey;
        m_deltaTick = startOffset;
        m_deltaKey = keyOffset;
        moveSelectedNotes(m_deltaTick, m_deltaKey);
        m_movedBeforeMouseUp = true;
    } else if (m_mouseMoveBehavior == ResizeLeft) {
        auto start = snapedTick;
        auto deltaStart = start - m_mouseDownStart;
        auto length = m_mouseDownLength - deltaStart;
        if (length < quantizedTickLength)
            deltaStart = m_mouseDownLength - quantizedTickLength;
        m_deltaTick = deltaStart;
        resizeLeftSelectedNote(m_deltaTick);
    } else if (m_mouseMoveBehavior == ResizeRight) {
        auto deltaX = static_cast<int>(sceneXToTick(scenePos.x() - m_mouseDownPos.x()));
        auto lengthOffset = MathUtils::round(deltaX, quantizedTickLength);
        auto right = m_mouseDownStart + m_mouseDownLength + lengthOffset;
        auto length = right - m_mouseDownStart;
        auto deltaLength = length - m_mouseDownLength;
        if (length < quantizedTickLength)
            deltaLength = -(m_mouseDownLength - quantizedTickLength);
        // qDebug() << "deltaLength" << deltaLength;
        m_deltaTick = deltaLength;
        resizeRightSelectedNote(m_deltaTick);
    } else if (m_mouseMoveBehavior == UpdateDrawingNote) {
        auto targetLength = snapedTick - m_currentDrawingNote->start();
        if (targetLength >= quantizedTickLength)
            m_currentDrawingNote->setLength(targetLength);
    } else
        TimeGraphicsView::mouseMoveEvent(event);
}
void PianoRollGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    bool ctrlDown = event->modifiers() == Qt::ControlModifier;

    if (scene()->items().contains(m_currentDrawingNote)) {
        scene()->removeCommonItem(m_currentDrawingNote);
        qDebug() << "fake note removed from scene";
    }
    if (m_mouseMoveBehavior == Move) {
        if (m_movedBeforeMouseUp) {
            resetSelectedNotesOffset();
            handleMoveNotesCompleted(m_deltaTick, m_deltaKey);
        } else if (!ctrlDown) {
            clearNoteSelections(m_currentEditingNote);
        }
    } else if (m_mouseMoveBehavior == ResizeLeft) {
        resetSelectedNotesOffset();
        handleResizeNoteLeftCompleted(m_currentEditingNote->id(), m_deltaTick);
    } else if (m_mouseMoveBehavior == ResizeRight) {
        resetSelectedNotesOffset();
        handleResizeNoteRightCompleted(m_currentEditingNote->id(), m_deltaTick);
    } else if (m_mouseMoveBehavior == UpdateDrawingNote) {
        handleDrawNoteCompleted(m_currentDrawingNote->start(), m_currentDrawingNote->length(),
                                m_currentDrawingNote->keyIndex());
    }
    m_mouseMoveBehavior = None;
    m_deltaTick = 0;
    m_deltaKey = 0;
    m_movedBeforeMouseUp = false;
    m_currentEditingNote = nullptr;

    if (!m_cachedSelectedNotes.isEmpty())
        updateSelectionState();
    m_selecting = false;

    TimeGraphicsView::mouseReleaseEvent(event);
}
void PianoRollGraphicsView::mouseDoubleClickEvent(QMouseEvent *event) {
    // TimeGraphicsView::mouseDoubleClickEvent(event);
}
void PianoRollGraphicsView::handleDrawNoteCompleted(int start, int length, int keyIndex) {
    qDebug() << "PianoRollGraphicsView::handleDrawNoteCompleted" << start << length << keyIndex;
    auto note = new Note;
    note->setStart(start);
    note->setLength(length);
    note->setKeyIndex(keyIndex);
    note->setLyric(defaultLyric);
    note->setPronunciation(Pronunciation("", ""));
    note->setSelected(true);
    clipController->onInsertNote(note);
}
void PianoRollGraphicsView::handleMoveNotesCompleted(int deltaTick, int deltaKey) const {
    qDebug() << "PianoRollGraphicsView::handleMoveNotesCompleted"
             << "deltaTick:" << deltaTick << "deltaKey:" << deltaKey;
    clipController->onMoveNotes(selectedNotesId(), deltaTick, deltaKey);
}
void PianoRollGraphicsView::handleResizeNoteLeftCompleted(int noteId, int deltaTick) {
    qDebug() << "PianoRollGraphicsView::handleResizeNoteLeftCompleted"
             << "noteId:" << noteId << "deltaTick:" << deltaTick;
    QList<int> notes;
    notes.append(noteId);
    clipController->onResizeNotesLeft(notes, deltaTick);
}
void PianoRollGraphicsView::handleResizeNoteRightCompleted(int noteId, int deltaTick) {
    qDebug() << "PianoRollGraphicsView::handleResizeNoteRightCompleted"
             << "noteId:" << noteId << "deltaTick:" << deltaTick;
    QList<int> notes;
    notes.append(noteId);
    clipController->onResizeNotesRight(notes, deltaTick);
}
void PianoRollGraphicsView::updateSelectionState() {
    m_canNotifySelectedNoteChanged = false;
    clearNoteSelections();

    for (const auto note : m_cachedSelectedNotes) {
        auto noteItem = m_noteLayer.findNoteById(note->id());
        noteItem->setSelected(note->selected());
    }
    m_cachedSelectedNotes.clear();
    m_canNotifySelectedNoteChanged = true;
}
void PianoRollGraphicsView::updateOverlappedState() {
    for (const auto note : m_clip->notes()) {
        auto noteItem = m_noteLayer.findNoteById(note->id());
        noteItem->setOverlapped(note->overlapped());
    }
    update();
}
void PianoRollGraphicsView::insertNoteToView(Note *note) {
    m_canNotifySelectedNoteChanged = false;
    qDebug() << "PianoRollGraphicsView::insertNote" << note->id() << note->lyric();
    auto noteItem = new NoteGraphicsItem(note->id());
    noteItem->setContext(this);
    noteItem->setStart(note->start());
    noteItem->setLength(note->length());
    noteItem->setKeyIndex(note->keyIndex());
    noteItem->setLyric(note->lyric());
    // TODO:: setEditedPronunciation
    noteItem->setPronunciation(note->pronunciation().original);
    noteItem->setSelected(note->selected());
    noteItem->setOverlapped(note->overlapped());
    connect(noteItem, &NoteGraphicsItem::removeTriggered, this,
            &PianoRollGraphicsView::onRemoveSelectedNotes);
    connect(noteItem, &NoteGraphicsItem::editLyricTriggered, this,
            &PianoRollGraphicsView::onEditSelectedNotesLyrics);
    m_layerManager.addItem(noteItem, &m_noteLayer);
    m_canNotifySelectedNoteChanged = true;
}
void PianoRollGraphicsView::removeNoteFromView(int noteId) {
    m_canNotifySelectedNoteChanged = false;
    qDebug() << "PianoRollGraphicsView::removeNote" << noteId;
    auto noteItem = m_noteLayer.findNoteById(noteId);
    m_layerManager.removeItem(noteItem, &m_noteLayer);
    m_canNotifySelectedNoteChanged = true;
}
void PianoRollGraphicsView::updateNoteTimeAndKey(Note *note) {
    qDebug() << "PianoRollGraphicsView::updateNoteTimeAndKey" << note->id() << note->start()
             << note->length() << note->keyIndex();
    auto noteItem = m_noteLayer.findNoteById(note->id());
    noteItem->setStart(note->start());
    noteItem->setLength(note->length());
    noteItem->setKeyIndex(note->keyIndex());
}
void PianoRollGraphicsView::updateNoteWord(Note *note) {
    qDebug() << "PianoRollGraphicsView::updateNoteWord" << note->id() << note->lyric()
             << note->pronunciation().original;
    auto noteItem = m_noteLayer.findNoteById(note->id());
    noteItem->setLyric(note->lyric());
    // TODO:: setEditedPronunciation
    noteItem->setPronunciation(note->pronunciation().original);
}
double PianoRollGraphicsView::keyIndexToSceneY(double index) const {
    return (127 - index) * scaleY() * noteHeight;
}
double PianoRollGraphicsView::sceneYToKeyIndexDouble(double y) const {
    return 127 - y / scaleY() / noteHeight;
}
int PianoRollGraphicsView::sceneYToKeyIndexInt(double y) const {
    auto keyIndexD = sceneYToKeyIndexDouble(y);
    auto keyIndex = static_cast<int>(keyIndexD) + 1;
    if (keyIndex > 127)
        keyIndex = 127;
    return keyIndex;
}
void PianoRollGraphicsView::moveSelectedNotes(int startOffset, int keyOffset) {
    auto notes = selectedNoteItems();
    for (auto note : notes) {
        note->setStartOffset(startOffset);
        note->setKeyOffset(keyOffset);
    }
}
void PianoRollGraphicsView::resetSelectedNotesOffset() {
    auto notes = selectedNoteItems();
    for (auto note : notes)
        note->resetOffset();
}
void PianoRollGraphicsView::updateMoveDeltaKeyRange() {
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
void PianoRollGraphicsView::resetMoveDeltaKeyRange() {
    m_moveMaxDeltaKey = 127;
    m_moveMinDeltaKey = 0;
}
void PianoRollGraphicsView::resizeLeftSelectedNote(int offset) {
    // TODO: resize all selected notes
    m_currentEditingNote->setStartOffset(offset);
    m_currentEditingNote->setLengthOffset(-offset);
}
void PianoRollGraphicsView::resizeRightSelectedNote(int offset) {
    m_currentEditingNote->setLengthOffset(offset);
}
QList<NoteGraphicsItem *> PianoRollGraphicsView::selectedNoteItems() const {
    QList<NoteGraphicsItem *> list;
    for (const auto noteItem : m_noteLayer.noteItems()) {
        if (noteItem->isSelected())
            list.append(noteItem);
    }
    return list;
}
void PianoRollGraphicsView::setPitchEditMode(bool on) {
    for (auto note : m_noteLayer.noteItems())
        note->setEditingPitch(on);
    if (on)
        clearNoteSelections();
    m_pitchItem->setTransparentForMouseEvents(!on);
}
NoteGraphicsItem *PianoRollGraphicsView::noteItemAt(const QPoint &pos) {
    for (const auto item : items(pos))
        if (auto noteItem = dynamic_cast<NoteGraphicsItem *>(item))
            return noteItem;
    return nullptr;
}
void PianoRollGraphicsView::handleNoteInserted(Note *note) {
    insertNoteToView(note);
    connect(note, &Note::propertyChanged, this,
            [=](Note::NotePropertyType type) { handleNotePropertyChanged(type, note); });
}
void PianoRollGraphicsView::handleNoteRemoved(Note *note) {
    removeNoteFromView(note->id());
    disconnect(note, nullptr, this, nullptr);
}
void PianoRollGraphicsView::handleNotePropertyChanged(Note::NotePropertyType type, Note *note) {
    if (type == Note::TimeAndKey) {
        updateNoteTimeAndKey(note);
        updateOverlappedState();
    } else if (type == Note::Word)
        updateNoteWord(note);
}
void PianoRollGraphicsView::reset() {
    m_layerManager.destroyItems(&m_noteLayer);
}
QList<int> PianoRollGraphicsView::selectedNotesId() const {
    QList<int> list;
    for (const auto noteItem : m_noteLayer.noteItems()) {
        if (noteItem->isSelected())
            list.append(noteItem->id());
    }
    return list;
}
void PianoRollGraphicsView::clearNoteSelections(NoteGraphicsItem *except) {
    for (const auto noteItem : m_noteLayer.noteItems()) {
        if (noteItem != except && noteItem->isSelected())
            noteItem->setSelected(false);
    }
}
void PianoRollGraphicsView::updatePitch(Param::ParamType paramType, const Param &param) {
    qDebug() << "PianoRollGraphicsView::updatePitch";
    OverlapableSerialList<DrawCurve> drawCurves;
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
double PianoRollGraphicsView::topKeyIndex() const {
    return sceneYToKeyIndexDouble(visibleRect().top());
}
double PianoRollGraphicsView::bottomKeyIndex() const {
    return sceneYToKeyIndexDouble(visibleRect().bottom());
}
void PianoRollGraphicsView::setViewportTopKey(double key) {
    auto vBarValue = qRound(keyIndexToSceneY(key));
    // verticalScrollBar()->setValue(vBarValue);
    vBarAnimateTo(vBarValue);
}
void PianoRollGraphicsView::setViewportCenterAt(double tick, double keyIndex) {
    setViewportCenterAtTick(tick);
    setViewportCenterAtKeyIndex(keyIndex);
}
void PianoRollGraphicsView::setViewportCenterAtKeyIndex(double keyIndex) {
    auto keyIndexRange = topKeyIndex() - bottomKeyIndex();
    auto keyIndexStart = keyIndex + keyIndexRange / 2 + 0.5;
    qDebug() << "keyIndexStart" << keyIndexStart;
    setViewportTopKey(keyIndexStart);
}
void PianoRollGraphicsView::setEditMode(PianoRollEditMode mode) {
    m_mode = mode;
    switch (m_mode) {
        case Select:
            setDragMode(RubberBandDrag);
            setPitchEditMode(false);
            break;
        case DrawNote:
            setDragMode(NoDrag);
            setPitchEditMode(false);
            break;
        case DrawPitch:
        case EditPitchAnchor:
            setDragMode(NoDrag);
            setPitchEditMode(true);
            break;
    }
}
