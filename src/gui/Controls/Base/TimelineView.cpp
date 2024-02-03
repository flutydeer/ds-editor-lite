//
// Created by fluty on 2024/2/3.
//

#include "TimelineView.h"
void TimelineView::setTimeRange(double startTick, double endTick) {
    m_startTick = startTick;
    m_endTick = endTick;
    update();
}
void TimelineView::setTimeSignature(int numerator, int denominator) {
    ITimelinePainter::setTimeSignature(numerator, denominator);
    update();
}
void TimelineView::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    QPainter painter(this);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 60));
    painter.drawRect(rect());
    painter.setBrush(Qt::NoBrush);

    drawTimeline(&painter, m_startTick, m_endTick, rect().width());
}
void TimelineView::drawBar(QPainter *painter, int tick, int bar) {
    QPen pen;
    auto x = tickToX(tick); // tick to itemX
    pen.setColor(barTextColor);
    painter->setPen(pen);
    painter->drawText(QPointF(x + m_textPaddingLeft, 10), QString::number(bar));
    pen.setColor(barLineColor);
    painter->setPen(pen);
    auto y1 = 0;
    auto y2 = rect().height();
    painter->drawLine(QLineF(x, y1, x, y2));
}
void TimelineView::drawBeat(QPainter *painter, int tick, int bar, int beat) {
    QPen pen;
    auto x = tickToX(tick);
    pen.setColor(beatTextColor);
    painter->setPen(pen);
    painter->drawText(QPointF(x + m_textPaddingLeft, 10), QString::number(bar) + "." + QString::number(beat));
    pen.setColor(beatLineColor);
    painter->setPen(pen);
    auto y1 = 2.0 / 3 * rect().height();
    auto y2 = rect().height();
    painter->drawLine(QLineF(x, y1, x, y2));
}
void TimelineView::drawEighth(QPainter *painter, int tick) {
    QPen pen;
    auto x = tickToX(tick);
    pen.setColor(commonLineColor);
    painter->setPen(pen);
    auto y1 = 3.0 / 4 * rect().height();
    auto y2 = rect().height();
    painter->drawLine(QLineF(x, y1, x, y2));
}
void TimelineView::wheelEvent(QWheelEvent *event) {
    emit wheelHorScale(event);
    QWidget::wheelEvent(event);
}
int TimelineView::tickToX(int tick) {
    auto ratio = (tick - m_startTick) / (m_endTick - m_startTick);
    auto x = qRound(rect().width() * ratio);
    return x;
}