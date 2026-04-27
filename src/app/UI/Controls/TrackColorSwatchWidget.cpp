//
// Created on 2026/4/27.
//

#include "TrackColorSwatchWidget.h"

#include "UI/Utils/TrackColorPalette.h"

#include <QMouseEvent>
#include <QPainter>

TrackColorSwatchWidget::TrackColorSwatchWidget(int currentIndex, QWidget *parent)
    : QWidget(parent), m_currentIndex(currentIndex) {
    setMouseTracking(true);
    int w = padding * 2 + columns * swatchSize + (columns - 1) * spacing;
    int h = padding * 2 + rows * swatchSize + (rows - 1) * spacing;
    setFixedSize(w, h);
}

QRect TrackColorSwatchWidget::swatchRect(int index) const {
    int col = index % columns;
    int row = index / columns;
    int x = padding + col * (swatchSize + spacing);
    int y = padding + row * (swatchSize + spacing);
    return {x, y, swatchSize, swatchSize};
}

int TrackColorSwatchWidget::indexAt(const QPoint &pos) const {
    for (int i = 0; i < rows * columns; i++) {
        if (swatchRect(i).contains(pos))
            return i;
    }
    return -1;
}

void TrackColorSwatchWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    auto &palette = *TrackColorPalette::instance();

    for (int i = 0; i < rows * columns; i++) {
        auto rect = swatchRect(i);
        auto color = palette.baseColor(i);

        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(rect, 4, 4);

        if (i == m_currentIndex) {
            auto borderRect = rect.adjusted(-1, -1, 1, 1);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(Qt::white, 1.5));
            painter.drawRoundedRect(borderRect, 5, 5);
        }

        if (i == m_hoveredIndex && i != m_currentIndex) {
            auto borderRect = rect.adjusted(-1, -1, 1, 1);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(255, 255, 255, 128), 1.5));
            painter.drawRoundedRect(borderRect, 5, 5);
        }
    }
}

void TrackColorSwatchWidget::mousePressEvent(QMouseEvent *event) {
    int idx = indexAt(event->pos());
    if (idx >= 0) {
        emit colorIndexSelected(idx);
    }
}

void TrackColorSwatchWidget::mouseMoveEvent(QMouseEvent *event) {
    int idx = indexAt(event->pos());
    if (idx != m_hoveredIndex) {
        m_hoveredIndex = idx;
        update();
    }
}

void TrackColorSwatchWidget::leaveEvent(QEvent *event) {
    Q_UNUSED(event)
    m_hoveredIndex = -1;
    update();
}
