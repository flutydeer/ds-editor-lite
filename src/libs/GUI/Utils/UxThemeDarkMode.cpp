#include <lite/GUI/Utils/UxThemeDarkMode.h>

#include <QtGlobal>

#ifdef Q_OS_WIN
#  include <QSysInfo>
#  include <QVersionNumber>
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
        PamDark = 2,
        PamLight = 3,
    };

    using SetPreferredAppModeFunc = PreferredAppMode(WINAPI *)(PreferredAppMode);
    using AllowDarkModeForAppFunc = bool(WINAPI *)(bool);
    using FlushMenuThemesFunc = void(WINAPI *)();

    // Ordinal 135 is SetPreferredAppMode on Win10 1903+ but AllowDarkModeForApp on
    // 1809-1903: same export slot, mutually exclusive signatures. Calling the
    // 1809 function through the SetPreferredAppMode signature makes the light path
    // pass PamLight (3), which bool interpretation reads as true, so the requested
    // light mode never lands. Pick one side based on the OS build, as Chromium and
    // QWindowKit do.
    //
    // QSysInfo::kernelVersion() reports the full kernel build (10.0.18362) via
    // RtlGetVersion. productVersion() only carries the release number ("10",
    // "11"), which parses below any build gate, and VerifyVersionInfoW is not
    // usable here either: this project links with /manifest:no (CMakeLists.txt),
    // and without a supported-OS manifest the version APIs are subject to
    // compatibility shims that can report an older build.
    bool isWin101903OrGreater() {
        static const bool cached = [] {
            const QVersionNumber build = QVersionNumber::fromString(QSysInfo::kernelVersion());
            return build >= QVersionNumber(10, 0, 18362);
        }();
        return cached;
    }

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
            // Ordinal 135 carries a different signature per build, so only the
            // side matching the current OS is resolved; the two must never both
            // be non-null.
            if (isWin101903OrGreater()) {
                resolved.setPreferredAppMode = reinterpret_cast<SetPreferredAppModeFunc>(
                    ::GetProcAddress(library, MAKEINTRESOURCEA(135)));
            } else {
                resolved.allowDarkModeForApp = reinterpret_cast<AllowDarkModeForAppFunc>(
                    ::GetProcAddress(library, MAKEINTRESOURCEA(135)));
            }
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
