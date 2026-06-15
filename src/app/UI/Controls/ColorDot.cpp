//
// Created by FlutyDeer on 2026/6/15.
//

#include "ColorDot.h"

#include <QPainter>
#include <QSizePolicy>

#include <algorithm>

ColorDot::ColorDot(QWidget *parent) : QWidget(parent) {
    setFixedSize(defaultSize, defaultSize);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

ColorDot::ColorDot(const QColor &color, QWidget *parent) : ColorDot(parent) {
    m_color = color;
}

double ColorDot::radius() const {
    return m_radius;
}

void ColorDot::setRadius(double radius) {
    radius = std::max(0.0, radius);
    if (m_radius == radius)
        return;

    m_radius = radius;
    update();
    emit radiusChanged(m_radius);
}

QColor ColorDot::color() const {
    return m_color;
}

void ColorDot::setColor(const QColor &color) {
    if (m_color == color)
        return;

    m_color = color;
    update();
    emit colorChanged(m_color);
}

void ColorDot::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    const double maxRadius = std::max(0.0, (std::min(width(), height()) - 1.0) / 2.0);
    const double dotRadius = std::min(m_radius, maxRadius);
    const QPointF center(width() / 2.0, height() / 2.0);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_color);
    painter.drawEllipse(center, dotRadius, dotRadius);
}
