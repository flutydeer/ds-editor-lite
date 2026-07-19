#ifndef THEMELOADER_H
#define THEMELOADER_H

#include "ThemeDefinition.h"

#include <optional>
#include <QStringList>

class ThemeLoader {
public:
    /// Returns the external theme root from DS_EDITOR_THEME_DIR, if configured and present.
    static QString externalThemeRoot();

    /// Scan :/theme/ and return folder names that contain a manifest.json.
    /// Note: does NOT fully validate the theme; use load() for verification.
    static QStringList themeCandidates();

    /// Load a theme by folder name. Returns nullopt if any file is missing or
    /// fails validation — guarantees all-or-nothing semantics.
    static std::optional<ThemeDefinition> load(const QString &folderName);

    /// Returns a human-readable error message from the last failed load() call.
    static QString lastError();

private:
    static bool isSafeResourceName(const QString &fileName);
    static QString manifestPath(const QString &folderName);
    static QString resourcePath(const QString &folderName, const QString &fileName);
};

#endif // THEMELOADER_H
