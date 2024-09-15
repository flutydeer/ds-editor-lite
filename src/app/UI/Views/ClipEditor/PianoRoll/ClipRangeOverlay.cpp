//
// Created by fluty on 24-9-16.
//

#include "ClipRangeOverlay.h"

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QPainter>

ClipRangeOverlay::ClipRangeOverlay() {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
}

void ClipRangeOverlay::setClipRange(int clipStart, int clipLen) {
    m_clipStart = clipStart;
    m_clipLen = clipLen;
    update();
}

void ClipRangeOverlay::updateRectAndPos() {
    auto pos = visibleRect().topLeft();
    setPos(pos);
    setRect(QRectF(0, 0, visibleRect().width(), visibleRect().height()));
    update();
}

void ClipRangeOverlay::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                             QWidget *widget) {
    painter->setPen(Qt::NoPen);
    auto fillColor = QColor(0, 0, 0, 64);
    if (m_clipStart > startTick()) {
        auto topLeft = QPointF(0, 0);
        auto bottomRight = QPointF(tickToItemX(m_clipStart), visibleRect().height());
        auto rect = QRectF(topLeft, bottomRight);
        painter->fillRect(rect, fillColor);
    }
    auto end = m_clipStart + m_clipLen;
    if (end < endTick()) {
        auto topLeft = QPointF(tickToItemX(end), 0);
        auto bottomRight = QPointF(visibleRect().width(), visibleRect().height());
        auto rect = QRectF(topLeft, bottomRight);
        painter->fillRect(rect, fillColor);
    }
}