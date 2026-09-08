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

    // Is there an editable object (note, clip, anchor...) under the point?
    [[nodiscard]] virtual bool touchHitsContent(const QPointF &viewportPosition) const = 0;
    [[nodiscard]] virtual BlankDragAction touchBlankDragAction() const = 0;

    // Abort whatever the synthetic pointer stream started, without committing.
    // Called when a second finger turns the gesture into navigation.
    virtual void cancelTouchPointerInteraction() {
    }
};

#endif // EDITORTOUCHTARGET_H
