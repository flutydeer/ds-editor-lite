//
// Created by fluty on 24-8-26.
//

#include "RubberBandView.h"

#include "TimeGraphicsScene.h"

#include <QPainter>

RubberBandView::RubberBandView() {
    setZValue(100);
}

void RubberBandView::mouseDown(const QPointF &pos) {
    m_mouseDownPos = pos;
    m_size = QSizeF(0, 0);
    updateRectAndPos();
}

void RubberBandView::mouseMove(const QPointF &pos) {
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

void RubberBandView::setSelectMode(const SelectMode mode) {
    m_selectMode = mode;
}

void RubberBandView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                           QWidget *widget) {
    painter->setRenderHint(QPainter::Antialiasing);
    constexpr auto borderColor = QColor(255, 204, 153);
    constexpr auto backgroundColor = QColor(255, 204, 153, 64);
    constexpr auto penWidth = 1.5f;
    constexpr auto radiusBase = 6;
    const auto radiusX =
        boundingRect().width() / 2 >= radiusBase ? radiusBase : boundingRect().width() / 2;
    const auto radiusY =
        boundingRect().height() / 2 >= radiusBase ? radiusBase : boundingRect().height() / 2;
    const auto radius = std::min(radiusX, radiusY);

    QPen pen;
    pen.setWidthF(penWidth);
    pen.setColor(borderColor);
    painter->setBrush(backgroundColor);
    if (m_selectMode == SelectMode::RectSelect) {
        painter->setPen(pen);
        painter->drawRoundedRect(boundingRect(), radius, radius);
    } else if (m_selectMode == SelectMode::BeamSelect) {
        painter->setPen(Qt::NoPen);
        painter->drawRect(boundingRect());
        painter->setPen(pen);
        painter->drawLine(boundingRect().topLeft(), boundingRect().bottomLeft());
        painter->drawLine(boundingRect().topRight(), boundingRect().bottomRight());
    }
}

void RubberBandView::updateRectAndPos() {
    constexpr auto topLeft = QPointF(0, 0);
    if (m_selectMode == SelectMode::RectSelect) {
        setPos(m_pos);
        setRect(QRectF(topLeft, m_size));
    } else if (m_selectMode == SelectMode::BeamSelect && scene()) {
        setPos(m_pos.x(), 0);
        setRect(QRectF(topLeft, QPointF(m_size.width(), scene()->sceneRect().height())));
    }
    update();
}