#include "PianoKeyboardView.h"

#include "UI/Controls/PianoPaintUtils.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/MidiSystem.h"

#include <TalcsMidi/MidiMessage.h>
#include <TalcsMidi/MidiMessageIntegrator.h>

#include <QPainter>

namespace {
    // Rounded corner of the key bed; the rects overhang the widget edges by this
    // much so the rounding only shows on the inner side.
    constexpr auto keyRadius = 4.0;
    constexpr auto hoverAlpha = 128;
    constexpr auto pressedAlpha = 200;
}

PianoKeyboardView::PianoKeyboardView(QWidget *parent) : PianoKeyboardBase(parent) {
    setFixedWidth(ClipEditorGlobal::pianoKeyboardWidth);
}

double PianoKeyboardView::pixelsPerBlackKey() const {
    return height() / (m_top - m_bottom);
}

double PianoKeyboardView::pixelsPerWhiteKey() const {
    return pixelsPerBlackKey() * 12 / 7;
}

double PianoKeyboardView::blackKeyY(const double key) const {
    return height() * (key - m_top) / (m_bottom - m_top);
}

double PianoKeyboardView::closestBlackKeyY(const double key) const {
    const auto remain = static_cast<int>(key) % 12;
    const auto bIndex = key + 12 - remain - 1;
    return blackKeyY(bIndex);
}

double PianoKeyboardView::whiteKeyY(const double key) const {
    static constexpr int whiteIndexInOctave[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    const auto index = qMax(whiteIndexInOctave[static_cast<int>(key) % 12], 0);
    return closestBlackKeyY(key) + (6 - index) * pixelsPerWhiteKey();
}

void PianoKeyboardView::paintKeyboard(QPainter &painter) const {
    const auto prevKeyIndex = static_cast<int>(m_top) + 1;
    const auto prevWhiteKeyIndex =
        PianoPaintUtils::isWhiteKey(prevKeyIndex) ? prevKeyIndex : prevKeyIndex + 1;

    QPen pen;
    pen.setWidthF(penWidth);

    for (int i = prevWhiteKeyIndex; i > m_bottom - 1; i--) {
        if (!PianoPaintUtils::isWhiteKey(i))
            continue;

        const auto y = whiteKeyY(i);
        painter.setBrush(m_whiteKeyColor);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(-keyRadius, y, width() + keyRadius, pixelsPerWhiteKey()),
                                keyRadius, keyRadius);

        pen.setColor(m_dividerColor);
        painter.setPen(pen);
        painter.drawLine(QLineF(0, y, width() - keyRadius + 1, y));
    }

    // White-key highlights stay under the black keys, so an overlapping black
    // key keeps its own colour.
    if (m_hoveredKeyIndex >= 0 && PianoPaintUtils::isWhiteKey(m_hoveredKeyIndex) &&
        m_hoveredKeyIndex != m_pressedKeyIndex)
        drawWhiteKeyOverlay(painter, m_hoveredKeyIndex, hoverAlpha);
    if (m_pressedKeyIndex >= 0 && PianoPaintUtils::isWhiteKey(m_pressedKeyIndex))
        drawWhiteKeyOverlay(painter, m_pressedKeyIndex, pressedAlpha);

    for (int i = prevKeyIndex; i > m_bottom; i--) {
        if (PianoPaintUtils::isWhiteKey(i))
            continue;

        const auto y = blackKeyY(i);
        painter.setBrush(m_blackKeyColor);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(
            QRectF(-keyRadius, y, width() / 5.0 * 3 + keyRadius, pixelsPerBlackKey()), keyRadius,
            keyRadius);
    }

    for (int i = prevWhiteKeyIndex; i > m_bottom - 1; i--) {
        if (!PianoPaintUtils::isWhiteKey(i) || i % 12 != 0)
            continue;

        const auto y = whiteKeyY(i);
        auto textRect = QRectF(6, y, width() - keyRadius - 6, pixelsPerWhiteKey());
        const auto fontMetrics = painter.fontMetrics();
        if (textRect.height() > fontMetrics.height()) {
            pen.setColor(m_blackKeyColor);
            painter.setPen(pen);
            painter.drawText(textRect, PianoPaintUtils::noteName(i),
                             QTextOption(Qt::AlignVCenter | Qt::AlignRight));
        }
    }

    // Black-key highlights go last: they have to cover the labels and the white
    // keys they overlap.
    if (m_hoveredKeyIndex >= 0 && !PianoPaintUtils::isWhiteKey(m_hoveredKeyIndex) &&
        m_hoveredKeyIndex != m_pressedKeyIndex)
        drawBlackKeyOverlay(painter, m_hoveredKeyIndex, hoverAlpha);
    if (m_pressedKeyIndex >= 0 && !PianoPaintUtils::isWhiteKey(m_pressedKeyIndex))
        drawBlackKeyOverlay(painter, m_pressedKeyIndex, pressedAlpha);
}

void PianoKeyboardView::drawWhiteKeyOverlay(QPainter &painter, const int keyIndex,
                                            const int alpha) const {
    painter.setBrush(overlayColor(alpha));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(
        QRectF(-keyRadius, whiteKeyY(keyIndex), width() + keyRadius, pixelsPerWhiteKey()),
        keyRadius, keyRadius);
}

void PianoKeyboardView::drawBlackKeyOverlay(QPainter &painter, const int keyIndex,
                                            const int alpha) const {
    painter.setBrush(overlayColor(alpha));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(
        QRectF(-keyRadius, blackKeyY(keyIndex), width() / 5.0 * 3 + keyRadius, pixelsPerBlackKey()),
        keyRadius, keyRadius);
}

int PianoKeyboardView::keyAtPosition(const QPointF &pos) const {
    const auto x = pos.x();
    const auto y = pos.y();
    if (y < 0 || y > height())
        return -1;

    const auto prevKeyIndex = static_cast<int>(m_top) + 1;
    const auto blackKeyWidth = width() / 5.0 * 3;

    // Black keys only occupy the leading part of the row; a point beyond that
    // belongs to the white key underneath.
    if (x >= 0 && x < blackKeyWidth) {
        for (int i = prevKeyIndex; i > m_bottom; i--) {
            if (PianoPaintUtils::isWhiteKey(i))
                continue;
            const auto keyY = blackKeyY(i);
            if (y >= keyY && y < keyY + pixelsPerBlackKey())
                return i;
        }
    }

    const auto prevWhiteKeyIndex =
        PianoPaintUtils::isWhiteKey(prevKeyIndex) ? prevKeyIndex : prevKeyIndex + 1;
    for (int i = prevWhiteKeyIndex; i > m_bottom - 1; i--) {
        if (!PianoPaintUtils::isWhiteKey(i))
            continue;
        const auto keyY = whiteKeyY(i);
        if (y >= keyY && y < keyY + pixelsPerWhiteKey())
            return i;
    }

    // Fractional ranges leave slivers between the last key and the edge.
    const auto ratio = y / height();
    return static_cast<int>(m_top - ratio * (m_top - m_bottom));
}

void PianoKeyboardView::playNoteOn(const int key) {
    auto integrator = AudioSystem::midiSystem()->integrator();
    if (integrator)
        integrator->messageCallback(talcs::MidiMessage::noteOn(1, key, 0.8f));
}

void PianoKeyboardView::playNoteOff(const int key) {
    auto integrator = AudioSystem::midiSystem()->integrator();
    if (integrator)
        integrator->messageCallback(talcs::MidiMessage::noteOff(1, key));
}
