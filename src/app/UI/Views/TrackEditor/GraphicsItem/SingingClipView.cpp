//
// Created by fluty on 2024/1/22.
//

#include "SingingClipView.h"

#include <QFile>
#include <QPainter>

#include "Global/TracksEditorGlobal.h"
#include "Model/AppModel/Note.h"
#include "UI/Utils/LanguageNameUtils.h"
#include "Utils/MathUtils.h"

using namespace TracksEditorGlobal;

int SingingClipView::NoteViewModel::compareTo(const NoteViewModel *obj) const {
    auto otherStart = obj->rStart;
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

SingingClipView::SingingClipView(int itemId, QGraphicsItem *parent)
    : AbstractClipView(itemId, parent) {
    setCanResizeLength(true);
    // setName("New Pattern");
}

void SingingClipView::loadNotes(const OverlappableSerialList<Note> &notes) {
    m_notes.clear();
    if (notes.count() != 0)
        for (const auto &note : notes)
            addNote(note);

    update();
}

void SingingClipView::onNoteListChanged(SingingClip::NoteChangeType type, Note *note) {
    switch (type) {
        case SingingClip::Inserted:
            addNote(note);
            connect(note, &Note::timeKeyPropertyChanged, this,
                    [=] { onNotePropertyChanged(note); });
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

void SingingClipView::onNotePropertyChanged(Note *note) {
    removeNote(note->id());
    addNote(note);
}

void SingingClipView::setDefaultLanguage(AppGlobal::LanguageType language) {
    m_language = language;
    update();
}

QString SingingClipView::text() const {
    return AbstractClipView::text() + langNameUtils->name(m_language) + " ";
}

void SingingClipView::drawPreviewArea(QPainter *painter, const QRectF &previewRect, int opacity) {
    painter->setRenderHint(QPainter::Antialiasing);

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

QString SingingClipView::clipTypeName() const {
    return tr("[Singing] ");
}

QString SingingClipView::iconPath() const {
    return ":svg/icons/midi_clip_16_filled.svg";
}

void SingingClipView::addNote(Note *note) {
    auto noteViewModel = new NoteViewModel;
    noteViewModel->id = note->id();
    noteViewModel->rStart = note->rStart();
    noteViewModel->length = note->length();
    noteViewModel->keyIndex = note->keyIndex();
    MathUtils::binaryInsert(m_notes, noteViewModel);
    update();
}

void SingingClipView::removeNote(int id) {
    for (auto note : m_notes) {
        if (note->id == id) {
            m_notes.removeOne(note);
            delete note;
            break;
        }
    }
    update();
}