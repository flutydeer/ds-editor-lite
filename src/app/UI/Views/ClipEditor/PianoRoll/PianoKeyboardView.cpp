//
// Created by fluty on 24-8-19.
//

#include "PianoKeyboardView.h"

#include "PianoPaintUtils.h"
#include "Utils/Logger.h"

#include <QPainter>

PianoKeyboardView::PianoKeyboardView(QWidget *parent) {
    setFixedWidth(56);
}

void PianoKeyboardView::setKeyRange(double top, double bottom) {
    m_top = top;
    m_bottom = bottom;
    update();
}

void PianoKeyboardView::paintEvent(QPaintEvent *paint_event) {
    QElapsedTimer mstimer;
    mstimer.start();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    constexpr auto penWidth = 1;
    constexpr auto colorWhite = QColor(220, 220, 220);
    constexpr auto colorBlack = QColor(62, 63, 68);
    constexpr auto lineColor = QColor(160, 160, 160);

    QPen pen;
    pen.setWidthF(penWidth);
    // pen.setColor(lineColor);
    // painter.setPen(pen);

    auto pixelsPerKey = height() / (m_top - m_bottom);

    auto prevKeyIndex = static_cast<int>(m_top) + 1;
    bool prevIsWhiteKey = false;
    for (int i = prevKeyIndex; i > m_bottom; i--) {
        constexpr auto radius = 4.0;
        auto y = keyToY(i);

        auto isWhiteKey = PianoPaintUtils::isWhiteKey(i);
        painter.setBrush(isWhiteKey ? colorWhite : colorBlack);
        auto keyRect = QRectF(-radius, y, width() + radius, pixelsPerKey);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(keyRect, radius, radius);

        auto textRect = QRectF(6, y, width(), pixelsPerKey);
        auto fontMetrics = painter.fontMetrics();
        auto textHeight = fontMetrics.height();
        if (textRect.height() > textHeight) {
            pen.setColor(isWhiteKey ? colorBlack : colorWhite);
            painter.setPen(pen);
            painter.drawText(textRect, PianoPaintUtils::noteName(i), QTextOption(Qt::AlignVCenter));
        }

        if (prevIsWhiteKey && isWhiteKey) {
            pen.setColor(lineColor);
            painter.setPen(pen);
            auto line = QLineF(0, y, width() - radius + 1, y);
            painter.drawLine(line);
        }

        prevIsWhiteKey = isWhiteKey;
    }
    const auto time = static_cast<double>(mstimer.nsecsElapsed()) / 1000000.0;
    Logger::d("PianoKeyboardView", "paint time: " + QString::number(time));
}

double PianoKeyboardView::keyToY(double key) const {
    auto ratio = (key - m_top) / (m_bottom - m_top);
    auto y = height() * ratio;
    return y;
}