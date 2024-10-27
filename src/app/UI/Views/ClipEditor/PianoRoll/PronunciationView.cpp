//
// Created by fluty on 24-10-27.
//

#include "PronunciationView.h"

#include <QPainter>

PronunciationView::PronunciationView(QGraphicsItem *parent) : AbstractGraphicsRectItem(parent){
}

void PronunciationView::setPronunciation(const QString &pronunciation, bool edited) {
    m_pronunciation = pronunciation;
    m_pronunciationEdited = edited;
    update();
}

void PronunciationView::setTextVisible(bool visible) {
    if (m_textVisible != visible)
        m_textVisible = visible;
    update();
}

void PronunciationView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                              QWidget *widget) {
    if (!m_textVisible)
        return;

    const auto pronColorOriginal = QColor(200, 200, 200);
    const auto pronColorEdited = QColor(155, 186, 255);
    const auto penWidth = 1.5f;
    const int padding = 2;

    QPen pen;
    pen.setWidthF(penWidth);
    pen.setColor(m_pronunciationEdited ? pronColorEdited : pronColorOriginal);
    painter->setPen(pen);
    painter->drawText(boundingRect(), m_pronunciation);
}

void PronunciationView::updateRectAndPos() {
    update();
}