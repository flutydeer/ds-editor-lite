#include "AudioViewBackground.h"

#include "AudioLaneGeometry.h"

#include <QPainter>

AudioViewBackground::AudioViewBackground(QGraphicsItem *parent) : TimeGridView(parent) {
}

void AudioViewBackground::setChannelCount(const int count) {
    if (m_channelCount == count)
        return;
    m_channelCount = count;
    update();
}

void AudioViewBackground::setLaneBackgroundColor(const QColor &color) {
    m_laneBackgroundColor = color;
    update();
}

void AudioViewBackground::setLaneAlternateBackgroundColor(const QColor &color) {
    m_laneAlternateBackgroundColor = color;
    update();
}

void AudioViewBackground::setLaneSeparatorColor(const QColor &color) {
    m_laneSeparatorColor = color;
    update();
}

void AudioViewBackground::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                QWidget *widget) {
    const auto visible = visibleRect();
    if (!visible.isEmpty()) {
        const auto lanes = AudioLaneGeometry::laneCount(m_channelCount);
        painter->setPen(Qt::NoPen);
        for (auto lane = 0; lane < lanes; lane++) {
            const auto laneRect = AudioLaneGeometry::laneRect(visible, m_channelCount, lane);
            if (laneRect.isEmpty())
                continue;
            painter->fillRect(mapRectFromScene(laneRect),
                              lane % 2 == 0 ? m_laneBackgroundColor
                                            : m_laneAlternateBackgroundColor);
            if (lane > 0) {
                // Separator shared with the axis column so the bands read as one
                // row across the whole view
                const auto itemRect = mapRectFromScene(laneRect);
                painter->setPen(QPen(m_laneSeparatorColor, 1.0));
                const auto y = qRound(itemRect.top()) + 0.5;
                painter->drawLine(QPointF(itemRect.left(), y), QPointF(itemRect.right(), y));
                painter->setPen(Qt::NoPen);
            }
        }
    }
    TimeGridView::paint(painter, option, widget);
}

void AudioViewBackground::updateRectAndPos() {
    const auto visible = visibleRect();
    setPos(visible.topLeft());
    setRect(QRectF(0, 0, visible.width(), visible.height()));
    update();
}
