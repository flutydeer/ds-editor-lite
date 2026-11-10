#ifndef UXTHEMEDARKMODE_H
#define UXTHEMEDARKMODE_H

// Header is intentionally platform-neutral: the Windows implementation carries
// the uxtheme plumbing, other platforms are a no-op. Consumers (WindowFrameUtils)
// call this from cross-platform code paths without ifdefs.

namespace UxThemeDarkMode {

    // Pushes the current ThemeManager color type into uxtheme's preferred app mode.
    // Dark: PAM_DARK (or AllowDarkModeForApp(TRUE) pre-1903); Light: PAM_LIGHT (or FALSE).
    // Safe to call repeatedly: the implementation early-returns when the mode is
    // already in the requested state, so cheap per-popup calls do not thrash the
    // menu theme cache. Unavailable ordinals (Win10 1809 and older) degrade to a no-op.
    void updateFromColorType(bool dark);

} // namespace UxThemeDarkMode

#endif // UXTHEMEDARKMODE_H
