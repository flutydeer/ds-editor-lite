//
// Created by fluty on 2024/1/22.
//

#include "SingingClipGraphicsItem.h"

#include <QFile>
#include <QPainter>

#include "Global/TracksEditorGlobal.h"
#include "Model/Note.h"

using namespace TracksEditorGlobal;

int SingingClipGraphicsItem::NoteViewModel::compareTo(NoteViewModel *obj) const {
    auto otherStart = obj->rStart;
    if (rStart < otherStart)
        return -1;
    if (rStart > otherStart)
        return 1;
    return 0;
}
bool SingingClipGraphicsItem::NoteViewModel::isOverlappedWith(NoteViewModel *obj) {
    return false;
}
std::tuple<qsizetype, qsizetype> SingingClipGraphicsItem::NoteViewModel::interval() const {
    return std::make_tuple(0, 0);
}
SingingClipGraphicsItem::SingingClipGraphicsItem(int itemId, QGraphicsItem *parent)
    : AbstractClipGraphicsItem(itemId, parent) {
    setCanResizeLength(true);
    // setName("New Pattern");
}
void SingingClipGraphicsItem::loadNotes(const OverlappableSerialList<Note> &notes) {
    m_notes.clear();
    if (notes.count() != 0)
        for (const auto &note : notes)
            addNote(note);

    update();
}
QString SingingClipGraphicsItem::audioCachePath() const {
    return m_audioCachePath;
}
void SingingClipGraphicsItem::setAudioCachePath(const QString &path) {
    m_audioCachePath = path;
}
void SingingClipGraphicsItem::onNoteListChanged(SingingClip::NoteChangeType type, Note *note) {
    switch (type) {
        case SingingClip::Inserted:
            addNote(note);
            connect(note, &Note::propertyChanged, this,
                    [=](Note::NotePropertyType type) { onNotePropertyChanged(type, note); });
            break;
        // case SingingClip::PropertyChanged:
        //     removeNote(id);
        //     addNote(note);
        //     break;
        case SingingClip::Removed:
            removeNote(note->id());
            break;
    }
}
void SingingClipGraphicsItem::onNotePropertyChanged(Note::NotePropertyType type, Note *note) {
    if (type == Note::TimeAndKey) {
        removeNote(note->id());
        addNote(note);
    }
}
void SingingClipGraphicsItem::drawPreviewArea(QPainter *painter, const QRectF &previewRect,
                                              int opacity) {
    painter->setRenderHint(QPainter::Antialiasing, false);

    auto rectTop = previewRect.top();
    auto rectWidth = previewRect.width();
    auto rectHeight = previewRect.height();

    if (rectHeight < 32 || rectWidth < 16)
        return;
    auto colorAlpha = rectHeight <= 48 ? 255 * (rectHeight - 32) / (48 - 32) : 255;
    auto noteColor = QColor(10, 10, 10, static_cast<int>(colorAlpha));

    painter->setPen(Qt::NoPen);
    painter->setBrush(noteColor);

    // find lowest and highest pitch
    int lowestKeyIndex = 127;
    int highestKeyIndex = 0;
    for (const auto note : m_notes) {
        auto keyIndex = note->keyIndex;
        if (keyIndex < lowestKeyIndex)
            lowestKeyIndex = keyIndex;
        if (keyIndex > highestKeyIndex)
            highestKeyIndex = keyIndex;
    }

    int divideCount = highestKeyIndex - lowestKeyIndex + 1;
    auto noteHeight = (rectHeight - rectTop) / divideCount;

    for (const auto &note : m_notes) {
        auto clipLeft = start() + clipStart();
        auto clipRight = clipLeft + clipLen();
        if (start() + note->rStart + note->length < clipLeft)
            continue;
        if (start() + note->rStart >= clipRight)
            break;

        auto leftScene = tickToSceneX(start() + note->rStart);
        auto left = sceneXToItemX(leftScene);
        auto width = tickToSceneX(note->length);
        if (start() + note->rStart < clipLeft) {
            left = sceneXToItemX(tickToSceneX(clipLeft));
            width = sceneXToItemX(tickToSceneX(start() + note->rStart + note->length)) - left;
            // qDebug() << left << width << note->lyric;
        } else if (start() + note->rStart + note->length >= clipRight)
            width = tickToSceneX(clipRight - start() - note->rStart);
        auto top = -(note->keyIndex - highestKeyIndex) * noteHeight + rectTop;
        painter->drawRect(QRectF(left, top, width, noteHeight));
    }
}
QString SingingClipGraphicsItem::clipTypeName() {
    return tr("[Singing] ");
}
void SingingClipGraphicsItem::addNote(Note *note) {
    auto noteViewModel = new NoteViewModel;
    noteViewModel->id = note->id();
    noteViewModel->rStart = note->start() - start();
    noteViewModel->length = note->length();
    noteViewModel->keyIndex = note->keyIndex();
    m_notes.add(noteViewModel);

    update();
}
void SingingClipGraphicsItem::removeNote(int id) {
    for (int i = 0; i < m_notes.count(); i++) {
        auto note = m_notes.at(i);
        if (note->id == id) {
            m_notes.remove(note);
            break;
        }
    }
    update();
}