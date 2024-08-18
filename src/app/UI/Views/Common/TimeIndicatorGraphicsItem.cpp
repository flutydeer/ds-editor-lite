//
// Created by fluty on 2024/2/3.
//

#include "TimeIndicatorGraphicsItem.h"

TimeIndicatorGraphicsItem::TimeIndicatorGraphicsItem(QObject *parent) : QObject(parent) {
    setFixedScaleY(true);
}

void TimeIndicatorGraphicsItem::setPixelsPerQuarterNote(int px) {
    m_pixelsPerQuarterNote = px;
    updateLengthAndPos();
}

void TimeIndicatorGraphicsItem::setPosition(double tick) {
    m_time = tick;
    updateLengthAndPos();
}

void TimeIndicatorGraphicsItem::setOffset(int tick) {
    m_offset = tick;
    updateLengthAndPos();
}

void TimeIndicatorGraphicsItem::afterSetScale() {
    updateLengthAndPos();
}

void TimeIndicatorGraphicsItem::afterSetVisibleRect() {
    updateLengthAndPos();
}

void TimeIndicatorGraphicsItem::updateLengthAndPos() {
    auto x = tickToItemX(m_time - m_offset);
    setPos(x, 0);
    auto line = QLineF(0, visibleRect().top(), 0, visibleRect().bottom());
    setLine(line);
    update();
}

double TimeIndicatorGraphicsItem::tickToItemX(double tick) const {
    return tick * scaleX() * m_pixelsPerQuarterNote / 480;
}