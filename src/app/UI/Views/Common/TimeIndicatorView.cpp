#include "TimeIndicatorView.h"
#include "TimeGraphicsScene.h"
#include "Global/AppGlobal.h"

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
    const auto scene = dynamic_cast<const TimeGraphicsScene *>(this->scene());
    return tick * scaleX() * m_pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote +
           (scene ? scene->leftMarginPx() : 0.0);
}