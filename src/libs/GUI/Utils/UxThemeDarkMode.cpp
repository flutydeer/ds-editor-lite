#include <lite/GUI/Utils/UxThemeDarkMode.h>

#include <QtGlobal>

#ifdef Q_OS_WIN
#  include <windows.h>

// Undocumented uxtheme exports that control how Windows renders system-drawn
// menus (title-bar right-click system menu, Alt+Space menu, native Win32 popup
// menus). DWMWA_USE_IMMERSIVE_DARK_MODE is a separate path and does NOT touch
// these, so setting it alone leaves the system menu light. Signatures mirror
// QWindowKit's qwkwindowsextra_p.h and Chromium's base/win/dark_mode_support.cc.
namespace {

    // PREFERRED_APP_MODE, Win10 1809+ (PAM_DARK/LIGHT need 1903+).
    enum PreferredAppMode {
        PamDefault = 0,
        PamAllowDark = 1,
        PamDark = 2,
        PamLight = 3,
    };

    using SetPreferredAppModeFunc = PreferredAppMode(WINAPI *)(PreferredAppMode);
    using AllowDarkModeForAppFunc = bool(WINAPI *)(bool);
    using FlushMenuThemesFunc = void(WINAPI *)();

    struct UxThemeApis {
        SetPreferredAppModeFunc setPreferredAppMode = nullptr;
        AllowDarkModeForAppFunc allowDarkModeForApp = nullptr;
        FlushMenuThemesFunc flushMenuThemes = nullptr;
    };

    const UxThemeApis &uxThemeApis() {
        static const UxThemeApis apis = [] {
            UxThemeApis resolved{};
            // uxtheme ships with Windows; resolving by ordinal avoids linking
            // against undocumented symbols at build time.
            HMODULE library =
                ::LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!library)
                return resolved;
            // Ordinal 135 is SetPreferredAppMode on 1903+ and AllowDarkModeForApp
            // on 1809-1903 (mutually exclusive per build); resolve both names
            // against the same ordinal and pick at runtime.
            resolved.setPreferredAppMode = reinterpret_cast<SetPreferredAppModeFunc>(
                ::GetProcAddress(library, MAKEINTRESOURCEA(135)));
            resolved.allowDarkModeForApp = reinterpret_cast<AllowDarkModeForAppFunc>(
                ::GetProcAddress(library, MAKEINTRESOURCEA(135)));
            resolved.flushMenuThemes = reinterpret_cast<FlushMenuThemesFunc>(
                ::GetProcAddress(library, MAKEINTRESOURCEA(136)));
            return resolved;
        }();
        return apis;
    }

    bool g_lastDark = false;
    bool g_lastValid = false;

} // namespace

void UxThemeDarkMode::updateFromColorType(const bool dark) {
    const auto &apis = uxThemeApis();
    if (!apis.flushMenuThemes)
        return;

    // FlushMenuThemes() rebuilds cached menu theme handles, so only call the
    // pair when the mode actually flips; popups re-enter this on every show.
    if (g_lastValid && g_lastDark == dark)
        return;

    if (apis.setPreferredAppMode)
        apis.setPreferredAppMode(dark ? PamDark : PamLight);
    else if (apis.allowDarkModeForApp)
        apis.allowDarkModeForApp(dark);

    apis.flushMenuThemes();
    g_lastDark = dark;
    g_lastValid = true;
}

#else

void UxThemeDarkMode::updateFromColorType(const bool dark) {
    Q_UNUSED(dark);
}

#endif
