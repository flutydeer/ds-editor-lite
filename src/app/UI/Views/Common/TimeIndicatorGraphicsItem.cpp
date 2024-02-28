//
// Created by fluty on 2024/2/3.
//

#include "TimeIndicatorGraphicsItem.h"
void TimeIndicatorGraphicsItem::setPixelsPerQuarterNote(int px) {
    m_pixelsPerQuarterNote = px;
    updateLengthAndPos();
}
void TimeIndicatorGraphicsItem::setPosition(double tick) {
    m_time = tick;
    updateLengthAndPos();
}
void TimeIndicatorGraphicsItem::setScale(qreal sx, qreal sy) {
    m_scaleX = sx;
    updateLengthAndPos();
}
void TimeIndicatorGraphicsItem::setVisibleRect(const QRectF &rect) {
    m_visibleRect = rect;
    updateLengthAndPos();
}
void TimeIndicatorGraphicsItem::updateLengthAndPos() {
    auto x = tickToItemX(m_time);
    setPos(x, 0);
    auto line = QLineF(0, m_visibleRect.top(), 0, m_visibleRect.bottom());
    setLine(line);
    update();
}
double TimeIndicatorGraphicsItem::tickToItemX(double tick) const {
    return tick * m_scaleX * m_pixelsPerQuarterNote / 480;
}