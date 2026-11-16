#ifndef NOTEHANDLEGEOMETRY_H
#define NOTEHANDLEGEOMETRY_H

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/EditorItemGeometry.h"
#include "UI/Views/Common/EditorResizeUtils.h"

#include <QPointF>
#include <QRectF>

#include <algorithm>

// The handle frame is a ring whose inner edges meet the note's visible edges.
// Only the outer edge is stroked. The vertical bands are wide enough to grab,
// and the horizontal bands stay thin to avoid covering adjacent rows.
//
// Supply the note's model rectangle in the target coordinate system.
// scale adjusts the constants only: use 1 for logical pixels, or use the
// device pixel ratio with a rectangle already converted to physical pixels.
namespace NoteHandleGeometry {
    // Thickness of the left/right vertical bands - the finger's handle width
    inline constexpr double sideBandWidth = 16.0;
    // Thickness of the top/bottom horizontal bands. They only close the frame, so
    // they are kept thin to minimize how far they press onto neighbouring rows
    inline constexpr double capBandWidth = 2.0;
    // The thin outline drawn only along the outer edge
    inline constexpr double outlineWidth = 1.0;
    // The grip indicator line centered in each vertical band: 1dp wide, one third
    // of the ring's height, centered both horizontally and vertically
    inline constexpr double gripWidth = 1.0;
    inline constexpr double gripHeightRatio = 1.0 / 3.0;
    // How far the grab zone expands beyond the model rectangle. The ring's outer
    // edge still sits the note's own border inset away from the model rectangle,
    // so this takes the vertical band thickness: the outer edge plus a bit of
    // finger slack.
    inline constexpr double grabExpansion = sideBandWidth;

    // The note's visible rectangle - the same inset the body itself is painted with
    inline QRectF visualRect(const QRectF &modelRect, const double scale = 1.0) {
        if (scale <= 0.0)
            return {};
        return EditorItemGeometry::notePaintRect(modelRect, scale);
    }

    // The hole: exactly the note's visible rectangle, so the hole's edge lands
    // right on the note's edge
    inline QRectF innerRect(const QRectF &modelRect, const double scale = 1.0) {
        return visualRect(modelRect, scale);
    }

    // The ring's outer edge: the hole expanded by one vertical band on the left and
    // right, and by one horizontal band on the top and bottom. An empty rectangle
    // means this note cannot draw a ring.
    inline QRectF outerRect(const QRectF &modelRect, const double scale = 1.0) {
        const auto inner = innerRect(modelRect, scale);
        if (inner.isEmpty())
            return {};
        const auto side = sideBandWidth * scale;
        const auto cap = capBandWidth * scale;
        return inner.adjusted(-side, -cap, side, cap);
    }

    // The hole's corner radius matches the note's own, so the band never leaves a
    // notch at the note's corners
    inline double innerRadius(const QRectF &modelRect, const double scale = 1.0) {
        const auto inner = innerRect(modelRect, scale);
        if (inner.isEmpty())
            return 0.0;
        return EditorItemGeometry::adaptiveCornerRadius(
            inner, EditorItemGeometry::noteCornerRadius * scale);
    }

    // The outer radius grows by the **horizontal band** thickness: the thin
    // top/bottom bands keep their thickness around the corners, and the outer ends
    // of the vertical bands get a slightly rounded cap.
    inline double outerRadius(const QRectF &modelRect, const double scale = 1.0) {
        return innerRadius(modelRect, scale) + capBandWidth * scale;
    }

    // The grip indicator line centered in one vertical band; right selects the
    // right band. Returns an empty rectangle when the ring cannot be drawn.
    inline QRectF gripRect(const QRectF &modelRect, const bool right, const double scale = 1.0) {
        const auto outer = outerRect(modelRect, scale);
        const auto inner = innerRect(modelRect, scale);
        if (outer.isEmpty() || inner.isEmpty())
            return {};
        const auto bandCenterX = right ? (inner.right() + outer.right()) * 0.5
                                       : (outer.left() + inner.left()) * 0.5;
        const auto width = gripWidth * scale;
        const auto height = outer.height() * gripHeightRatio;
        return {bandCenterX - width * 0.5, outer.center().y() - height * 0.5, width, height};
    }

    // Note tools: with these tools a click selects a note and moving/resizing is
    // allowed, so the ring is worth drawing. RHI's noteEditingEnabled() keeps the
    // same set.
    inline bool toolAllowsHandles(const EditorViewGlobal::PianoRollEditMode mode) {
        return mode == EditorViewGlobal::Select || mode == EditorViewGlobal::IntervalSelect ||
               mode == EditorViewGlobal::DrawNote;
    }

    // Ring visibility contract: the ring appears only for touch (a finger has no
    // hover cursor), when exactly one note is selected, the current tool is a note
    // tool, and that note has no inline lyric editing open.
    inline bool frameVisible(const bool touchAffordance, const int selectedNoteCount,
                             const EditorViewGlobal::PianoRollEditMode mode,
                             const bool inlineEditing) {
        return touchAffordance && selectedNoteCount == 1 && !inlineEditing &&
               toolAllowsHandles(mode);
    }

    // Hit only the vertical bands. A hit on a horizontal band could select this note
    // from an adjacent pitch row and move it by a semitone when the drag begins.
    inline bool sideBandContains(const QRectF &modelRect, const QPointF &position,
                                 const double scale = 1.0) {
        const auto outer = outerRect(modelRect, scale);
        const auto inner = innerRect(modelRect, scale);
        if (outer.isEmpty() || !outer.contains(position))
            return false;
        return position.x() <= inner.left() || position.x() >= inner.right();
    }

    // The resize target includes the inner edge tolerance and, when a touch frame is
    // active, the vertical band outside the note.
    inline EditorResizeUtils::HorizontalEdge resizeEdgeAt(const QPointF &localPos,
                                                          const QRectF &modelRect,
                                                          const double tolerance,
                                                          const bool framesActive,
                                                          const double scale = 1.0) {
        const auto expansion = framesActive ? grabExpansion * scale : 0.0;
        return EditorResizeUtils::horizontalEdgeAt(localPos.x() - modelRect.left(),
                                                   modelRect.width(), tolerance, expansion);
    }
}

#endif // NOTEHANDLEGEOMETRY_H
