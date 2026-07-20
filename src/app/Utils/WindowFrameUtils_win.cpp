#include "WindowFrameUtils.h"

#include "UI/Utils/ThemeManager.h"

#ifdef Q_OS_WIN
#  include <QWidget>
#  include <dwmapi.h>

namespace {

    constexpr auto kImmersiveDarkMode = static_cast<DWMWINDOWATTRIBUTE>(20);
    constexpr auto kWindowCornerPreference = static_cast<DWMWINDOWATTRIBUTE>(33);
    constexpr auto kBorderColor = static_cast<DWMWINDOWATTRIBUTE>(34);
    // DWMWA_COLOR_DEFAULT: let DWM pick the border color for the current scheme
    constexpr COLORREF kDwmColorDefault = 0xFFFFFFFF;
    // DWMWA_COLOR_NONE: suppress the DWM window border entirely
    constexpr COLORREF kDwmColorNone = 0xFFFFFFFE;

    void applyTheme(HWND hwnd, const COLORREF borderColor) {
        const BOOL dark =
            ThemeManager::instance()->colorType() != ThemeManager::ThemeColorType::Light;
        DwmSetWindowAttribute(hwnd, kImmersiveDarkMode, &dark, sizeof(dark));

        // Update the border color for the new scheme. Do NOT force a synchronous non-client
        // refresh here (SetWindowPos with SWP_FRAMECHANGED + RedrawWindow with RDW_UPDATENOW):
        // issuing those from theme/show paths floods dwm.exe with synchronous cross-process
        // calls and can freeze the whole desktop compositor.
        DwmSetWindowAttribute(hwnd, kBorderColor, &borderColor, sizeof(borderColor));
    }

}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void WindowFrameUtils::applyFrameEffects(QWidget *widget) {
    if (!widget || !widget->winId())
        return;

    HWND hwnd = reinterpret_cast<HWND>(widget->winId());

    bool micaOn = false;
    const auto version = QSysInfo::productVersion();
    if (micaOn && version == "11") {
        // Enable Mica background
        auto backDropType = DWMSBT_MAINWINDOW;
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backDropType, sizeof(backDropType));
        // Extend title bar blur effect into client area
        constexpr int mgn = -1;
        MARGINS margins = {mgn, mgn, mgn, mgn};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    }

    // Experiment: in light themes drop the DWM border on the main window as well; keep the
    // default border in dark themes where it separates the window from dark backgrounds.
    const bool light = ThemeManager::instance()->colorType() == ThemeManager::ThemeColorType::Light;
    applyTheme(hwnd, light ? kDwmColorNone : kDwmColorDefault);
}

void WindowFrameUtils::applyPopupEffects(QWidget *widget, const CornerPreference cornerPreference) {
    if (!widget || !widget->winId())
        return;

    HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    DWMNCRENDERINGPOLICY renderingPolicy = DWMNCRP_ENABLED;
    DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &renderingPolicy,
                          sizeof(renderingPolicy));

    // NOTE: Do NOT call DwmExtendFrameIntoClientArea on these popups. Extending the DWM frame
    // into a frameless popup surface triggered a composition storm that froze dwm.exe
    // system-wide (confirmed by bisection, 2026-07), and it is not needed for the current
    // opaque popups anyway.

    const INT corner = static_cast<INT>(cornerPreference);
    DwmSetWindowAttribute(hwnd, kWindowCornerPreference, &corner, sizeof(corner));
    // Popups draw their own 1px border via QSS when needed; suppress the DWM border so it
    // doesn't paint a second (dark) outline hugging the rounded corners.
    applyTheme(hwnd, kDwmColorNone);
}

#endif
