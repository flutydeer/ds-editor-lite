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

    // True while any pointer is pressed, mouse button or finger. Timer-driven
    // safety nets must use this instead of QGuiApplication::mouseButtons(),
    // which is always NoButton during a touch drag.
    [[nodiscard]] bool isPointerPressed();

    // AppGlobal::resizeTolerance, widened while a touch stream is active.
    [[nodiscard]] double resizeTolerance();
    [[nodiscard]] double resizeTolerance(double baseTolerance);
}

#endif // EDITORPOINTERUTILS_H
