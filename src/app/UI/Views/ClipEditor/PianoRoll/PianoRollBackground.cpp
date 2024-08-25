//
// Created by fluty on 2024/1/24.
//

#include "PianoRollBackground.h"

#include "PianoPaintUtils.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QPainter>

using namespace ClipEditorGlobal;

void PianoRollBackground::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                QWidget *widget) {
    // Draw background
    auto backgroundColor = QColor(42, 43, 44);
    painter->setPen(Qt::NoPen);
    painter->setBrush(backgroundColor);
    painter->drawRect(boundingRect());

    auto lineColor = QColor(57, 59, 61);
    // auto keyIndexTextColor = QColor(160, 160, 160);
    auto penWidth = 1;

    QPen pen;
    pen.setWidthF(penWidth);
    pen.setColor(lineColor);
    painter->setPen(pen);
    // painter->setRenderHint(QPainter::Antialiasing);

    auto sceneYToKeyIndex = [&](const double y) { return 127 - y / scaleY() / noteHeight; };

    auto keyIndexToSceneY = [&](const double index) {
        return (127 - index) * scaleY() * noteHeight;
    };

    auto sceneYToItemY = [&](const double y) { return mapFromScene(QPointF(0, y)).y(); };

    auto startKeyIndex = sceneYToKeyIndex(visibleRect().top());
    auto endKeyIndex = sceneYToKeyIndex(visibleRect().bottom());
    auto prevKeyIndex = static_cast<int>(startKeyIndex) + 1;
    for (int i = prevKeyIndex; i > endKeyIndex; i--) {
        auto y = sceneYToItemY(keyIndexToSceneY(i));

        painter->setBrush(PianoPaintUtils::isWhiteKey(i) ? QColor(42, 43, 44) : QColor(35, 36, 37));
        auto gridRect = QRectF(0, y, visibleRect().width(), noteHeight * scaleY());
        painter->setPen(Qt::NoPen);
        painter->drawRect(gridRect);

        // pen.setColor(keyIndexTextColor);
        // painter->setPen(pen);
        // painter->drawText(gridRect, PianoPaintUtils::noteName(i), QTextOption(Qt::AlignVCenter));

        if ((i + 1) % 12 == 0) {
            pen.setColor(lineColor);
            painter->setPen(pen);
            auto line = QLineF(0, y, visibleRect().width(), y);
            painter->drawLine(line);
        }
    }

    TimeGridGraphicsItem::paint(painter, option, widget);
}