//
// Created by FlutyDeer on 24-3-18.
//

#include "DividerLine.h"

#include <QPainter>
#include <QPaintEvent>

DividerLine::DividerLine(QWidget *parent) : QWidget(parent) {
    initSizePolicy();
}

DividerLine::DividerLine(const Qt::Orientation orientation, QWidget *parent)
    : QWidget(parent), m_orientation(orientation) {
    initSizePolicy();
}

void DividerLine::initSizePolicy() {
    if (m_orientation == Qt::Vertical)
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    else
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

Qt::Orientation DividerLine::orientation() const {
    return m_orientation;
}

void DividerLine::setOrientation(const Qt::Orientation orientation) {
    m_orientation = orientation;
    initSizePolicy();
    update();
    updateGeometry();
}

int DividerLine::lineWidth() const {
    return m_lineWidth;
}

void DividerLine::setLineWidth(const int width) {
    m_lineWidth = width;
    update();
}

int DividerLine::lineMargin() const {
    return m_lineMargin;
}

void DividerLine::setLineMargin(const int margin) {
    m_lineMargin = margin;
    update();
    updateGeometry();
}

QColor DividerLine::lineColor() const {
    return m_lineColor;
}

void DividerLine::setLineColor(const QColor &color) {
    m_lineColor = color;
    update();
}

void DividerLine::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen pen(m_lineColor, m_lineWidth, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(pen);

    if (m_orientation == Qt::Vertical) {
        // Vertical line: 1px wide, centered horizontally, with top/bottom margins
        const qreal cx = width() / 2.0;
        const qreal y1 = m_lineMargin;
        const qreal y2 = height() - m_lineMargin;
        if (y2 > y1)
            painter.drawLine(QPointF(cx, y1), QPointF(cx, y2));
    } else {
        // Horizontal line: 1px tall, centered vertically, with left/right margins
        const qreal cy = height() / 2.0;
        const qreal x1 = m_lineMargin;
        const qreal x2 = width() - m_lineMargin;
        if (x2 > x1)
            painter.drawLine(QPointF(x1, cy), QPointF(x2, cy));
    }
}

QSize DividerLine::sizeHint() const {
    if (m_orientation == Qt::Vertical)
        return {8, 2 * m_lineMargin + m_lineWidth};
    return {2 * m_lineMargin + m_lineWidth, 8};
}

QSize DividerLine::minimumSizeHint() const {
    if (m_orientation == Qt::Vertical)
        return {8, 0};
    return {0, 8};
}
