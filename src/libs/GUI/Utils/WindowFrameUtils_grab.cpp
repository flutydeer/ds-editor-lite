#include <lite/GUI/Utils/WindowFrameUtils.h>

// Only compiled on platforms without a dedicated implementation. The project
// builds every .cpp on every platform, so the whole unit is empty elsewhere.
#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC)

#  include <QWidget>

bool WindowFrameUtils::captureWindowChrome(QWidget *widget, const QString &filePath) {
    return widget && widget->grab().save(filePath);
}

#endif
