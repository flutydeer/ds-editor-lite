#ifndef WINDOWCHROMECAPTURE_H
#define WINDOWCHROMECAPTURE_H

class QWidget;

// CI-only screenshot harness.
//
// macOS runners cannot use screencapture(1) headlessly: the screen recording
// consent prompt is a system dialog nobody can click, and it fires for the
// second capture in a job. Capturing from inside the process avoids that
// entirely, because a window may always read its own pixels.
//
// Set LITE_CHROME_CAPTURE to a directory and the app writes title-bar-*.png
// there once the window has settled, then quits. LITE_FORCE_NATIVE_FRAME picks
// the frame mode for that run without touching the saved config.
namespace WindowChromeCapture {
    // Arms the harness for window. Does nothing when the variable is unset.
    void armIfRequested(QWidget &window);
}

#endif // WINDOWCHROMECAPTURE_H
