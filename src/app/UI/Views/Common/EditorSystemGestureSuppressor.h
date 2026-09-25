#ifndef EDITORSYSTEMGESTURESUPPRESSOR_H
#define EDITORSYSTEMGESTURESUPPRESSOR_H

class QWidget;

// Windows recognises a press and hold on its own, independently of anything the
// application does with the same contact: a translucent square while the finger
// is down, then a right click on release (a pen gets a ring instead). No Qt
// event exposes it and no Qt switch turns it off, so wherever the touch layer
// intends to own the long press the platform has to be told to stand down. It
// asks once per contact, about the window under the finger:
//
//     WM_TABLET_QUERYSYSTEMGESTURESTATUS -> TABLET_DISABLE_PRESSANDHOLD
//
// and this is the shim that answers. It is the only mechanism that works: the
// legacy gesture stack (`SetGestureConfig` with `GC_ALLGESTURES`) is not
// consulted for these windows at all. Measured on a Surface-class tablet
// 2026-09-25, see docs/design/touch-and-pen-input-design.md §11.4.
//
// The scope is deliberately narrow, because the same answer is also the only
// way a long press reaches a context menu everywhere else: only windows that
// host a touch-enabled editor widget are answered, and only while the gesture
// layer is switched on. A dialog or a text field keeps the platform menu it has
// always had.
namespace EditorSystemGestureSuppressor {

    // Claim `editorWidget`'s window. The window is resolved on every query
    // rather than cached, so a dock that gets floated later follows along.
    // Idempotent, and the platform shim is installed on the first call.
    void addWindow(QWidget *editorWidget);

} // namespace EditorSystemGestureSuppressor

#endif // EDITORSYSTEMGESTURESUPPRESSOR_H
