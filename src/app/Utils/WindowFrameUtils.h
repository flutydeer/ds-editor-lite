//
// Created by fluty on 24-2-19.
//

#ifndef WINDOWFRAMEUTILS_H
#define WINDOWFRAMEUTILS_H

#include <QWidget>

#ifdef Q_OS_WIN
#  include <dwmapi.h>
#endif

class WindowFrameUtils {
public:
    static void applyFrameEffects(QWidget *widget) {
#ifdef Q_OS_WIN
        // Install Windows 11 SDK 22621 if DWMWA_SYSTEMBACKDROP_TYPE is not recognized by the compiler

        bool micaOn = true;
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
#endif
    }
};


#endif // WINDOWFRAMEUTILS_H
