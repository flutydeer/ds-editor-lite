//
// Created by fluty on 24-10-27.
//

#include "PronunciationView.h"
#include "NoteView.h"
#include "UI/Utils/AppColorPalette.h"

#include <QElapsedTimer>
#include <QPainter>

PronunciationView::PronunciationView(const int noteId, QGraphicsItem *parent)
    : AbstractGraphicsRectItem(parent), UniqueObject(noteId) {
}

PronunciationView::~PronunciationView() = default;

void PronunciationView::setPronunciation(const QString &pronunciation, const bool edited) {
    m_pronunciation = pronunciation;
    m_pronunciationEdited = edited;
    update();
}

void PronunciationView::setTextVisible(const bool visible) {
    if (m_textVisible != visible) {
        m_textVisible = visible;
        update();
    }
}

void PronunciationView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                              QWidget *widget) {
    if (!m_textVisible || m_editingPronunciation)
        return;

    QElapsedTimer timer;
    timer.start();
    constexpr auto pronColorOriginal = QColor(200, 200, 200);
    const auto pronColorEdited =
        AppColorPalette::instance()->phonemeEdited(NoteView::trackColorIndex());
    constexpr auto penWidth = 1.5f;
    constexpr int padding = 2;

    auto rect = boundingRect();
    auto left = rect.left() + penWidth;
    auto width = rect.width() - penWidth * 2;
    auto paddedRect = QRectF(left, rect.top(), width, rect.height());
    auto textRectLeft = paddedRect.left() + padding;
    auto textRectTop = paddedRect.top();
    auto textRectWidth = paddedRect.width() - 2 * padding;
    auto textRectHeight = paddedRect.height();
    auto textRect = QRectF(textRectLeft, textRectTop, textRectWidth, textRectHeight);

    QPen pen;
    pen.setWidthF(penWidth);
    pen.setColor(m_pronunciationEdited ? pronColorEdited : pronColorOriginal);
    painter->setPen(pen);
    painter->drawText(textRect, m_pronunciation);

}

void PronunciationView::updateRectAndPos() {
}

void PronunciationView::setEditingPronunciation(const bool editing) {
    if (m_editingPronunciation == editing)
        return;
    m_editingPronunciation = editing;
    update();
}

bool PronunciationView::isEditingPronunciation() const {
    return m_editingPronunciation;
}
