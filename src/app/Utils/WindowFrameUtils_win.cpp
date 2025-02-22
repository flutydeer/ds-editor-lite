#include "WindowFrameUtils.h"

#ifdef Q_OS_WIN
#  include <QWidget>
#  include <dwmapi.h>

void WindowFrameUtils::applyFrameEffects(QWidget *widget) {

    // Install Windows 11 SDK 22621 if DWMWA_SYSTEMBACKDROP_TYPE is not recognized by the compiler

    bool micaOn = false;
    auto version = QSysInfo::productVersion();
    if (micaOn && version == "11") {
        // Enable Mica background
        auto backDropType = DWMSBT_MAINWINDOW;
        DwmSetWindowAttribute(reinterpret_cast<HWND>(widget->winId()), DWMWA_SYSTEMBACKDROP_TYPE,
                              &backDropType, sizeof(backDropType));
        // Extend title bar blur effect into client area
        constexpr int mgn = -1;
        MARGINS margins = {mgn, mgn, mgn, mgn};
        DwmExtendFrameIntoClientArea(reinterpret_cast<HWND>(widget->winId()), &margins);
    }
    // Dark theme
    uint dark = 1;
    DwmSetWindowAttribute(reinterpret_cast<HWND>(widget->winId()), DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &dark, sizeof(dark));
}

#endif
