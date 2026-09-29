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

    // May a single finger reach the interaction layer (the active tool) at all?
    // A view whose canvas is edited only by the pen and the mouse answers false,
    // and that answer is stronger than touchBlankDragAction(): a plain drag pans,
    // a tap does nothing, and a held press pans (or keeps the platform's menu)
    // instead of editing. Two-finger navigation and the pen are never affected.
    [[nodiscard]] virtual bool touchFingerEdits() const {
        return true;
    }

    // Where a single-finger stream goes. Kept a pure function of its inputs so
    // the whole table can be unit tested headlessly (see TestTouchGestures); the
    // controller only turns the answer into state.
    enum class FingerStream {
        // Nothing happens at all: no press, no move, no scroll.
        Consumed,
        // The viewport pans with the finger.
        Pan,
        // Held: staying put keeps the platform's menu, travelling pans.
        DeferredPan,
        // The interaction layer takes the stream, as if it came from the mouse.
        Synthetic,
        // Held: staying put keeps the menu, travelling edits.
        DeferredSynthetic,
    };

    struct FingerContext {
        // EditorTouchTarget::touchFingerEdits()
        bool fingerEdits = true;
        // EditorTouchGesture::Event::tap
        bool tap = false;
        // EditorTouchGesture::Event::fromLongPress
        bool fromLongPress = false;
        // EditorTouchTarget::touchContentAt()
        ContentHit content = ContentHit::None;
        // EditorTouchTarget::touchBlankDragAction()
        BlankDragAction blankDrag = BlankDragAction::SyntheticMouse;
    };

    [[nodiscard]] static FingerStream fingerStreamFor(const FingerContext &context) {
        if (!context.fingerEdits) {
            // Navigation only: a tap has nothing to select here and must not
            // create anything either, a drag pans, and a held press keeps the
            // menu until the finger travels — the bargain Pan territory makes.
            if (context.tap)
                return FingerStream::Consumed;
            return context.fromLongPress ? FingerStream::DeferredPan : FingerStream::Pan;
        }
        if (context.tap || context.fromLongPress) {
            // A tap always reaches the interaction layer: it selects or
            // deselects, never creates and never scrolls. A held press reaches it
            // too, but only where a plain drag is already taken by scrolling:
            // that is the one way a finger can still start a rubber band.
            const bool deferred =
                context.fromLongPress && context.blankDrag == BlankDragAction::Pan;
            return deferred ? FingerStream::DeferredSynthetic : FingerStream::Synthetic;
        }
        // Only a plain drag has to pick a side, and it may move content just when
        // the finger has already selected it. Over anything else it does what it
        // does over blank canvas.
        if (context.content == ContentHit::Selected)
            return FingerStream::Synthetic;
        return context.blankDrag == BlankDragAction::Pan ? FingerStream::Pan
                                                         : FingerStream::Synthetic;
    }

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

    // Touch streams that begin on an active inline text editor are owned by
    // the editor, not by the canvas: the controller forwards them untouched
    // and the gesture machine never sees the point. Begin answers whether the
    // point is over such an editor and took the stream; the rest deliver the
    // owned stream until it ends or the platform cancels it.
    [[nodiscard]] virtual bool touchRelayTextBegin(const QPointF &viewportPosition) {
        return false;
    }
    virtual void touchRelayTextMove(const QPointF &viewportPosition) {
    }
    virtual void touchRelayTextEnd(const QPointF &viewportPosition) {
    }
    virtual void touchRelayTextCancel() {
    }
};

#endif // EDITORTOUCHTARGET_H
