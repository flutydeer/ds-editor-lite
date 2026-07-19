#include "ThemeLoader.h"

#include "Global/AppGlobal.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

static QString s_lastError;

QString ThemeLoader::lastError() {
    return s_lastError;
}

QStringList ThemeLoader::themeCandidates() {
    QStringList result;
    QDir themeDir(":/theme");
    const auto entries = themeDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &subdir : entries) {
        if (QFileInfo::exists(manifestPath(subdir)))
            result.append(subdir);
    }
    return result;
}

std::optional<ThemeDefinition> ThemeLoader::load(const QString &folderName) {
    s_lastError.clear();

    // --- 1. Read & parse manifest ---
    QFile manifestFile(manifestPath(folderName));
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        s_lastError = QStringLiteral("Cannot open manifest: %1").arg(manifestPath(folderName));
        return std::nullopt;
    }

    const auto manifestDoc = QJsonDocument::fromJson(manifestFile.readAll());
    manifestFile.close();
    if (!manifestDoc.isObject()) {
        s_lastError =
            QStringLiteral("Manifest is not a valid JSON object: %1").arg(manifestPath(folderName));
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

    // --- 3. Load style sheets ---
    const auto styleSheets = root.value("styleSheets").toArray();
    QString combinedStyleSheet;
    for (const auto &val : styleSheets) {
        const auto fileName = val.toString();
        if (fileName.isEmpty()) {
            s_lastError =
                QStringLiteral("Empty file name in styleSheets list for theme: %1").arg(folderName);
            return std::nullopt;
        }

        QFile file(resourcePath(folderName, fileName));
        if (!file.open(QIODevice::ReadOnly)) {
            s_lastError =
                QStringLiteral("Cannot open QSS file '%1' for theme: %2").arg(fileName, folderName);
            return std::nullopt;
        }

        combinedStyleSheet += file.readAll() + "\n";
        file.close();
    }

    // --- 4. Load appColorPalette ---
    // All-or-nothing: empty QSS list is invalid
    if (combinedStyleSheet.isEmpty()) {
        s_lastError =
            QStringLiteral("No style sheets loaded or all empty for theme: %1").arg(folderName);
        return std::nullopt;
    }

    const auto paletteFileName = root.value("appColorPalette").toString();
    QList<QColor> paletteColors;
    if (!paletteFileName.isEmpty()) {
        QFile paletteFile(resourcePath(folderName, paletteFileName));
        if (!paletteFile.open(QIODevice::ReadOnly)) {
            s_lastError = QStringLiteral("Cannot open palette file '%1' for theme: %2")
                              .arg(paletteFileName, folderName);
            return std::nullopt;
        }

        const auto paletteDoc = QJsonDocument::fromJson(paletteFile.readAll());
        paletteFile.close();
        if (!paletteDoc.isObject()) {
            s_lastError = QStringLiteral("Palette file is not a valid JSON object: %1")
                              .arg(resourcePath(folderName, paletteFileName));
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

    // --- 5. Load lyricStyleSheet ---
    // All-or-nothing: palette must be present and complete
    if (paletteColors.isEmpty()) {
        s_lastError =
            QStringLiteral("Missing or incomplete appColorPalette in theme: %1").arg(folderName);
        return std::nullopt;
    }

    const auto lyricFileName = root.value("lyricStyleSheet").toString();
    QString lyricStyleSheet;
    if (!lyricFileName.isEmpty()) {
        QFile lyricFile(resourcePath(folderName, lyricFileName));
        if (!lyricFile.open(QIODevice::ReadOnly)) {
            s_lastError = QStringLiteral("Cannot open lyric style sheet '%1' for theme: %2")
                              .arg(lyricFileName, folderName);
            return std::nullopt;
        }

        lyricStyleSheet = QString::fromUtf8(lyricFile.readAll());
        lyricFile.close();
    }

    // --- 6. All-or-nothing: construct result ---
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
    return def;
}

QString ThemeLoader::manifestPath(const QString &folderName) {
    return QStringLiteral(":/theme/%1/manifest.json").arg(folderName);
}

QString ThemeLoader::resourcePath(const QString &folderName, const QString &fileName) {
    return QStringLiteral(":/theme/%1/%2").arg(folderName, fileName);
}
