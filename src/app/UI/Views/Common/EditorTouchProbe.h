#ifndef EDITORTOUCHPROBE_H
#define EDITORTOUCHPROBE_H

// Diagnostics for the touch input path above the widget layer.
//
// EditorTouchController can only report what reaches the widget, and the
// failure this exists to catch is one where Qt stops delivering touch to
// anybody. QGuiApplicationPrivate::processTouchEvent() drops every touch event
// after a TouchCancel until the next TouchBegin, and it arms that gate even
// when no window had an active point, so no widget is told anything at all.
//
// Two filters therefore watch from further out, and neither consumes anything:
//
//   - an application event filter sees the window level QTouchEvent, including
//     a TouchCancel that never reaches a widget
//   - a native event filter (Windows) sees WM_POINTERCAPTURECHANGED itself,
//     which is what makes Qt cancel, plus the legacy mouse messages Windows
//     promotes from touch, which is what makes Windows change the capture
//
// Everything is off unless the Developer option "Log touch events" is on, and
// every line carries the EditorTouchProbe tag.
namespace EditorTouchProbe {

    // Installs both filters on the application, once. Safe to call repeatedly.
    void install();

} // namespace EditorTouchProbe

#endif // EDITORTOUCHPROBE_H
