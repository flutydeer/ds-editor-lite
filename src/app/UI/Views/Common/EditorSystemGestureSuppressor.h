#ifndef EDITORSYSTEMGESTURESUPPRESSOR_H
#define EDITORSYSTEMGESTURESUPPRESSOR_H

class QWidget;

// Suppress the Windows press-and-hold right click for touch-enabled editor windows.
// Windows queries the window under the pointer with WM_TABLET_QUERYSYSTEMGESTURESTATUS.
// Reply with TABLET_DISABLE_PRESSANDHOLD so the editor can handle the same contact.
// SetGestureConfig(GC_ALLGESTURES) leaves press-and-hold active on this WM_POINTER path.
// Other windows keep their native press-and-hold behavior.
namespace EditorSystemGestureSuppressor {

    // Claim `editorWidget`'s window. The window is resolved on every query
    // rather than cached, so a dock that gets floated later follows along.
    // Idempotent, and the platform shim is installed on the first call.
    void addWindow(QWidget *editorWidget);

} // namespace EditorSystemGestureSuppressor

#endif // EDITORSYSTEMGESTURESUPPRESSOR_H
