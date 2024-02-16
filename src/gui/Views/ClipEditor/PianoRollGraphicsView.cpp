//
// Created by fluty on 2024/1/23.
//

#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsView.h"

#include "GraphicsItem/PianoRollBackgroundGraphicsItem.h"
#include "Model/AppModel.h"
#include "Utils/AppGlobal.h"
#include "Utils/MathUtils.h"

#include <QMouseEvent>
#include <QScrollBar>

PianoRollGraphicsView::PianoRollGraphicsView(PianoRollGraphicsScene *scene)
    : TimeGraphicsView(scene) {
    setScaleXMax(5);
    // QScroller::grabGesture(this, QScroller::TouchGesture);

    m_currentDrawingNote = new NoteGraphicsItem(-1);
    m_currentDrawingNote->setLyric(defaultLyric);
    m_currentDrawingNote->setPronunciation(defaultPronunciation);
    m_currentDrawingNote->setSelected(true);
    connect(this, &PianoRollGraphicsView::scaleChanged, m_currentDrawingNote,
            &NoteGraphicsItem::setScale);
    connect(this, &PianoRollGraphicsView::visibleRectChanged, m_currentDrawingNote,
            &NoteGraphicsItem::setVisibleRect);
}
void PianoRollGraphicsView::onSceneSelectionChanged() {
    if (m_canNotifySelectedNoteChanged)
        emit selectedNoteChanged(selectedNotesId());
}
void PianoRollGraphicsView::paintEvent(QPaintEvent *event) {
    CommonGraphicsView::paintEvent(event);

    if (m_isSingingClipSelected)
        return;

    QPainter painter(viewport());
    painter.setPen(QColor(160, 160, 160));
    painter.drawText(viewport()->rect(), "Select a singing clip to edit",
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
    auto snapedTick = MathUtils::roundDown(tick, 1920 / AppModel::instance()->quantize());
    qDebug() << "Draw note at" << snapedTick;
    m_currentDrawingNote->setStart(snapedTick);
    m_currentDrawingNote->setLength(1920 / AppModel::instance()->quantize());
    m_currentDrawingNote->setKeyIndex(keyIndex);
    scene()->addItem(m_currentDrawingNote);
    qDebug() << "fake note added to scene";
    m_mouseMoveBehavior = UpdateDrawingNote;
}
void PianoRollGraphicsView::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        m_mouseMoveBehavior = None;
        if (auto item = itemAt(event->pos()))
            if (auto noteItem = dynamic_cast<NoteGraphicsItem *>(item)) {
                if (selectedNoteItems().count() <= 1 || !selectedNoteItems().contains(noteItem))
                    clearNoteSelections();
                noteItem->setSelected(true);
            }
        TimeGraphicsView::mousePressEvent(event);
        return;
    }

    m_selecting = true;
    event->accept();
    auto scenePos = mapToScene(event->position().toPoint());
    auto tick = static_cast<int>(sceneXToTick(scenePos.x()));
    auto keyIndex = sceneYToKeyIndexInt(scenePos.y());

    if (m_mode == Select) {
        if (auto item = itemAt(event->pos()))
            if (auto noteItem = dynamic_cast<NoteGraphicsItem *>(item)) {
                auto rPos = noteItem->mapFromScene(scenePos);
                auto ry = rPos.y();
                auto mouseInFilledRect =
                    ry < noteItem->rect().height() - noteItem->pronunciationTextHeight();
                if (mouseInFilledRect)
                    prepareForMovingOrResizingNotes(event, scenePos, keyIndex, noteItem);
                else
                    TimeGraphicsView::mousePressEvent(event);
            } else {
                TimeGraphicsView::mousePressEvent(event);
            }
        else
            TimeGraphicsView::mousePressEvent(event);
    } else if (m_mode == DrawNote) {
        clearNoteSelections();
        if (auto item = itemAt(event->pos())) {
            if (dynamic_cast<PianoRollBackgroundGraphicsItem *>(item)) { // mouse down on background
                PrepareForDrawingNote(tick, keyIndex);
            } else if (auto noteItem = dynamic_cast<NoteGraphicsItem *>(item)) {
                qDebug() << "DrawNote mode, move or resize note";
                auto rPos = noteItem->mapFromScene(scenePos);
                auto ry = rPos.y();
                auto mouseInFilledRect =
                    ry < noteItem->rect().height() - noteItem->pronunciationTextHeight();
                if (mouseInFilledRect)
                    prepareForMovingOrResizingNotes(event, scenePos, keyIndex, noteItem);
                else
                    PrepareForDrawingNote(tick, keyIndex);
            }
        }
    } else
        TimeGraphicsView::mousePressEvent(event);
}
void PianoRollGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    if (event->modifiers() == Qt::AltModifier)
        m_tempQuantizeOff = true;
    else
        m_tempQuantizeOff = false;

    auto scenePos = mapToScene(event->position().toPoint());
    auto tick = static_cast<int>(sceneXToTick(scenePos.x()));
    auto quantizedTickLength = m_tempQuantizeOff ? 1 : 1920 / AppModel::instance()->quantize();
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
        scene()->removeItem(m_currentDrawingNote);
        qDebug() << "fake note removed from scene";
    }
    if (m_mouseMoveBehavior == Move) {
        if (m_movedBeforeMouseUp) {
            resetSelectedNotesOffset();
            emit moveNotesCompleted(m_deltaTick, m_deltaKey);
        } else if (!ctrlDown) {
            clearNoteSelections(m_currentEditingNote);
        }
    } else if (m_mouseMoveBehavior == ResizeLeft) {
        resetSelectedNotesOffset();
        emit resizeNoteLeftCompleted(m_currentEditingNote->id(), m_deltaTick);
    } else if (m_mouseMoveBehavior == ResizeRight) {
        resetSelectedNotesOffset();
        emit resizeNoteRightCompleted(m_currentEditingNote->id(), m_deltaTick);
    } else if (m_mouseMoveBehavior == UpdateDrawingNote) {
        emit drawNoteCompleted(m_currentDrawingNote->start(), m_currentDrawingNote->length(),
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
void PianoRollGraphicsView::updateSelectionState() {
    m_canNotifySelectedNoteChanged = false;
    clearNoteSelections();

    for (const auto note : m_cachedSelectedNotes) {
        auto noteItem = findNoteById(note->id());
        noteItem->setSelected(note->selected());
    }
    m_cachedSelectedNotes.clear();
    m_canNotifySelectedNoteChanged = true;
}
void PianoRollGraphicsView::insertNote(Note *note) {
    m_canNotifySelectedNoteChanged = false;
    qDebug() << "PianoRollGraphicsView::insertNote" << note->id() << note->lyric();
    auto noteItem = new NoteGraphicsItem(note->id());
    noteItem->setContext(this);
    noteItem->setStart(note->start());
    noteItem->setLength(note->length());
    noteItem->setKeyIndex(note->keyIndex());
    noteItem->setLyric(note->lyric());
    noteItem->setPronunciation(note->pronunciation());
    noteItem->setVisibleRect(visibleRect());
    noteItem->setScaleX(scaleX());
    noteItem->setSelected(note->selected());
    noteItem->setScaleY(scaleY());
    noteItem->setOverlapped(note->overlapped());
    scene()->addItem(noteItem);
    connect(this, &PianoRollGraphicsView::scaleChanged, noteItem, &NoteGraphicsItem::setScale);
    connect(this, &PianoRollGraphicsView::visibleRectChanged, noteItem,
            &NoteGraphicsItem::setVisibleRect);
    connect(noteItem, &NoteGraphicsItem::removeTriggered, this,
            [=] { emit removeNoteTriggered(); });
    connect(noteItem, &NoteGraphicsItem::editLyricTriggered, this,
            [=] { emit editNoteLyricTriggered(); });
    m_noteItems.append(noteItem);
    m_canNotifySelectedNoteChanged = true;
}
void PianoRollGraphicsView::removeNote(int noteId) {
    m_canNotifySelectedNoteChanged = false;
    qDebug() << "PianoRollGraphicsView::removeNote" << noteId;
    auto noteItem = findNoteById(noteId);
    scene()->removeItem(noteItem);
    m_noteItems.removeOne(noteItem);
    delete noteItem;
    m_canNotifySelectedNoteChanged = true;
}
void PianoRollGraphicsView::updateNoteTimeAndKey(Note *note) {
    qDebug() << "PianoRollGraphicsView::updateNoteTimeAndKey" << note->id() << note->start()
             << note->length() << note->keyIndex();
    auto noteItem = findNoteById(note->id());
    noteItem->setStart(note->start());
    noteItem->setLength(note->length());
    noteItem->setKeyIndex(note->keyIndex());
}
void PianoRollGraphicsView::updateNoteWord(Note *note) {
    qDebug() << "PianoRollGraphicsView::updateNoteWord" << note->id() << note->lyric()
             << note->pronunciation();
    auto noteItem = findNoteById(note->id());
    noteItem->setLyric(note->lyric());
    noteItem->setPronunciation(note->pronunciation());
}
void PianoRollGraphicsView::updateNoteSelection(const QList<Note *> &selectedNotes) {
    qDebug() << "PianoRollGraphicsView::updateNoteSelection"
             << "selected notes"
             << (selectedNotes.isEmpty() ? "" : selectedNotes.first()->lyric());
    if (m_selecting)
        m_cachedSelectedNotes = selectedNotes;
    else
        updateSelectionState();
}
NoteGraphicsItem *PianoRollGraphicsView::findNoteById(int id) {
    for (const auto note : m_noteItems)
        if (note->id() == id)
            return note;
    return nullptr;
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
    for (const auto noteItem : m_noteItems) {
        if (noteItem->isSelected())
            list.append(noteItem);
    }
    return list;
}
void PianoRollGraphicsView::reset() {
    for (auto note : m_noteItems) {
        scene()->removeItem(note);
        delete note;
    }
    m_noteItems.clear();
}
QList<int> PianoRollGraphicsView::selectedNotesId() const {
    QList<int> list;
    for (const auto noteItem : m_noteItems) {
        if (noteItem->isSelected())
            list.append(noteItem->id());
    }
    return list;
}
void PianoRollGraphicsView::updateOverlappedState(SingingClip *singingClip) {
    for (const auto note : singingClip->notes()) {
        auto noteItem = findNoteById(note->id());
        noteItem->setOverlapped(note->overlapped());
    }
    update();
}
void PianoRollGraphicsView::clearNoteSelections(NoteGraphicsItem *except) {
    for (const auto noteItem : m_noteItems) {
        if (noteItem != except && noteItem->isSelected())
            noteItem->setSelected(false);
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
    verticalScrollBar()->setValue(vBarValue);
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
void PianoRollGraphicsView::setIsSingingClip(bool isSingingClip) {
    m_isSingingClipSelected = isSingingClip;
    update();
}
void PianoRollGraphicsView::setEditMode(PianoRollEditMode mode) {
    m_mode = mode;
    switch (m_mode) {

        case Select:
            setDragMode(RubberBandDrag);
            break;

        case DrawNote:
        case DrawPitch:
        case EditPitchAnchor:
            setDragMode(NoDrag);
            break;
    }
}
