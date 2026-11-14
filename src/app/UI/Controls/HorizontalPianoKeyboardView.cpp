#include "HorizontalPianoKeyboardView.h"

#include "UI/Controls/PianoPaintUtils.h"

#include <QFontMetrics>
#include <QPainter>

namespace {
    // C2 - C6: five octaves of key bed, the last one closing on a single C.
    constexpr int lowestKey = 36;
    constexpr int highestKey = 84;
    constexpr int whiteKeyWidth = 20;
    constexpr int keyboardHeight = 64;
    constexpr double blackKeyWidthRatio = 0.6;
    constexpr int blackKeyHeight = 40;
    constexpr auto keyRadius = 2.0;
    constexpr auto hoverAlpha = 128;
    constexpr auto pressedAlpha = 200;
    // Space the note name needs around it before it is worth drawing.
    constexpr int labelPadding = 4;
    constexpr int minLabelPixelSize = 8;
}

HorizontalPianoKeyboardView::HorizontalPianoKeyboardView(QWidget *parent)
    : PianoKeyboardBase(parent) {
    // The 29 white keys fit a normal options card, so the bed keeps its natural
    // size instead of stretching with the layout. The owner puts it in a scroll
    // area to cover windows too narrow for it.
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setKeyRange(highestKey, lowestKey);
}

QSize HorizontalPianoKeyboardView::sizeHint() const {
    return {static_cast<int>(totalWidth()), keyboardHeight};
}

QSize HorizontalPianoKeyboardView::minimumSizeHint() const {
    return sizeHint();
}

void HorizontalPianoKeyboardView::keyRangeChanged() {
    rebuildLayout();
}

void HorizontalPianoKeyboardView::rebuildLayout() {
    m_whiteKeys.clear();
    for (int key = static_cast<int>(m_bottom); key <= static_cast<int>(m_top); ++key) {
        if (PianoPaintUtils::isWhiteKey(key))
            m_whiteKeys.append(key);
    }

    m_blackKeysByBoundary = QList<int>(m_whiteKeys.size() + 1, -1);
    auto whiteSeen = 0;
    for (int key = static_cast<int>(m_bottom); key <= static_cast<int>(m_top); ++key) {
        if (PianoPaintUtils::isWhiteKey(key)) {
            ++whiteSeen;
            continue;
        }
        // A black key sits between the last white key below it and the first
        // one above it, which is the boundary whose index is the white count.
        if (whiteSeen > 0 && whiteSeen < m_whiteKeys.size())
            m_blackKeysByBoundary[whiteSeen] = key;
    }

    setFixedSize(sizeHint());
    updateGeometry();
}

int HorizontalPianoKeyboardView::whiteKeyCount() const {
    return static_cast<int>(m_whiteKeys.size());
}

double HorizontalPianoKeyboardView::totalWidth() const {
    return whiteKeyCount() * whiteKeyWidth;
}

int HorizontalPianoKeyboardView::blackKeyAtBoundary(const int boundary) const {
    if (boundary < 0 || boundary >= m_blackKeysByBoundary.size())
        return -1;
    return m_blackKeysByBoundary.at(boundary);
}

QRectF HorizontalPianoKeyboardView::whiteKeyRect(const int whiteIndex) const {
    constexpr auto keyWidth = static_cast<double>(whiteKeyWidth);
    return {whiteIndex * keyWidth, 0.0, keyWidth, static_cast<double>(height())};
}

QRectF HorizontalPianoKeyboardView::blackKeyRect(const int boundary) const {
    constexpr auto blackWidth = whiteKeyWidth * blackKeyWidthRatio;
    constexpr auto blackHeight = static_cast<double>(blackKeyHeight);
    const auto center = boundary * static_cast<double>(whiteKeyWidth);
    // Overhang the top edge by the corner radius so the rounding only shows on
    // the front corners: the key bed is cut off at the widget's top edge.
    return {center - blackWidth / 2, -keyRadius, blackWidth, blackHeight + keyRadius};
}

int HorizontalPianoKeyboardView::keyAtPosition(const QPointF &pos) const {
    if (m_whiteKeys.isEmpty())
        return -1;
    const auto x = pos.x();
    const auto y = pos.y();
    if (x < 0 || x >= totalWidth() || y < 0 || y > height())
        return -1;

    const auto whiteIndex = qBound(0, static_cast<int>(x / whiteKeyWidth), whiteKeyCount() - 1);
    if (y < blackKeyHeight) {
        // Black keys straddle a boundary, so the two neighbours of the white key
        // under the pointer are the only candidates.
        constexpr auto halfBlackWidth = whiteKeyWidth * blackKeyWidthRatio / 2;
        const auto leftBoundary = whiteIndex;
        if (const auto key = blackKeyAtBoundary(leftBoundary);
            key >= 0 && qAbs(x - whiteKeyRect(leftBoundary).left()) <= halfBlackWidth)
            return key;
        const auto rightBoundary = whiteIndex + 1;
        if (const auto key = blackKeyAtBoundary(rightBoundary);
            key >= 0 && qAbs(x - whiteKeyRect(rightBoundary).left()) <= halfBlackWidth)
            return key;
    }
    return m_whiteKeys.at(whiteIndex);
}

void HorizontalPianoKeyboardView::paintKeyboard(QPainter &painter) const {
    const auto bedHeight = static_cast<double>(height());
    QPen pen;
    pen.setWidthF(penWidth);

    painter.setPen(Qt::NoPen);
    painter.setBrush(m_whiteKeyColor);
    for (int i = 0; i < whiteKeyCount(); ++i)
        painter.drawRect(whiteKeyRect(i));

    pen.setColor(m_dividerColor);
    painter.setPen(pen);
    for (int i = 1; i < whiteKeyCount(); ++i) {
        const auto x = i * whiteKeyWidth;
        painter.drawLine(QLineF(x, 0, x, bedHeight));
    }

    // White-key tints stay under the black keys: a black key straddles two
    // white keys, and the tint must not paint over it.
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < whiteKeyCount(); ++i) {
        const auto alpha = keyTintAlpha(m_whiteKeys.at(i));
        if (alpha <= 0)
            continue;
        painter.setBrush(overlayColor(alpha));
        painter.drawRect(whiteKeyRect(i));
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(m_blackKeyColor);
    for (int boundary = 0; boundary < m_blackKeysByBoundary.size(); ++boundary) {
        if (m_blackKeysByBoundary.at(boundary) < 0)
            continue;
        painter.drawRoundedRect(blackKeyRect(boundary), keyRadius, keyRadius);
    }

    // Octave labels. A 20px white key is too narrow for the interface font, so
    // the label font shrinks one pixel at a time until the note name fits.
    const auto labelFits = [](const QFont &labelFont) {
        return QFontMetrics(labelFont).horizontalAdvance(QStringLiteral("C8")) + labelPadding <=
               whiteKeyWidth;
    };
    auto labelFont = font();
    auto labelPixelSize = labelFont.pixelSize() > 0 ? labelFont.pixelSize() : 12;
    labelFont.setPixelSize(labelPixelSize);
    while (labelPixelSize > minLabelPixelSize && !labelFits(labelFont))
        labelFont.setPixelSize(--labelPixelSize);
    painter.setFont(labelFont);
    if (labelFits(labelFont)) {
        for (int i = 0; i < whiteKeyCount(); ++i) {
            const auto key = m_whiteKeys.at(i);
            if (key % 12 != 0)
                continue;
            pen.setColor(m_blackKeyColor);
            painter.setPen(pen);
            painter.drawText(QRectF(i * whiteKeyWidth, 0, whiteKeyWidth, bedHeight - 4),
                             Qt::AlignHCenter | Qt::AlignBottom, PianoPaintUtils::noteName(key));
        }
    }

    // Black-key tints go last: they cover the labels and the white keys they
    // overlap.
    for (int boundary = 0; boundary < m_blackKeysByBoundary.size(); ++boundary) {
        const auto alpha = keyTintAlpha(m_blackKeysByBoundary.at(boundary));
        if (alpha <= 0)
            continue;
        painter.setBrush(overlayColor(alpha));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(blackKeyRect(boundary), keyRadius, keyRadius);
    }

    // Outer frame. The white keys and the card surface are too close in value
    // for the bed's edges to read on their own, so the frame closes the shape.
    pen.setColor(m_dividerColor);
    pen.setWidthF(penWidth);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(0.5, 0.5, width() - 1.0, bedHeight - 1.0));
}

int HorizontalPianoKeyboardView::keyTintAlpha(const int key) const {
    if (key < 0)
        return 0;
    // A mouse press wins over an external hold, which wins over hover.
    if (key == m_pressedKeyIndex || m_heldKeys.contains(key))
        return pressedAlpha;
    if (key == m_hoveredKeyIndex)
        return hoverAlpha;
    return 0;
}

void HorizontalPianoKeyboardView::playNoteOn(const int key) {
    emit noteOn(key);
}

void HorizontalPianoKeyboardView::playNoteOff(const int key) {
    emit noteOff(key);
}
