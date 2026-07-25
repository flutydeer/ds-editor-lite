#ifndef THEMEIDS_H
#define THEMEIDS_H

#include <QString>

// Theme-system vocabulary: the built-in theme ids, the user-facing theme
// preference ids, and the mapping between them. Owned by the theme system, not
// by the settings class (AppearanceOption) that merely persists a preference
// string. Consumers: ThemeManager, the appearance settings page.
namespace ThemeIds {
    inline QString defaultThemeId() {
        return QStringLiteral("lite-dark");
    }

    inline QString lightThemeId() {
        return QStringLiteral("lite-light");
    }

    inline QString systemThemePreferenceId() {
        return QStringLiteral("system");
    }

    inline QString lightThemePreferenceId() {
        return QStringLiteral("light");
    }

    inline QString darkThemePreferenceId() {
        return QStringLiteral("dark");
    }

    inline QString themeIdForPreference(const QString &themePreferenceId) {
        if (themePreferenceId == systemThemePreferenceId())
            return QString();
        if (themePreferenceId == lightThemePreferenceId())
            return lightThemeId();
        if (themePreferenceId == darkThemePreferenceId())
            return defaultThemeId();
        return themePreferenceId;
    }

    inline bool isBuiltInThemeId(const QString &themeId) {
        return themeId == defaultThemeId() || themeId == lightThemeId();
    }

    inline bool isThemePreferenceId(const QString &themeId) {
        return themeId == systemThemePreferenceId() || themeId == lightThemePreferenceId() ||
               themeId == darkThemePreferenceId();
    }
}

#endif // THEMEIDS_H
