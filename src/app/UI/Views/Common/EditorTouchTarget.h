#ifndef EDITORTOUCHTARGET_H
#define EDITORTOUCHTARGET_H

#include <QPointF>

// What an editor view must provide for EditorTouchController to drive it.
//
// Both editor backends (the legacy TimeGraphicsView family and the RHI
// EditorRhiWidget family) implement this, which is what lets a single gesture
// layer cover the piano roll, the parameter editor and the arrangement view.
class EditorTouchTarget {
public:
    enum class BlankDragAction {
        // Hand the drag to the existing mouse state machine unchanged. The
        // active tool decides what happens (rubber band, draw curve, erase...).
        SyntheticMouse,
        // Blank area is empty canvas — dragging it scrolls the viewport.
        Pan,
    };

    virtual ~EditorTouchTarget();

    // Cancel any running scroll/zoom animation before a gesture takes over.
    virtual void stopTouchViewportAnimation() = 0;
    // Move the content with the finger: a positive delta drags content right
    // and down, which means the scroll offset goes the other way.
    virtual void panTouchViewportBy(const QPointF &deltaPixels) = 0;
    // Multiply the current scales, keeping `anchor` (widget coordinates) fixed.
    virtual void zoomTouchViewportBy(double horizontalFactor, double verticalFactor,
                                     const QPointF &anchor) = 0;

    // What sits under a finger. A touch may only drag or resize an object it
    // has already selected, otherwise every attempt to scroll the viewport
    // would nudge whatever note or clip the finger happened to land on. A tap
    // selects, and the drag after it moves.
    enum class ContentHit {
        // Blank canvas. touchBlankDragAction() decides what a drag does here.
        None,
        // An object is here, but this finger has to select it first. A drag
        // does whatever it would do over blank canvas, which is normally
        // scrolling.
        Unselected,
        // An object a finger may drag straight away, either because it is
        // already selected or because an explicitly picked tool owns it.
        Selected,
    };

    // What is under the point: nothing, an object to select, or an object to
    // drag. Anything that is not plain canvas counts as content.
    [[nodiscard]] virtual ContentHit touchContentAt(const QPointF &viewportPosition) const = 0;
    [[nodiscard]] virtual BlankDragAction touchBlankDragAction() const = 0;

    // Is there an editable object (note, clip, anchor...) under the point? The
    // long press asks this, because a context menu opens on any object the
    // finger can grab, selected or not.
    [[nodiscard]] bool touchHitsContent(const QPointF &viewportPosition) const {
        return touchContentAt(viewportPosition) != ContentHit::None;
    }

    // Abort whatever the synthetic pointer stream started, without committing.
    // Called when a second finger turns the gesture into navigation.
    virtual void cancelTouchPointerInteraction() {
    }
};

#endif // EDITORTOUCHTARGET_H
