//
// Created by fluty on 24-2-13.
//

#include "PhonemeGraphicsItem.h"

#include <QPainter>
#include <QTextOption>
#include <QGraphicsSceneContextMenuEvent>
#include <QCursor>

#include "Global/AppGlobal.h"

PhonemeGraphicsItem::PhonemeGraphicsItem(int noteId, PhonemeItemType type, QGraphicsItem *parent)
    : CommonGraphicsRectItem(parent), m_noteId(noteId), m_itemType(type) {
}
int PhonemeGraphicsItem::start() const {
    return m_start;
}
void PhonemeGraphicsItem::setStart(int start) {
    m_start = start;
    updateRectAndPos();
}
int PhonemeGraphicsItem::length() const {
    return m_length;
}
void PhonemeGraphicsItem::setLength(int length) {
    m_length = length;
    updateRectAndPos();
}
QString PhonemeGraphicsItem::name() const {
    return m_name;
}
void PhonemeGraphicsItem::setName(const QString &name) {
    m_name = name;
    update();
}
PhonemeGraphicsItem::PhonemeItemType PhonemeGraphicsItem::itemType() const {
    return m_itemType;
}
int PhonemeGraphicsItem::startOffset() const {
    return m_startOffset;
}
void PhonemeGraphicsItem::setStartOffset(int tick) {
    m_startOffset = tick;
    updateRectAndPos();
}
int PhonemeGraphicsItem::lengthOffset() const {
    return m_lengthOffset;
}
void PhonemeGraphicsItem::setLengthOffset(int tick) {
    m_lengthOffset = tick;
    updateRectAndPos();
}
void PhonemeGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                QWidget *widget) {
    const auto colorPrimary = QColor(155, 186, 255);
    const auto colorPrimaryDarker = QColor(112, 156, 255);
    /*const auto colorAccent = QColor(255, 175, 95);
    const auto colorAccentDarker = QColor(255, 159, 63);*/
    const auto colorForeground = QColor(0, 0, 0);
    const auto penWidth = 2.0f;

    QPen pen;
    if (isSelected())
        pen.setColor(QColor(255, 255, 255));
    /* else if (m_overlapped)
        pen.setColor(AppGlobal::overlappedViewBorder);*/
    else
        pen.setColor(colorPrimaryDarker);

    auto rect = boundingRect();
    auto noteBoundingRect = QRectF(rect.left(), rect.top(), rect.width(), rect.height());
    auto left = noteBoundingRect.left() + penWidth;
    auto top = noteBoundingRect.top() + penWidth;
    auto width = noteBoundingRect.width() - penWidth * 2;
    auto height = noteBoundingRect.height() - penWidth * 2;
    auto paddedRect = QRectF(left, top, width, height);

    pen.setWidthF(penWidth);
    painter->setPen(pen);
    painter->setBrush(colorPrimary);

    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(Qt::NoPen);
    painter->setBrush(isSelected() ? QColor(255, 255, 255) : colorPrimary);
    auto l = noteBoundingRect.left() + penWidth / 2;
    auto t = noteBoundingRect.top() + penWidth / 2;
    auto w = noteBoundingRect.width() - penWidth < 2 ? 2 : noteBoundingRect.width() - penWidth;
    auto h = noteBoundingRect.height() - penWidth < 2 ? 2 : noteBoundingRect.height() - penWidth;
    painter->drawRect(QRectF(l, t, w, h));

    pen.setColor(colorForeground);
    painter->setPen(pen);
    auto font = QFont();
    font.setPointSizeF(10);
    painter->setFont(font);
    int padding = 2;
    auto textRectLeft = paddedRect.left() + padding;
    // auto textRectTop = paddedRect.top() + padding;
    auto textRectTop = paddedRect.top();
    auto textRectWidth = paddedRect.width() - 2 * padding;
    // auto textRectHeight = paddedRect.height() - 2 * padding;
    auto textRectHeight = paddedRect.height();
    auto textRect = QRectF(textRectLeft, textRectTop, textRectWidth, textRectHeight);

    auto fontMetrics = painter->fontMetrics();
    auto textHeight = fontMetrics.height();
    auto text = m_name;
    auto textWidth = fontMetrics.horizontalAdvance(text);
    QTextOption textOption(Qt::AlignVCenter);
    textOption.setWrapMode(QTextOption::NoWrap);

    if (textWidth < textRectWidth && textHeight < textRectHeight)
        painter->drawText(textRect, text, textOption);
}
void PhonemeGraphicsItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    const auto rx = event->pos().rx();
    if (rx >= 0 && rx <= AppGlobal::resizeTolarance ||
        rx >= rect().width() - AppGlobal::resizeTolarance && rx <= rect().width())
        setCursor(Qt::SizeHorCursor);
    else
        setCursor(Qt::ArrowCursor);

    CommonGraphicsRectItem::hoverMoveEvent(event);
}
void PhonemeGraphicsItem::updateRectAndPos() {
    const auto x = (m_start + m_startOffset) * scaleX() * pixelsPerQuarterNote / 480;
    const auto w = (m_length + m_lengthOffset) * scaleX() * pixelsPerQuarterNote / 480;
    const auto h = phonemeEditorHeight;
    setPos(x, 0);
    setRect(QRectF(0, 0, w, h));
    update();
}