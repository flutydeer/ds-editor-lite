#ifndef EDITORPOINTERUTILS_H
#define EDITORPOINTERUTILS_H

class QPointerEvent;
class QSinglePointEvent;

// Synthetic mouse events carry their originating touch device. Editors use this
// to choose hit tolerances and track pressed state across pointer types.
namespace EditorPointer {
    // Touch hit targets are much coarser than a mouse cursor, so edge grab
    // zones are widened by this factor while a touch stream is in flight.
    inline constexpr double touchToleranceScale = 2.5;

    [[nodiscard]] bool isTouchDevice(const QPointerEvent *event);
    [[nodiscard]] bool isPenDevice(const QPointerEvent *event);

    // A synthetic touch stream is currently driving mouse events somewhere.
    [[nodiscard]] bool isTouchStreamActive();
    void beginTouchStream();
    void endTouchStream();

    // Track synthetic pen streams separately so only touch widens hit targets.
    [[nodiscard]] bool isPenStreamActive();
    void beginPenStream();
    void endPenStream();

    // The pen stroke in flight is an erase stroke: the platform reported the
    // eraser tip, or a barrel button drag. A view takes a plain left-button
    // stream and routes it to the erase path of the tool it is sitting on
    // while this is set, which is how erasing avoids switching tools.
    [[nodiscard]] bool isPenEraseIntentActive();
    void beginPenEraseIntent();
    void endPenEraseIntent();

    // True while any pointer is pressed, mouse button, finger or pen.
    // Timer-driven safety nets must use this instead of
    // QGuiApplication::mouseButtons(), which is always NoButton during a touch
    // drag and during a pen stroke the pen layer translates itself.
    [[nodiscard]] bool isPointerPressed();

    // Keep touch resize handles visible after finger lift so the user can target them.
    // Mouse, touchpad, or pen input clears the handles and uses the hover cursor.
    [[nodiscard]] bool touchAffordanceActive();
    void latchTouchAffordance();
    void clearTouchAffordance();

    // AppGlobal::resizeTolerance, widened while a touch stream is active.
    [[nodiscard]] double resizeTolerance();
    [[nodiscard]] double resizeTolerance(double baseTolerance);
}

#endif // EDITORPOINTERUTILS_H
