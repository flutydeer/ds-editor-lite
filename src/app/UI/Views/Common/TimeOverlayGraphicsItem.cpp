//
// Created by fluty on 2024/1/25.
//

#include "TimeOverlayGraphicsItem.h"

#include <QGraphicsSceneMouseEvent>

bool TimeOverlayGraphicsItem::transparentMouseEvents() const {
    return m_transparentMouseEvents;
}

void TimeOverlayGraphicsItem::setTransparentMouseEvents(bool on) {
    m_transparentMouseEvents = on;
    setAcceptHoverEvents(!m_transparentMouseEvents);
    update();
}

void TimeOverlayGraphicsItem::setPixelsPerQuarterNote(int p) {
    pixelsPerQuarterNote =p;
}

void TimeOverlayGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (!m_transparentMouseEvents)
        event->accept();
    else
        QGraphicsRectItem::mousePressEvent(event);
}

double TimeOverlayGraphicsItem::startTick() const {
    return sceneXToTick(visibleRect().left());
}

double TimeOverlayGraphicsItem::endTick() const {
    return sceneXToTick(visibleRect().right());
}

double TimeOverlayGraphicsItem::sceneXToTick(double x) const {
    auto tick = 480 * x / scaleX() / pixelsPerQuarterNote;
    return tick;
}

double TimeOverlayGraphicsItem::tickToSceneX(double tick) const {
    auto x = tick * scaleX() * pixelsPerQuarterNote / 480;
    return x;
}

double TimeOverlayGraphicsItem::sceneXToItemX(double x) const {
    return mapFromScene(QPointF(x, 0)).x();
}

double TimeOverlayGraphicsItem::tickToItemX(double tick) const {
    return sceneXToItemX(tickToSceneX(tick));
}

double TimeOverlayGraphicsItem::sceneYToItemY(double y) const {
    return mapFromScene(QPointF(0, y)).y();
}