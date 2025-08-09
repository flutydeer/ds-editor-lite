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
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_whiteKeyColor);
    painter->drawRect(boundingRect());

    constexpr auto penWidth = 1;

    QPen pen;
    pen.setWidthF(penWidth);
    pen.setColor(m_octaveDividerColor);
    painter->setPen(pen);
    // painter->setRenderHint(QPainter::Antialiasing);

    auto sceneYToKeyIndex = [&](const double y) { return 127 - y / scaleY() / noteHeight; };

    auto keyIndexToSceneY = [&](const double index) {
        return (127 - index) * scaleY() * noteHeight;
    };

    auto sceneYToItemY = [&](const double y) { return mapFromScene(QPointF(0, y)).y(); };

    const auto startKeyIndex = sceneYToKeyIndex(visibleRect().top());
    const auto endKeyIndex = sceneYToKeyIndex(visibleRect().bottom());
    const auto prevKeyIndex = static_cast<int>(startKeyIndex) + 1;
    for (int i = prevKeyIndex; i > endKeyIndex; i--) {
        const auto y = sceneYToItemY(keyIndexToSceneY(i));

        painter->setBrush(PianoPaintUtils::isWhiteKey(i) ? m_whiteKeyColor : m_blackKeyColor);
        auto gridRect = QRectF(0, y, visibleRect().width(), noteHeight * scaleY());
        painter->setPen(Qt::NoPen);
        painter->drawRect(gridRect);

        // pen.setColor(keyIndexTextColor);
        // painter->setPen(pen);
        // painter->drawText(gridRect, PianoPaintUtils::noteName(i), QTextOption(Qt::AlignVCenter));

        if ((i + 1) % 12 == 0) {
            pen.setColor(m_octaveDividerColor);
            painter->setPen(pen);
            auto line = QLineF(0, y, visibleRect().width(), y);
            painter->drawLine(line);
        }
    }

    TimeGridView::paint(painter, option, widget);
}

QColor PianoRollBackground::whiteKeyColor() const {
    return m_whiteKeyColor;
}

void PianoRollBackground::setWhiteKeyColor(const QColor &color) {
    m_whiteKeyColor = color;
    update();
}

QColor PianoRollBackground::blackKeyColor() const {
    return m_blackKeyColor;
}

void PianoRollBackground::setBlackKeyColor(const QColor &color) {
    m_blackKeyColor = color;
    update();
}

QColor PianoRollBackground::octaveDividerColor() const {
    return m_octaveDividerColor;
}

void PianoRollBackground::setOctaveDividerColor(const QColor &color) {
    m_octaveDividerColor = color;
    update();
}
