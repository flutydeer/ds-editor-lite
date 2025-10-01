//
// Created by fluty on 2024/1/22.
//

#include "SingingClipView.h"

#include <QFile>
#include <QPainter>

#include "Global/TracksEditorGlobal.h"
#include "Model/AppModel/Note.h"
#include "Utils/MathUtils.h"

using namespace TracksEditorGlobal;

int SingingClipView::NoteViewModel::compareTo(const NoteViewModel *obj) const {
    const auto otherStart = obj->rStart;
    if (rStart < otherStart)
        return -1;
    if (rStart > otherStart)
        return 1;
    return 0;
}

bool SingingClipView::NoteViewModel::isOverlappedWith(NoteViewModel *obj) {
    return false;
}

std::tuple<qsizetype, qsizetype> SingingClipView::NoteViewModel::interval() const {
    return std::make_tuple(0, 0);
}

SingingClipView::SingingClipView(const int itemId, QGraphicsItem *parent)
    : AbstractClipView(itemId, parent) {
    setCanResizeLength(true);
    // setName("New Pattern");
}

SingingClipView::~SingingClipView() {
    disconnect();
    dispose();
}

void SingingClipView::loadNotes(const OverlappableSerialList<Note> &notes) {
    dispose();
    if (notes.count() != 0)
        for (const auto &note : notes)
            addNote(note);

    update();
}

int SingingClipView::contentLength() const {
    if (m_notes.isEmpty())
        return 1920;
    const auto lastNote = m_notes.last();
    return lastNote->rStart + lastNote->length;
}

void SingingClipView::onNoteListChanged(const SingingClip::NoteChangeType type,
                                        const QList<Note *> &notes) {
    switch (type) {
        case SingingClip::Insert:
            for (const auto &note : notes)
                addNote(note);
            break;
        case SingingClip::TimeKeyPropertyChange:
            for (const auto &note : notes) {
                removeNote(note->id());
                addNote(note);
            }
            break;
        case SingingClip::Remove:
            for (const auto &note : notes)
                removeNote(note->id());
            break;
        default:
            break;
    }
    update();
}

void SingingClipView::onNotePropertyChanged(const Note *note) {
    removeNote(note->id());
    addNote(note);
}

void SingingClipView::setSingerName(const QString &singerName) {
    m_singerName = singerName;
    update();
}

void SingingClipView::setSpeakerName(const QString &speakerName) {
    m_speakerName = speakerName;
    update();
}

void SingingClipView::setDefaultLanguage(const QString &language) {
    m_language = language;
    update();
}

QString SingingClipView::text() const {
    return AbstractClipView::text() + m_singerName +
           (!m_speakerName.isEmpty() ? (" (" + m_speakerName + ")") : "") + " " + m_language + " ";
}

void SingingClipView::drawPreviewArea(QPainter *painter, const QRectF &previewRect,
                                      const QColor color) {
    // painter->setPen(Qt::red);
    // painter->drawRect(previewRect);
    // painter->setPen(Qt::NoPen);
    painter->setRenderHint(QPainter::Antialiasing);

    const auto rectTop = previewRect.top();
    const auto rectWidth = previewRect.width();
    const auto rectHeight = previewRect.height();

    if (rectHeight < 32 || rectWidth < 16)
        return;

    painter->setPen(Qt::NoPen);
    painter->setBrush(color);

    // find lowest and highest pitch
    int lowestKeyIndex = 127;
    int highestKeyIndex = 0;
    for (const auto note : m_notes) {
        const auto keyIndex = note->keyIndex;
        if (keyIndex < lowestKeyIndex)
            lowestKeyIndex = keyIndex;
        if (keyIndex > highestKeyIndex)
            highestKeyIndex = keyIndex;
    }

    const int divideCount = highestKeyIndex - lowestKeyIndex + 1;
    auto noteHeight = rectHeight / divideCount;
    auto totalHeight = rectHeight;
    if (noteHeight > 16) {
        noteHeight = 16;
        totalHeight = divideCount * noteHeight;
    }

    for (const auto &note : m_notes) {
        const auto clipLeft = start() + clipStart();
        const auto clipRight = clipLeft + clipLen();
        if (start() + note->rStart + note->length < clipLeft)
            continue;
        if (start() + note->rStart >= clipRight)
            break;

        const auto leftScene = tickToSceneX(start() + note->rStart);
        auto left = sceneXToItemX(leftScene);
        auto width = tickToSceneX(note->length);
        if (start() + note->rStart < clipLeft) {
            left = sceneXToItemX(tickToSceneX(clipLeft));
            width = sceneXToItemX(tickToSceneX(start() + note->rStart + note->length)) - left;
            // qDebug() << left << width << note->lyric;
        } else if (start() + note->rStart + note->length >= clipRight)
            width = tickToSceneX(clipRight - start() - note->rStart);
        const auto top = -(note->keyIndex - highestKeyIndex) * noteHeight + rectTop;
        painter->drawRect(QRectF(left, top, width, noteHeight));
    }
}

QString SingingClipView::clipTypeName() const {
    return tr("[Singing] ");
}

QString SingingClipView::iconPath() const {
    return ":svg/icons/midi_clip_16_filled.svg";
}

void SingingClipView::addNote(const Note *note) {
    const auto vm = new NoteViewModel;
    vm->id = note->id();
    vm->rStart = note->localStart();
    vm->length = note->length();
    vm->keyIndex = note->keyIndex();
    MathUtils::binaryInsert(m_notes, vm);
}

void SingingClipView::removeNote(const int id) {
    for (const auto note : m_notes) {
        if (note->id == id) {
            m_notes.removeOne(note);
            delete note;
            return;
        }
    }
    qWarning() << "removeNote: item not found:" << id;
}

void SingingClipView::dispose() {
    qDebug() << "dispose()";
    for (const auto note : m_notes)
        delete note;
    m_notes.clear();
}