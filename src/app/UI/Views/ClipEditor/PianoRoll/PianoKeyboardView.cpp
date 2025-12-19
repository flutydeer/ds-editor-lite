//
// Created by fluty on 24-8-19.
//

#include "PianoKeyboardView.h"

#include "PianoPaintUtils.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "Utils/Log.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QWheelEvent>
#include <QEnterEvent>
#include <QMouseEvent>

PianoKeyboardView::PianoKeyboardView(QWidget *parent) : QWidget(parent) {
    setFixedWidth(ClipEditorGlobal::pianoKeyboardWidth);
    setMouseTracking(true);
}

void PianoKeyboardView::setKeyRange(const double top, const double bottom) {
    m_top = top;
    m_bottom = bottom;
    update();
}

QColor PianoKeyboardView::whiteKeyColor() const {
    return m_whiteKeyColor;
}

void PianoKeyboardView::setWhiteKeyColor(const QColor &whiteKeyColor) {
    m_whiteKeyColor = whiteKeyColor;
    update();
}

QColor PianoKeyboardView::blackKeyColor() const {
    return m_blackKeyColor;
}

void PianoKeyboardView::setBlackKeyColor(const QColor &blackKeyColor) {
    m_blackKeyColor = blackKeyColor;
    update();
}

QColor PianoKeyboardView::dividerColor() const {
    return m_dividerColor;
}

void PianoKeyboardView::setDividerColor(const QColor &dividerColor) {
    m_dividerColor = dividerColor;
    update();
}

void PianoKeyboardView::paintEvent(QPaintEvent *event) {
    QElapsedTimer mstimer;
    mstimer.start();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (m_style == Uniform) {
        drawUniformKeyboard(painter);
        if (m_hoveredKeyIndex >= 0) {
            drawHoverOverlay(painter);
        }
    } else {
        drawClassicKeyboard(painter);
        if (m_hoveredKeyIndex >= 0 && !PianoPaintUtils::isWhiteKey(m_hoveredKeyIndex)) {
            drawHoverOverlay(painter);
        }
    }

    // const auto time = static_cast<double>(mstimer.nsecsElapsed()) / 1000000.0;
    // Logger::d("PianoKeyboardView", "paint time: " + QString::number(time));
}

void PianoKeyboardView::wheelEvent(QWheelEvent *e) {
    const auto angleDelta = e->angleDelta().transposed();
    const auto event =
        new QWheelEvent(e->position(), e->globalPosition(), e->pixelDelta(), angleDelta,
                        e->buttons(), e->modifiers(), e->phase(), e->inverted());
    emit wheelScroll(event);
    // QWidget::wheelEvent(event);
}

void PianoKeyboardView::enterEvent(QEnterEvent *event) {
    QWidget::enterEvent(event);
}

void PianoKeyboardView::setHoveredKeyIndex(int keyIndex) {
    if (keyIndex != m_hoveredKeyIndex) {
        m_hoveredKeyIndex = keyIndex;
        update();
    }
}

void PianoKeyboardView::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    if (m_hoveredKeyIndex != -1) {
        m_hoveredKeyIndex = -1;
        update();
    }
}

void PianoKeyboardView::mouseMoveEvent(QMouseEvent *event) {
    QWidget::mouseMoveEvent(event);
    const auto x = event->position().x();
    const auto y = event->position().y();
    const int keyIndex = xyToKeyIndex(x, y);
    if (keyIndex != m_hoveredKeyIndex) {
        m_hoveredKeyIndex = keyIndex;
        update();
    }
}

int PianoKeyboardView::yToKeyIndex(double y) const {
    if (y < 0 || y > height())
        return -1;

    if (m_style == Uniform) {
        const auto ratio = y / height();
        const auto keyIndex = m_top - ratio * (m_top - m_bottom);
        return static_cast<int>(keyIndex);
    } else {
        const auto pixelsPerBlackKey = height() / (m_top - m_bottom);
        const auto pixelsPerWhiteKey = pixelsPerBlackKey * 12 / 7;

        auto blackKeyToY = [this](const int key) {
            const auto ratio = (key - m_top) / (m_bottom - m_top);
            const auto y = height() * ratio;
            return y;
        };

        auto closestBKeyY = [=](const int key) {
            const int remain = key % 12;
            const int bIndex = key + 12 - remain - 1;
            return blackKeyToY(bIndex);
        };

        auto whiteKeyToY = [=](const int key) {
            const int remain = key % 12;
            int index = 0;
            if (remain == 0)
                index = 0;
            else if (remain == 2)
                index = 1;
            else if (remain == 4)
                index = 2;
            else if (remain == 5)
                index = 3;
            else if (remain == 7)
                index = 4;
            else if (remain == 9)
                index = 5;
            else if (remain == 11)
                index = 6;

            const auto y = closestBKeyY(key) + (6 - index) * pixelsPerWhiteKey;
            return y;
        };

        const auto prevKeyIndex = static_cast<int>(m_top) + 1;

        for (int i = prevKeyIndex; i > m_bottom; i--) {
            if (PianoPaintUtils::isWhiteKey(i))
                continue;

            const auto keyY = blackKeyToY(i);
            if (y >= keyY && y < keyY + pixelsPerBlackKey) {
                return i;
            }
        }

        const auto prevWhiteKeyIndex =
            PianoPaintUtils::isWhiteKey(prevKeyIndex) ? prevKeyIndex : prevKeyIndex + 1;
        for (int i = prevWhiteKeyIndex; i > m_bottom - 1; i--) {
            if (!PianoPaintUtils::isWhiteKey(i))
                continue;

            const auto keyY = whiteKeyToY(i);
            if (y >= keyY && y < keyY + pixelsPerWhiteKey) {
                return i;
            }
        }

        const auto ratio = y / height();
        const auto approximateKey = m_top - ratio * (m_top - m_bottom);
        return static_cast<int>(approximateKey);
    }
}

int PianoKeyboardView::xyToKeyIndex(double x, double y) const {
    if (y < 0 || y > height())
        return -1;

    if (m_style == Uniform) {
        // Uniform style doesn't need x coordinate
        return yToKeyIndex(y);
    } else {
        // Classic style: first check x coordinate, then y coordinate
        const auto blackKeyWidth = width() / 5.0 * 3;
        const auto pixelsPerBlackKey = height() / (m_top - m_bottom);
        const auto pixelsPerWhiteKey = pixelsPerBlackKey * 12 / 7;

        auto blackKeyToY = [this](const int key) {
            const auto ratio = (key - m_top) / (m_bottom - m_top);
            const auto y = height() * ratio;
            return y;
        };

        auto closestBKeyY = [=](const int key) {
            const int remain = key % 12;
            const int bIndex = key + 12 - remain - 1;
            return blackKeyToY(bIndex);
        };

        auto whiteKeyToY = [=](const int key) {
            const int remain = key % 12;
            int index = 0;
            if (remain == 0)
                index = 0;
            else if (remain == 2)
                index = 1;
            else if (remain == 4)
                index = 2;
            else if (remain == 5)
                index = 3;
            else if (remain == 7)
                index = 4;
            else if (remain == 9)
                index = 5;
            else if (remain == 11)
                index = 6;

            const auto y = closestBKeyY(key) + (6 - index) * pixelsPerWhiteKey;
            return y;
        };

        const auto prevKeyIndex = static_cast<int>(m_top) + 1;

        // First check if x is within black key width range
        // If yes, check if y falls on a black key
        if (x >= 0 && x < blackKeyWidth) {
            for (int i = prevKeyIndex; i > m_bottom; i--) {
                if (PianoPaintUtils::isWhiteKey(i))
                    continue;

                const auto keyY = blackKeyToY(i);
                if (y >= keyY && y < keyY + pixelsPerBlackKey) {
                    return i;
                }
            }
        }

        // If x is outside black key width, or y doesn't fall on any black key,
        // check white keys
        const auto prevWhiteKeyIndex =
            PianoPaintUtils::isWhiteKey(prevKeyIndex) ? prevKeyIndex : prevKeyIndex + 1;
        for (int i = prevWhiteKeyIndex; i > m_bottom - 1; i--) {
            if (!PianoPaintUtils::isWhiteKey(i))
                continue;

            const auto keyY = whiteKeyToY(i);
            if (y >= keyY && y < keyY + pixelsPerWhiteKey) {
                return i;
            }
        }

        // Fallback: approximate based on y coordinate
        const auto ratio = y / height();
        const auto approximateKey = m_top - ratio * (m_top - m_bottom);
        return static_cast<int>(approximateKey);
    }
}

void PianoKeyboardView::drawUniformKeyboard(QPainter &painter) const {
    QPen pen;
    pen.setWidthF(penWidth);

    const auto pixelsPerKey = height() / (m_top - m_bottom);
    const auto prevKeyIndex = static_cast<int>(m_top) + 1;
    bool prevIsWhiteKey = false;

    auto keyToY = [this](const int key) {
        const auto ratio = (key - m_top) / (m_bottom - m_top);
        const auto y = height() * ratio;
        return y;
    };

    for (int i = prevKeyIndex; i > m_bottom; i--) {
        constexpr auto radius = 4.0;
        const auto y = keyToY(i);

        const auto isWhiteKey = PianoPaintUtils::isWhiteKey(i);
        painter.setBrush(isWhiteKey ? m_whiteKeyColor : m_blackKeyColor);
        auto keyRect = QRectF(-radius, y, width() + radius, pixelsPerKey);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(keyRect, radius, radius);

        if (i % 12 == 0) {
            auto textRect = QRectF(6, y, width(), pixelsPerKey);
            auto fontMetrics = painter.fontMetrics();
            const auto textHeight = fontMetrics.height();
            if (textRect.height() > textHeight) {
                pen.setColor(isWhiteKey ? m_blackKeyColor : m_whiteKeyColor);
                painter.setPen(pen);
                painter.drawText(textRect, PianoPaintUtils::noteName(i),
                                 QTextOption(Qt::AlignVCenter));
            }
        }

        if (prevIsWhiteKey && isWhiteKey) {
            pen.setColor(m_dividerColor);
            painter.setPen(pen);
            auto line = QLineF(0, y, width() - radius + 1, y);
            painter.drawLine(line);
        }

        prevIsWhiteKey = isWhiteKey;
    }
}

void PianoKeyboardView::drawClassicKeyboard(QPainter &painter) {
    constexpr auto radius = 4.0;
    const auto pixelsPerBlackKey = height() / (m_top - m_bottom);
    const auto pixelsPerWhiteKey = pixelsPerBlackKey * 12 / 7;
    const auto prevKeyIndex = static_cast<int>(m_top) + 1;
    const auto prevWhiteKeyIndex =
        PianoPaintUtils::isWhiteKey(prevKeyIndex) ? prevKeyIndex : prevKeyIndex + 1;

    auto blackKeyToY = [this](const int key) {
        const auto ratio = (key - m_top) / (m_bottom - m_top);
        const auto y = height() * ratio;
        return y;
    };

    auto closestBKeyY = [=](const int key) {
        const int remain = key % 12;
        const int bIndex = key + 12 - remain - 1;
        // qDebug() << "bIndex" << bIndex << "Note name: " << PianoPaintUtils::noteName(bIndex);
        return blackKeyToY(bIndex);
    };

    auto whiteKeyToY = [=](const int key) {
        const int remain = key % 12;
        int index = 0;
        if (remain == 0)
            index = 0;
        else if (remain == 2)
            index = 1;
        else if (remain == 4)
            index = 2;
        else if (remain == 5)
            index = 3;
        else if (remain == 7)
            index = 4;
        else if (remain == 9)
            index = 5;
        else if (remain == 11)
            index = 6;

        const auto y = closestBKeyY(key) + (6 - index) * pixelsPerWhiteKey;
        // qDebug() << "closestBKeyY: " << y;
        return y;
    };

    QPen pen;
    pen.setWidthF(penWidth);

    auto drawWhiteKeys = [&] {
        for (int i = prevWhiteKeyIndex; i > m_bottom - 1; i--) {
            if (!PianoPaintUtils::isWhiteKey(i))
                continue;

            const auto y = whiteKeyToY(i);
            painter.setBrush(m_whiteKeyColor);
            auto keyRect = QRectF(-radius, y, width() + radius, pixelsPerWhiteKey);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(keyRect, radius, radius);

            pen.setColor(m_dividerColor);
            painter.setPen(pen);
            auto line = QLineF(0, y, width() - radius + 1, y);
            painter.drawLine(line);

            if (i % 12 == 0) {
                auto textRect = QRectF(6, y, width() - radius - 6, pixelsPerWhiteKey);
                auto fontMetrics = painter.fontMetrics();
                const auto textHeight = fontMetrics.height();
                if (textRect.height() > textHeight) {
                    pen.setColor(m_blackKeyColor);
                    painter.setPen(pen);
                    painter.drawText(textRect, PianoPaintUtils::noteName(i),
                                     QTextOption(Qt::AlignVCenter | Qt::AlignRight));
                }
            }
        }
    };

    auto drawBlackKeys = [&] {
        for (int i = prevKeyIndex; i > m_bottom; i--) {
            if (PianoPaintUtils::isWhiteKey(i))
                continue;

            const auto y = blackKeyToY(i);
            painter.setBrush(m_blackKeyColor);
            auto keyRect = QRectF(-radius, y, width() / 5.0 * 3 + radius, pixelsPerBlackKey);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(keyRect, radius, radius);

            // auto textRect = QRectF(6, y, width(), pixelsPerBlackKey);
            // auto fontMetrics = painter.fontMetrics();
            // auto textHeight = fontMetrics.height();
            // if (textRect.height() > textHeight) {
            //     pen.setColor(m_whiteKeyColor);
            //     painter.setPen(pen);
            //     painter.drawText(textRect, PianoPaintUtils::noteName(i),
            //                      QTextOption(Qt::AlignVCenter));
            // }
        }
    };

    drawWhiteKeys();

    if (m_hoveredKeyIndex >= 0 && PianoPaintUtils::isWhiteKey(m_hoveredKeyIndex)) {
        constexpr auto radius = 4.0;
        constexpr auto overlayAlpha = 128;
        QColor overlayColor = m_primaryColor;
        overlayColor.setAlpha(overlayAlpha);
        painter.setBrush(overlayColor);
        painter.setPen(Qt::NoPen);

        const auto y = whiteKeyToY(m_hoveredKeyIndex);
        auto keyRect = QRectF(-radius, y, width() + radius, pixelsPerWhiteKey);
        painter.drawRoundedRect(keyRect, radius, radius);
    }

    drawBlackKeys();
}

void PianoKeyboardView::drawHoverOverlay(QPainter &painter) const {
    if (m_hoveredKeyIndex < 0)
        return;

    constexpr auto radius = 4.0;
    constexpr auto overlayAlpha = 128;
    QColor overlayColor = m_primaryColor;
    overlayColor.setAlpha(overlayAlpha);
    painter.setBrush(overlayColor);
    painter.setPen(Qt::NoPen);

    if (m_style == Uniform) {
        const auto pixelsPerKey = height() / (m_top - m_bottom);
        auto keyToY = [this](const int key) {
            const auto ratio = (key - m_top) / (m_bottom - m_top);
            const auto y = height() * ratio;
            return y;
        };

        const auto y = keyToY(m_hoveredKeyIndex);
        auto keyRect = QRectF(-radius, y, width() + radius, pixelsPerKey);
        painter.drawRoundedRect(keyRect, radius, radius);
    } else {
        const auto pixelsPerBlackKey = height() / (m_top - m_bottom);
        const auto pixelsPerWhiteKey = pixelsPerBlackKey * 12 / 7;

        auto blackKeyToY = [this](const int key) {
            const auto ratio = (key - m_top) / (m_bottom - m_top);
            const auto y = height() * ratio;
            return y;
        };

        auto closestBKeyY = [=](const int key) {
            const int remain = key % 12;
            const int bIndex = key + 12 - remain - 1;
            return blackKeyToY(bIndex);
        };

        auto whiteKeyToY = [=](const int key) {
            const int remain = key % 12;
            int index = 0;
            if (remain == 0)
                index = 0;
            else if (remain == 2)
                index = 1;
            else if (remain == 4)
                index = 2;
            else if (remain == 5)
                index = 3;
            else if (remain == 7)
                index = 4;
            else if (remain == 9)
                index = 5;
            else if (remain == 11)
                index = 6;

            const auto y = closestBKeyY(key) + (6 - index) * pixelsPerWhiteKey;
            return y;
        };

        const bool isWhiteKey = PianoPaintUtils::isWhiteKey(m_hoveredKeyIndex);
        if (isWhiteKey) {
            const auto y = whiteKeyToY(m_hoveredKeyIndex);
            auto keyRect = QRectF(-radius, y, width() + radius, pixelsPerWhiteKey);
            painter.drawRoundedRect(keyRect, radius, radius);
        } else {
            const auto y = blackKeyToY(m_hoveredKeyIndex);
            auto keyRect = QRectF(-radius, y, width() / 5.0 * 3 + radius, pixelsPerBlackKey);
            painter.drawRoundedRect(keyRect, radius, radius);
        }
    }
}