//
// Created by fluty on 2024/1/23.
//

#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsView.h"

#include <QMouseEvent>
#include <QScrollBar>

PianoRollGraphicsView::PianoRollGraphicsView(PianoRollGraphicsScene *scene)
    : TimeGraphicsView(scene) {
    setScaleXMax(5);
    // QScroller::grabGesture(this, QScroller::TouchGesture);
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
    } else
        TimeGraphicsView::mousePressEvent(event);
}
void PianoRollGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    TimeGraphicsView::mouseMoveEvent(event);
}
void PianoRollGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    TimeGraphicsView::mouseReleaseEvent(event);
}
void PianoRollGraphicsView::mouseDoubleClickEvent(QMouseEvent *event) {
    TimeGraphicsView::mouseDoubleClickEvent(event);
}
void PianoRollGraphicsView::insertNote(DsNote *dsNote) {
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
    m_noteItems.append(noteItem);
}
void PianoRollGraphicsView::removeNote(int noteId) {
    auto noteItem = findNoteById(noteId);
    scene()->removeItem(noteItem);
    m_noteItems.removeOne(noteItem);
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
double PianoRollGraphicsView::sceneYToKeyIndex(double y) const {
    return 127 - y / scaleY() / noteHeight;
}
void PianoRollGraphicsView::reset() {
    for (auto note : m_noteItems) {
        scene()->removeItem(note);
        delete note;
    }
    m_noteItems.clear();
}
double PianoRollGraphicsView::topKeyIndex() const {
    return sceneYToKeyIndex(visibleRect().top());
}
double PianoRollGraphicsView::bottomKeyIndex() const {
    return sceneYToKeyIndex(visibleRect().bottom());
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
