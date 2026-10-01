#include "NoteHandleOverlay.h"

#include "NoteHandleGeometry.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

NoteHandleOverlay::NoteHandleOverlay() {
    setAcceptedMouseButtons(Qt::NoButton);
}

void NoteHandleOverlay::setNoteSceneRect(const QRectF &sceneRect) {
    const auto normalized = sceneRect.normalized();
    const auto next = normalized.isEmpty() ? QRectF() : normalized;
    if (next == m_noteRect)
        return;
    prepareGeometryChange();
    m_noteRect = next;
    update();
}

QRectF NoteHandleOverlay::noteSceneRect() const {
    return m_noteRect;
}

bool NoteHandleOverlay::frameContains(const QPointF &scenePos) const {
    return NoteHandleGeometry::sideBandContains(m_noteRect, scenePos);
}

QColor NoteHandleOverlay::fillColor() const {
    return m_fillColor;
}

void NoteHandleOverlay::setFillColor(const QColor &color) {
    if (m_fillColor == color)
        return;
    m_fillColor = color;
    update();
}

QColor NoteHandleOverlay::borderColor() const {
    return m_borderColor;
}

void NoteHandleOverlay::setBorderColor(const QColor &color) {
    if (m_borderColor == color)
        return;
    m_borderColor = color;
    update();
}

QColor NoteHandleOverlay::gripColor() const {
    return m_gripColor;
}

void NoteHandleOverlay::setGripColor(const QColor &color) {
    if (m_gripColor == color)
        return;
    m_gripColor = color;
    update();
}

QRectF NoteHandleOverlay::boundingRect() const {
    const auto outer = NoteHandleGeometry::outerRect(m_noteRect);
    if (outer.isEmpty())
        return {};
    // The thin outer outline is centered on the ring's outer edge, so half of it
    // reaches outside
    const auto pad = NoteHandleGeometry::outlineWidth;
    return outer.adjusted(-pad, -pad, pad, pad);
}

void NoteHandleOverlay::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                              QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)

    const auto outer = NoteHandleGeometry::outerRect(m_noteRect);
    const auto inner = NoteHandleGeometry::innerRect(m_noteRect);
    if (outer.isEmpty() || inner.isEmpty())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // A note-sized hole punched out of the rounded rectangle: the two contours
    // combine into one even-odd filled path, shaping the whole band in one pass.
    const auto outerRadius = NoteHandleGeometry::outerRadius(m_noteRect);
    const auto innerRadius = NoteHandleGeometry::innerRadius(m_noteRect);
    QPainterPath ring;
    ring.setFillRule(Qt::OddEvenFill);
    ring.addRoundedRect(outer, outerRadius, outerRadius);
    ring.addRoundedRect(inner, innerRadius, innerRadius);
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_fillColor);
    painter->drawPath(ring);

    // Stroke only the outer edge; the inner edge is finished by the note's own border
    const auto inset = NoteHandleGeometry::outlineWidth * 0.5;
    QPen outlinePen(m_borderColor);
    outlinePen.setWidthF(NoteHandleGeometry::outlineWidth);
    painter->setPen(outlinePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(outer.adjusted(inset, inset, -inset, -inset),
                             outerRadius - inset, outerRadius - inset);

    // The grip indicator line centered in each vertical band
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_gripColor);
    for (const auto right : {false, true}) {
        const auto grip = NoteHandleGeometry::gripRect(m_noteRect, right);
        if (!grip.isEmpty())
            painter->drawRect(grip);
    }

    painter->restore();
}

void NoteHandleOverlay::afterSetScale() {
    update();
}

void NoteHandleOverlay::afterSetVisibleRect() {
    update();
}
