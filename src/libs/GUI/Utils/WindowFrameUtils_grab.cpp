#include <lite/GUI/Utils/WindowFrameUtils.h>

// Only compiled on platforms without a dedicated implementation. The project
// builds every .cpp on every platform, so the whole unit is empty elsewhere.
#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC)

#  include <QWidget>

bool WindowFrameUtils::captureWindowChrome(QWidget *widget, const QString &filePath,
                                           QString *captureMethod) {
    const bool saved = widget && widget->grab().save(filePath);
    if (captureMethod)
        *captureMethod = saved ? QStringLiteral("qt-client-fallback") : QStringLiteral("failed");
    qInfo("Window capture: %s (Qt client area only)", saved ? "qt-client-fallback" : "failed");
    return saved;
}

#endif
