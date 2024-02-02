//
// Created by fluty on 2024/1/21.
//

#include <QDebug>
#include <QPainter>

#include "TimeGridGraphicsItem.h"

TimeGridGraphicsItem::TimeGridGraphicsItem(QGraphicsItem *parent) {
}

void TimeGridGraphicsItem::setPixelsPerQuarterNote(int px) {
    m_pixelsPerQuarterNote = px;
    update();
}
void TimeGridGraphicsItem::onTimeSignatureChanged(int numerator, int denominator) {
    m_numerator = numerator;
    m_denominator = denominator;
    update();
}
void TimeGridGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                 QWidget *widget) {
    auto penWidth = 1;

    QPen pen;
    pen.setWidthF(penWidth);
    painter->setBrush(Qt::NoBrush);
    pen.setColor(commonLineColor);
    painter->setPen(pen);
    // painter->setRenderHint(QPainter::Antialiasing);

    auto startTick = sceneXToTick(visibleRect().left());
    auto endTick = sceneXToTick(visibleRect().right());
    drawTimeline(painter, startTick, endTick, m_numerator, m_denominator, m_pixelsPerQuarterNote, scaleX());
}

void TimeGridGraphicsItem::updateRectAndPos() {
    auto pos = visibleRect().topLeft();
    setPos(pos);
    setRect(QRectF(0, 0, visibleRect().width(), visibleRect().height()));
    update();
}
void TimeGridGraphicsItem::drawBar(QPainter *painter, int tick, int bar) {
    QPen pen;
    auto x = sceneXToItemX(tickToSceneX(tick));
    pen.setColor(barTextColor);
    painter->setPen(pen);
    painter->drawText(QPointF(x, 10), QString::number(bar));
    pen.setColor(barLineColor);
    painter->setPen(pen);
    painter->drawLine(QLineF(x, 0, x, visibleRect().height()));
}
void TimeGridGraphicsItem::drawBeat(QPainter *painter, int tick, int bar, int beat) {
    QPen pen;
    auto x = sceneXToItemX(tickToSceneX(tick));
    pen.setColor(beatTextColor);
    painter->setPen(pen);
    painter->drawText(QPointF(x, 10), QString::number(bar) + "." + QString::number(beat));
    pen.setColor(beatLineColor);
    painter->setPen(pen);
    painter->drawLine(QLineF(x, 0, x, visibleRect().height()));
}
void TimeGridGraphicsItem::drawEighth(QPainter *painter, int tick) {
    QPen pen;
    auto x = sceneXToItemX(tickToSceneX(tick));
    pen.setColor(commonLineColor);
    painter->setPen(pen);
    painter->drawLine(QLineF(x, 0, x, visibleRect().height()));

}
double TimeGridGraphicsItem::sceneXToTick(double pos) {
    return 480 * pos / scaleX() / m_pixelsPerQuarterNote;
}
double TimeGridGraphicsItem::tickToSceneX(double tick) {
    return tick * scaleX() * m_pixelsPerQuarterNote / 480;
}
double TimeGridGraphicsItem::sceneXToItemX(double x) {
    return mapFromScene(QPointF(x, 0)).x();
}