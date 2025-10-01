//
// Created by fluty on 2024/2/3.
//

#include "TimeIndicatorView.h"

TimeIndicatorView::TimeIndicatorView(QObject *parent) : QObject(parent) {
    setFixedScaleY(true);
}

void TimeIndicatorView::setPixelsPerQuarterNote(int px) {
    m_pixelsPerQuarterNote = px;
    updateLengthAndPos();
}

void TimeIndicatorView::setPosition(double tick) {
    m_time = tick;
    updateLengthAndPos();
}

void TimeIndicatorView::setOffset(int tick) {
    m_offset = tick;
    updateLengthAndPos();
}

void TimeIndicatorView::afterSetScale() {
    updateLengthAndPos();
}

void TimeIndicatorView::afterSetVisibleRect() {
    updateLengthAndPos();
}

void TimeIndicatorView::updateLengthAndPos() {
    auto x = tickToItemX(m_time - m_offset);
    setPos(x, 0);
    auto line = QLineF(0, visibleRect().top(), 0, visibleRect().bottom());
    setLine(line);
    update();
}

double TimeIndicatorView::tickToItemX(double tick) const {
    return tick * scaleX() * m_pixelsPerQuarterNote / 480;
}