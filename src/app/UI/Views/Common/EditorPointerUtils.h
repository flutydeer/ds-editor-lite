#ifndef EDITORPOINTERUTILS_H
#define EDITORPOINTERUTILS_H

class QPointerEvent;
class QSinglePointEvent;

// Helpers shared by every editor view that has to tell a finger from a mouse.
//
// Touch is delivered to the interaction state machines as synthetic mouse
// events carrying the originating touch device (see EditorTouchController), so
// the whole codebase can keep working in QMouseEvent terms while still being
// able to ask "did this come from a finger?" where it matters — hit tolerance,
// and the "is the pointer still down?" safety nets that used to read
// QGuiApplication::mouseButtons().
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

    // A synthetic pen stream is currently driving mouse events somewhere. This
    // is a counter of its own rather than a reuse of the touch one: the two
    // differ exactly where the counter is read, because hit tolerance is
    // widened for a finger and must not be for a pen.
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

    // AppGlobal::resizeTolerance, widened while a touch stream is active.
    [[nodiscard]] double resizeTolerance();
    [[nodiscard]] double resizeTolerance(double baseTolerance);
}

#endif // EDITORPOINTERUTILS_H
