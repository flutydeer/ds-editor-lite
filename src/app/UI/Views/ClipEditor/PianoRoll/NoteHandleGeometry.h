#ifndef NOTEHANDLEGEOMETRY_H
#define NOTEHANDLEGEOMETRY_H

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/EditorItemGeometry.h"
#include "UI/Views/Common/EditorResizeUtils.h"

#include <QPointF>
#include <QRectF>

#include <algorithm>

// The resize handle frame on the selected note.
//
// Shape contract: the frame is a **ring** — a rounded rectangle with a note-sized
// hole punched out of the middle. The hole's four edges coincide with the note's
// visible edges, so the whole ring band lies outside the note and only the outer
// edge gets a thin outline (the inner edge is not stroked; the note's own border
// finishes it).
//
// **The band is not equally thick on all four sides**: the left/right vertical
// bands are the finger's resize handles and are far thicker than the top/bottom
// horizontal bands (the reference mock does the same: the vertical bars are
// clearly heavier than the horizontal lines). The horizontal bands only close the
// frame and should reach into the neighbouring rows as little as possible.
//
// Coordinate system: the modelRect the caller passes is the note's model rectangle
// (Legacy: item coordinates of NoteView::rect(); RHI: viewport logical pixels), and
// both are device-independent pixels, so the constants here need no scaling. The
// scale parameter is used the same way as EditorItemGeometry::notePaintRect — it
// only scales the constants proportionally, and the rectangle must already be in
// that same unit (RHI painting passes the dpr-multiplied physical rectangle, hit
// testing passes the logical one).
namespace NoteHandleGeometry {
    // Thickness of the left/right vertical bands — the finger's handle width
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

    // The note's visible rectangle — the same inset the body itself is painted with
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

    // Hit testing: whether the point falls on the **left/right vertical bands**.
    //
    // The top/bottom horizontal bands only close the frame and take no part in hit
    // testing. They span across the note; if they counted as a hit, a finger
    // resting on the band right above or below the note would hit this note while
    // the press position resolves to the neighbouring row, and the first drag move
    // would jump the note a whole row. The vertical bands' horizontal range falls
    // inside the resize grab tolerance anyway, so a hit there always resolves to a
    // left/right resize — no such problem.
    inline bool sideBandContains(const QRectF &modelRect, const QPointF &position,
                                 const double scale = 1.0) {
        const auto outer = outerRect(modelRect, scale);
        const auto inner = innerRect(modelRect, scale);
        if (outer.isEmpty() || !outer.contains(position))
            return false;
        return position.x() <= inner.left() || position.x() >= inner.right();
    }

    // Resize hit: the grab zone is "vertical band area (outside the note) ∪ the
    // existing tolerance area (inside the note)". With framesActive false (mouse,
    // pen, or multi-select) this degrades to the original pure rectangle-edge test,
    // point-for-point identical to before the change.
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
