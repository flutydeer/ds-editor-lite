#include "TrackAppendSlotView.h"

#include <QPainter>

TrackAppendSlotView::TrackAppendSlotView(QWidget *parent)
    : QWidget(parent), WheelEventPolicySupport(WheelEventPolicy::Pass) {
    setObjectName(QStringLiteral("trackAppendSlotView"));
}

QColor TrackAppendSlotView::lineColor() const {
    return m_lineColor;
}

void TrackAppendSlotView::setLineColor(const QColor &color) {
    if (m_lineColor == color)
        return;
    m_lineColor = color;
    update();
}

void TrackAppendSlotView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(m_lineColor, 1));
    painter.drawLine(0, 0, width(), 0);
}

void TrackAppendSlotView::wheelEvent(QWheelEvent *event) {
    if (processWheelEventPolicy(this, event))
        return;
    QWidget::wheelEvent(event);
}
