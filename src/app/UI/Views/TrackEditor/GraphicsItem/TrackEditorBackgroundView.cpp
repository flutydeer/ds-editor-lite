//
// Created by fluty on 2024/1/22.
//

#include "TrackEditorBackgroundView.h"

#include <QPainter>

#include "Global/TracksEditorGlobal.h"

using namespace TracksEditorGlobal;

void TrackEditorBackgroundView::onTrackCountChanged(const int count) {
    m_trackCount = count;
    update();
}

void TrackEditorBackgroundView::onTrackSelectionChanged(const int trackIndex) {
    m_trackIndex = trackIndex;
    update();
}

void TrackEditorBackgroundView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                      QWidget *widget) {
    auto sceneYToTrackIndex = [&](const double y) { return y / scaleY() / trackHeight; };
    auto trackIndexToSceneY = [&](const double index) { return index * scaleY() * trackHeight; };
    auto sceneYToItemY = [&](const double y) { return mapFromScene(QPointF(0, y)).y(); };
    const auto startTrackIndex = sceneYToTrackIndex(visibleRect().top());
    auto endTrackIndex = sceneYToTrackIndex(visibleRect().bottom());

    // Draw background
    // auto backgroundColor = QColor(42, 43, 44);
    // painter->setPen(Qt::NoPen);
    // painter->setBrush(backgroundColor);
    // painter->drawRect(boundingRect());

    // Draw selected track background
    constexpr auto selectionBackgroundColor = QColor(55, 56, 57);
    if (m_trackIndex != -1) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(selectionBackgroundColor);
        const auto y = sceneYToItemY(trackIndexToSceneY(m_trackIndex));
        const auto rect = QRectF(0, y, visibleRect().width(), trackHeight * scaleY());
        painter->drawRect(rect);
    }

    TimeGridView::paint(painter, option, widget);

    // Draw track divider
    constexpr auto penWidth = 1;

    QPen pen;
    pen.setWidthF(penWidth);
    pen.setColor(commonLineColor());
    painter->setPen(pen);
    // painter->setRenderHint(QPainter::Antialiasing);

    if (m_trackCount < endTrackIndex)
        endTrackIndex = m_trackCount + 1;
    const auto prevLineTrackIndex = static_cast<int>(startTrackIndex);
    for (int i = prevLineTrackIndex + 1; i < endTrackIndex; i++) {
        const auto y = sceneYToItemY(trackIndexToSceneY(i));
        auto line = QLineF(0, y, visibleRect().width(), y);
        painter->drawLine(line);
    }
}