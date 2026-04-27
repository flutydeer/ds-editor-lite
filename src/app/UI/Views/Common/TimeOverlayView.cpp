//
// Created by fluty on 2024/1/25.
//

#include "TimeOverlayView.h"

#include <QGraphicsSceneMouseEvent>
#include "Global/AppGlobal.h"

bool TimeOverlayView::transparentMouseEvents() const {
    return m_transparentMouseEvents;
}

void TimeOverlayView::setTransparentMouseEvents(bool on) {
    m_transparentMouseEvents = on;
    setAcceptHoverEvents(!m_transparentMouseEvents);
    update();
}

void TimeOverlayView::setPixelsPerQuarterNote(int p) {
    pixelsPerQuarterNote =p;
}

void TimeOverlayView::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (!m_transparentMouseEvents)
        event->accept();
    else
        QGraphicsRectItem::mousePressEvent(event);
}

double TimeOverlayView::startTick() const {
    return sceneXToTick(visibleRect().left());
}

double TimeOverlayView::endTick() const {
    return sceneXToTick(visibleRect().right());
}

double TimeOverlayView::sceneXToTick(double x) const {
    auto tick = AppGlobal::ticksPerQuarterNote * x / scaleX() / pixelsPerQuarterNote;
    return tick;
}

double TimeOverlayView::tickToSceneX(double tick) const {
    auto x = tick * scaleX() * pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote;
    return x;
}

double TimeOverlayView::sceneXToItemX(double x) const {
    return mapFromScene(QPointF(x, 0)).x();
}

double TimeOverlayView::tickToItemX(double tick) const {
    return sceneXToItemX(tickToSceneX(tick));
}

double TimeOverlayView::sceneYToItemY(double y) const {
    return mapFromScene(QPointF(0, y)).y();
}