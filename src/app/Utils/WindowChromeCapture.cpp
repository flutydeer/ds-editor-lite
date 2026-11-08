#include "WindowChromeCapture.h"

#include "Model/AppOptions/AppOptions.h"
#include <lite/GUI/Utils/WindowFrameUtils.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QTimer>
#include <QWidget>

namespace {
    // Long enough for the first paint, the theme style sheet and the initial
    // document to settle on a cold CI runner.
    constexpr int kDefaultDelayMs = 6000;
} // namespace

void WindowChromeCapture::armIfRequested(QWidget &window) {
    const auto directory = qEnvironmentVariable("LITE_CHROME_CAPTURE").trimmed();
    if (directory.isEmpty())
        return;

    bool hasDelay = false;
    const auto requestedDelay = qEnvironmentVariableIntValue("LITE_CHROME_CAPTURE_DELAY_MS",
                                                             &hasDelay);
    const auto delay = hasDelay && requestedDelay > 0 ? requestedDelay : kDefaultDelayMs;

    const auto mode = appOptions->appearance()->useNativeFrame ? QStringLiteral("native-frame")
                                                              : QStringLiteral("frameless");
    const auto path = QDir(directory).filePath(QStringLiteral("title-bar-%1.png").arg(mode));
    QDir().mkpath(QFileInfo(path).absolutePath());

    qInfo("Chrome capture armed: %s in %d ms", qPrintable(path), delay);
    QTimer::singleShot(delay, &window, [&window, path] {
        // The merge attempts during startup can land before AppKit attaches the
        // window to its view. By now the window is definitely on screen, so make
        // sure the chrome is in its settled shape before the shutter.
        WindowFrameUtils::mergeTitleBarIntoContent(&window);
        QTimer::singleShot(400, &window, [&window, path] {
            if (!WindowFrameUtils::captureWindowChrome(&window, path)) {
                qCritical("Chrome capture failed: %s", qPrintable(path));
                QCoreApplication::exit(2);
                return;
            }
            qInfo("Chrome capture written: %s", qPrintable(path));
            QCoreApplication::quit();
        });
    });
}
