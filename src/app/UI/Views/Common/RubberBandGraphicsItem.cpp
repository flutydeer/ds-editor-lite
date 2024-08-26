//
// Created by fluty on 24-8-26.
//

#include "RubberBandGraphicsItem.h"

#include <QPainter>

RubberBandGraphicsItem::RubberBandGraphicsItem() {
    setZValue(100);
}

void RubberBandGraphicsItem::mouseDown(const QPointF &pos) {
    m_mouseDownPos = pos;
    m_size = QSizeF(0, 0);
    updateRectAndPos();
}

void RubberBandGraphicsItem::mouseMove(const QPointF &pos) {
    m_currentMousePos = pos;
    const auto x1 = m_mouseDownPos.x();
    const auto y1 = m_mouseDownPos.y();
    const auto x2 = m_currentMousePos.x();
    const auto y2 = m_currentMousePos.y();
    const auto x = x1 < x2 ? x1 : x2;
    const auto y = y1 < y2 ? y1 : y2;
    m_pos = QPointF(x, y);
    m_size = QSizeF(qAbs(x1 - x2), qAbs(y1 - y2));
    updateRectAndPos();
}

void RubberBandGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                   QWidget *widget) {
    painter->setRenderHint(QPainter::Antialiasing);
    const auto borderColor = QColor(255, 204, 153);
    const auto backgroundColor = QColor(255, 204, 153, 64);
    const auto penWidth = 1.5f;
    const auto radiusBase = 6;
    const auto radiusX =
        boundingRect().width() / 2 >= radiusBase ? radiusBase : boundingRect().width() / 2;
    const auto radiusY =
        boundingRect().height() / 2 >= radiusBase ? radiusBase : boundingRect().height() / 2;
    const auto radius = std::min(radiusX, radiusY);

    QPen pen;
    pen.setWidthF(penWidth);
    pen.setColor(borderColor);
    painter->setPen(pen);
    painter->setBrush(backgroundColor);
    painter->drawRoundedRect(boundingRect(), radius, radius);
}

void RubberBandGraphicsItem::updateRectAndPos() {
    setPos(m_pos);
    setRect(QRectF(QPointF(0, 0), m_size));
    update();
}