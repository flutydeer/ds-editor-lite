//
// Created by fluty on 2024/1/23.
//

#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsView.h"

#include "GraphicsItem/PianoRollBackgroundGraphicsItem.h"
#include "Model/AppModel.h"
#include "Utils/MathUtils.h"

#include <QMouseEvent>
#include <QScrollBar>

PianoRollGraphicsView::PianoRollGraphicsView(PianoRollGraphicsScene *scene)
    : TimeGraphicsView(scene) {
    setScaleXMax(5);
    // QScroller::grabGesture(this, QScroller::TouchGesture);

    m_currentDrawingNote = new NoteGraphicsItem(-1);
    m_currentDrawingNote->setLyric("啦");
    m_currentDrawingNote->setPronunciation("la");
    connect(this, &PianoRollGraphicsView::scaleChanged, m_currentDrawingNote,
            &NoteGraphicsItem::setScale);
    connect(this, &PianoRollGraphicsView::visibleRectChanged, m_currentDrawingNote,
            &NoteGraphicsItem::setVisibleRect);
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
void PianoRollGraphicsView::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        TimeGraphicsView::mousePressEvent(event);
    }
    if (m_mode == DrawNote) {
        event->accept();
        auto scenePos = mapToScene(event->position().toPoint());
        auto tick = static_cast<int>(sceneXToTick(scenePos.x()));
        auto keyIndex = sceneYToKeyIndexInt(scenePos.y());

        if (auto item = itemAt(event->pos())) {
            if (dynamic_cast<PianoRollBackgroundGraphicsItem *>(item)) { // mouse down on background
                auto snapedTick =
                    MathUtils::roundDown(tick, 1920 / AppModel::instance()->quantize());
                qDebug() << "Draw note at" << snapedTick;
                m_currentDrawingNote->setStart(snapedTick);
                m_currentDrawingNote->setLength(1920 / AppModel::instance()->quantize());
                m_currentDrawingNote->setKeyIndex(keyIndex);
                scene()->addItem(m_currentDrawingNote);
                qDebug() << "fake note added to scene";
                m_mouseMoveBehavior = UpdateDrawingNote;
            } else if (dynamic_cast<NoteGraphicsItem *>(item)) {
                auto noteItem = dynamic_cast<NoteGraphicsItem *>(item);
                m_mouseMoveBehavior = Move;
                // TODO: resize or move note
            }
        }
    } else
        TimeGraphicsView::mousePressEvent(event);
}
void PianoRollGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    if (m_mode == DrawNote && m_mouseMoveBehavior == UpdateDrawingNote) {
        auto scenePos = mapToScene(event->position().toPoint());
        auto tick = static_cast<int>(sceneXToTick(scenePos.x()));
        auto quantizedTickLength = 1920 / AppModel::instance()->quantize();
        auto snapedTick = MathUtils::roundDown(tick, quantizedTickLength);
        auto targetLength = snapedTick - m_currentDrawingNote->start();
        if (targetLength >= quantizedTickLength)
            m_currentDrawingNote->setLength(targetLength);
    } else
        TimeGraphicsView::mouseMoveEvent(event);
}
void PianoRollGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    if (scene()->items().contains(m_currentDrawingNote)) {
        scene()->removeItem(m_currentDrawingNote);
        qDebug() << "fake note removed from scene";
    }
    if (m_mode == DrawNote &&m_mouseMoveBehavior == UpdateDrawingNote) {
        emit drawNoteCompleted(m_currentDrawingNote->start(), m_currentDrawingNote->length(),
                               m_currentDrawingNote->keyIndex());
    }
    TimeGraphicsView::mouseReleaseEvent(event);
}
void PianoRollGraphicsView::mouseDoubleClickEvent(QMouseEvent *event) {
    TimeGraphicsView::mouseDoubleClickEvent(event);
}
void PianoRollGraphicsView::insertNote(Note *dsNote) {
    auto noteItem = new NoteGraphicsItem(dsNote->id());
    noteItem->setContext(this);
    noteItem->setStart(dsNote->start());
    noteItem->setLength(dsNote->length());
    noteItem->setKeyIndex(dsNote->keyIndex());
    noteItem->setLyric(dsNote->lyric());
    noteItem->setPronunciation(dsNote->pronunciation());
    noteItem->setVisibleRect(visibleRect());
    noteItem->setScaleX(scaleX());
    noteItem->setScaleY(scaleY());
    scene()->addItem(noteItem);
    connect(this, &PianoRollGraphicsView::scaleChanged, noteItem, &NoteGraphicsItem::setScale);
    connect(this, &PianoRollGraphicsView::visibleRectChanged, noteItem,
            &NoteGraphicsItem::setVisibleRect);
    connect(noteItem, &NoteGraphicsItem::removeTriggered, this,
            [=] { emit removeNoteTriggered(); });
    connect(noteItem, &NoteGraphicsItem::editLyricTriggered, this,
            [=] { emit editNoteLyricTriggered(); });
    m_noteItems.append(noteItem);
}
void PianoRollGraphicsView::removeNote(int noteId) {
    auto noteItem = findNoteById(noteId);
    scene()->removeItem(noteItem);
    m_noteItems.removeOne(noteItem);
    delete noteItem;
}
void PianoRollGraphicsView::updateNote(Note *note) {
    auto noteItem = findNoteById(note->id());
    noteItem->setStart(note->start());
    noteItem->setLength(note->length());
    noteItem->setKeyIndex(note->keyIndex());
    noteItem->setLyric(note->lyric());
    noteItem->setPronunciation(note->pronunciation());
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
