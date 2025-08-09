//
// Created by fluty on 24-9-16.
//

#include "ClipRangeOverlay.h"

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QPainter>

ClipRangeOverlay::ClipRangeOverlay() {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
}

void ClipRangeOverlay::setClipRange(const int clipStart, const int clipLen) {
    m_clipStart = clipStart;
    m_clipLen = clipLen;
    update();
}

void ClipRangeOverlay::updateRectAndPos() {
    const auto pos = visibleRect().topLeft();
    setPos(pos);
    setRect(QRectF(0, 0, visibleRect().width(), visibleRect().height()));
    update();
}

void ClipRangeOverlay::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                             QWidget *widget) {
    painter->setPen(Qt::NoPen);
    constexpr auto fillColor = QColor(0, 0, 0, 64);
    if (m_clipStart > startTick()) {
        constexpr auto topLeft = QPointF(0, 0);
        const auto bottomRight = QPointF(tickToItemX(m_clipStart), visibleRect().height());
        const auto rect = QRectF(topLeft, bottomRight);
        painter->fillRect(rect, fillColor);
    }
    const auto end = m_clipStart + m_clipLen;
    if (end < endTick()) {
        const auto topLeft = QPointF(tickToItemX(end), 0);
        const auto bottomRight = QPointF(visibleRect().width(), visibleRect().height());
        const auto rect = QRectF(topLeft, bottomRight);
        painter->fillRect(rect, fillColor);
    }
}