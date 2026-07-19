#include "ThemeLoader.h"

#include "ThemeColorResolver.h"

#include "Global/AppGlobal.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

static QString s_lastError;

namespace {

    QString bundledThemeRoot(const QString &folderName) {
        return QStringLiteral(":/theme/%1").arg(folderName);
    }

    QString externalThemeRootPath(const QString &folderName) {
        const auto root = ThemeLoader::externalThemeRoot();
        if (root.isEmpty())
            return {};
        return QDir(root).filePath(folderName);
    }

    bool isSafeFolderName(const QString &folderName) {
        if (folderName.isEmpty() || QDir::isAbsolutePath(folderName))
            return false;
        const auto parts = folderName.split(QRegularExpression(QStringLiteral("[/\\\\]")));
        return parts.size() == 1 && parts.first() != QStringLiteral(".") &&
               parts.first() != QStringLiteral("..");
    }

}

QString ThemeLoader::lastError() {
    return s_lastError;
}

QString ThemeLoader::externalThemeRoot() {
    const auto value = qEnvironmentVariable("DS_EDITOR_THEME_DIR").trimmed();
    if (value.isEmpty())
        return {};

    const QFileInfo info(value);
    if (!info.exists() || !info.isDir())
        return {};
    return info.absoluteFilePath();
}

QStringList ThemeLoader::themeCandidates() {
    QStringList result;

    const auto externalRoot = externalThemeRoot();
    if (!externalRoot.isEmpty()) {
        const QDir externalThemeDir(externalRoot);
        for (const auto &subdir : externalThemeDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (QFileInfo::exists(externalThemeDir.filePath(subdir + QStringLiteral("/manifest.json"))) &&
                !result.contains(subdir))
                result.append(subdir);
        }
    }

    QDir themeDir(":/theme");
    const auto entries = themeDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &subdir : entries) {
        if (QFileInfo::exists(manifestPath(subdir)) && !result.contains(subdir))
            result.append(subdir);
    }
    return result;
}

std::optional<ThemeDefinition> ThemeLoader::load(const QString &folderName) {
    s_lastError.clear();

    if (!isSafeFolderName(folderName)) {
        s_lastError = QStringLiteral("Invalid theme folder name: %1").arg(folderName);
        return std::nullopt;
    }

    const auto externalRoot = externalThemeRootPath(folderName);
    const auto externalManifest = externalRoot.isEmpty()
                                      ? QString{}
                                      : QDir(externalRoot).filePath(QStringLiteral("manifest.json"));
    const bool useExternal = !externalManifest.isEmpty() && QFileInfo::exists(externalManifest);
    const auto themeRoot = useExternal ? externalRoot : bundledThemeRoot(folderName);
    const auto manifestFilePath = useExternal ? externalManifest : manifestPath(folderName);

    auto filePath = [&](const QString &fileName) {
        return useExternal ? QDir(themeRoot).filePath(fileName)
                           : resourcePath(folderName, fileName);
    };

    // --- 1. Read & parse manifest ---
    QFile manifestFile(manifestFilePath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        s_lastError = QStringLiteral("Cannot open manifest: %1").arg(manifestFilePath);
        return std::nullopt;
    }

    const auto manifestDoc = QJsonDocument::fromJson(manifestFile.readAll());
    manifestFile.close();
    if (!manifestDoc.isObject()) {
        s_lastError =
            QStringLiteral("Manifest is not a valid JSON object: %1").arg(manifestFilePath);
        return std::nullopt;
    }

    const auto root = manifestDoc.object();

    // --- 2. Read manifest fields ---
    const auto name = root.value("name").toString();
    const auto author = root.value("author").toString();
    const auto colorType = root.value("colorType").toString();

    if (name.isEmpty() || colorType.isEmpty()) {
        s_lastError = QStringLiteral("Manifest missing required fields (name, colorType): %1")
                          .arg(manifestPath(folderName));
        return std::nullopt;
    }

    // All-or-nothing: colorType must be one of the defined values
    if (colorType != QStringLiteral("light") && colorType != QStringLiteral("dark") &&
        colorType != QStringLiteral("highContrast")) {
        s_lastError = QStringLiteral("Invalid colorType '%1' in manifest: %2")
                          .arg(colorType, manifestPath(folderName));
        return std::nullopt;
    }

    // --- 3. Load semantic color tokens ---
    const auto colorsFileName = root.value("colors").toString();
    if (!isSafeResourceName(colorsFileName)) {
        s_lastError = QStringLiteral("Manifest missing required field 'colors': %1")
                          .arg(manifestFilePath);
        return std::nullopt;
    }

    QFile colorsFile(filePath(colorsFileName));
    if (!colorsFile.open(QIODevice::ReadOnly)) {
        s_lastError = QStringLiteral("Cannot open colors file '%1' for theme: %2")
                          .arg(colorsFileName, folderName);
        return std::nullopt;
    }

    QString colorsError;
    const auto themeColors = ThemeColorResolver::parse(colorsFile.readAll(), &colorsError);
    colorsFile.close();
    if (!themeColors) {
        s_lastError = QStringLiteral("Invalid colors file '%1' for theme '%2': %3")
                          .arg(colorsFileName, folderName, colorsError);
        return std::nullopt;
    }

    // --- 4. Load style sheets ---
    const auto styleSheets = root.value("styleSheets").toArray();
    QString combinedStyleSheet;
    for (const auto &val : styleSheets) {
        const auto fileName = val.toString();
        if (fileName.isEmpty()) {
            s_lastError =
                QStringLiteral("Empty file name in styleSheets list for theme: %1").arg(folderName);
            return std::nullopt;
        }

        if (!isSafeResourceName(fileName)) {
            s_lastError = QStringLiteral("Invalid QSS file name '%1' for theme: %2")
                              .arg(fileName, folderName);
            return std::nullopt;
        }

        QFile file(filePath(fileName));
        if (!file.open(QIODevice::ReadOnly)) {
            s_lastError =
                QStringLiteral("Cannot open QSS file '%1' for theme: %2").arg(fileName, folderName);
            return std::nullopt;
        }

        const QString rawStyleSheet = QString::fromUtf8(file.readAll());
        file.close();

        QString substitutionError;
        const auto resolvedStyleSheet = ThemeColorResolver::applyToStyleSheet(
            rawStyleSheet, *themeColors, nullptr, &substitutionError);
        if (!resolvedStyleSheet) {
            s_lastError =
                QStringLiteral("Invalid color placeholder in QSS file '%1' for theme '%2': %3")
                    .arg(fileName, folderName, substitutionError);
            return std::nullopt;
        }
        combinedStyleSheet += *resolvedStyleSheet + "\n";
    }

    // --- 5. Load appColorPalette ---
    // All-or-nothing: empty QSS list is invalid
    if (combinedStyleSheet.isEmpty()) {
        s_lastError =
            QStringLiteral("No style sheets loaded or all empty for theme: %1").arg(folderName);
        return std::nullopt;
    }

    const auto paletteFileName = root.value("appColorPalette").toString();
    QList<QColor> paletteColors;
    if (!paletteFileName.isEmpty()) {
        if (!isSafeResourceName(paletteFileName)) {
            s_lastError = QStringLiteral("Invalid palette file name '%1' for theme: %2")
                              .arg(paletteFileName, folderName);
            return std::nullopt;
        }

        QFile paletteFile(filePath(paletteFileName));
        if (!paletteFile.open(QIODevice::ReadOnly)) {
            s_lastError = QStringLiteral("Cannot open palette file '%1' for theme: %2")
                              .arg(paletteFileName, folderName);
            return std::nullopt;
        }

        const auto paletteDoc = QJsonDocument::fromJson(paletteFile.readAll());
        paletteFile.close();
        if (!paletteDoc.isObject()) {
            s_lastError = QStringLiteral("Palette file is not a valid JSON object: %1")
                              .arg(filePath(paletteFileName));
            return std::nullopt;
        }

        const auto arr = paletteDoc.object().value("baseColors").toArray();
        if (arr.size() != AppGlobal::paletteColorCount) {
            s_lastError =
                QStringLiteral("Palette has %1 colors instead of expected %2 in theme: %3")
                    .arg(arr.size())
                    .arg(AppGlobal::paletteColorCount)
                    .arg(folderName);
            return std::nullopt;
        }

        for (const auto &val : arr) {
            QColor c(val.toString());
            if (!c.isValid()) {
                s_lastError = QStringLiteral("Invalid color value '%1' in palette for theme: %2")
                                  .arg(val.toString(), folderName);
                return std::nullopt;
            }
            paletteColors.append(c);
        }
    }

    // --- 6. Load lyricStyleSheet ---
    // All-or-nothing: palette must be present and complete
    if (paletteColors.isEmpty()) {
        s_lastError =
            QStringLiteral("Missing or incomplete appColorPalette in theme: %1").arg(folderName);
        return std::nullopt;
    }

    const auto lyricFileName = root.value("lyricStyleSheet").toString();
    QString lyricStyleSheet;
    if (!lyricFileName.isEmpty()) {
        if (!isSafeResourceName(lyricFileName)) {
            s_lastError = QStringLiteral("Invalid lyric style sheet name '%1' for theme: %2")
                              .arg(lyricFileName, folderName);
            return std::nullopt;
        }

        QFile lyricFile(filePath(lyricFileName));
        if (!lyricFile.open(QIODevice::ReadOnly)) {
            s_lastError = QStringLiteral("Cannot open lyric style sheet '%1' for theme: %2")
                              .arg(lyricFileName, folderName);
            return std::nullopt;
        }

        const QString rawLyricStyleSheet = QString::fromUtf8(lyricFile.readAll());
        lyricFile.close();

        QString substitutionError;
        const auto resolvedLyricStyleSheet = ThemeColorResolver::applyToStyleSheet(
            rawLyricStyleSheet, *themeColors, nullptr, &substitutionError);
        if (!resolvedLyricStyleSheet) {
            s_lastError = QStringLiteral(
                              "Invalid color placeholder in lyric QSS file '%1' for theme '%2': %3")
                              .arg(lyricFileName, folderName, substitutionError);
            return std::nullopt;
        }
        lyricStyleSheet = *resolvedLyricStyleSheet;
    }

    // --- 7. All-or-nothing: construct result ---
    // All-or-nothing: lyric style sheet is required
    if (lyricStyleSheet.isEmpty()) {
        s_lastError =
            QStringLiteral("Missing or empty lyricStyleSheet in theme: %1").arg(folderName);
        return std::nullopt;
    }

    ThemeDefinition def;
    def.folderName = folderName;
    def.name = name;
    def.author = author;
    def.colorType = colorType;
    def.styleSheet = combinedStyleSheet;
    def.paletteColors = paletteColors;
    def.lyricStyleSheet = lyricStyleSheet;
    def.semanticColors = themeColors->tokens;
    return def;
}

QString ThemeLoader::manifestPath(const QString &folderName) {
    return QStringLiteral(":/theme/%1/manifest.json").arg(folderName);
}

bool ThemeLoader::isSafeResourceName(const QString &fileName) {
    if (fileName.isEmpty() || QDir::isAbsolutePath(fileName))
        return false;

    const auto parts = fileName.split(QRegularExpression(QStringLiteral("[/\\\\]")));
    for (const auto &part : parts) {
        if (part.isEmpty() || part == QStringLiteral(".") || part == QStringLiteral(".."))
            return false;
    }
    return true;
}

QString ThemeLoader::resourcePath(const QString &folderName, const QString &fileName) {
    return QStringLiteral(":/theme/%1/%2").arg(folderName, fileName);
}
