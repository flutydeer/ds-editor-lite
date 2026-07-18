#include "ThemeLoader.h"

#include "Global/AppGlobal.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QStringList ThemeLoader::availableThemes() {
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
    // --- 1. Read & parse manifest ---
    QFile manifestFile(manifestPath(folderName));
    if (!manifestFile.open(QIODevice::ReadOnly))
        return std::nullopt;

    const auto manifestDoc = QJsonDocument::fromJson(manifestFile.readAll());
    manifestFile.close();
    if (!manifestDoc.isObject())
        return std::nullopt;

    const auto root = manifestDoc.object();

    // --- 2. Read manifest fields ---
    const auto name = root.value("name").toString();
    const auto author = root.value("author").toString();
    const auto colorType = root.value("colorType").toString();

    if (name.isEmpty() || colorType.isEmpty())
        return std::nullopt;

    // --- 3. Load style sheets ---
    const auto styleSheets = root.value("styleSheets").toArray();
    QString combinedStyleSheet;
    for (const auto &val : styleSheets) {
        const auto fileName = val.toString();
        if (fileName.isEmpty())
            return std::nullopt;

        QFile file(resourcePath(folderName, fileName));
        if (!file.open(QIODevice::ReadOnly))
            return std::nullopt;

        combinedStyleSheet += file.readAll() + "\n";
        file.close();
    }

    // --- 4. Load appColorPalette ---
    const auto paletteFileName = root.value("appColorPalette").toString();
    QList<QColor> paletteColors;
    if (!paletteFileName.isEmpty()) {
        QFile paletteFile(resourcePath(folderName, paletteFileName));
        if (!paletteFile.open(QIODevice::ReadOnly))
            return std::nullopt;

        const auto paletteDoc = QJsonDocument::fromJson(paletteFile.readAll());
        paletteFile.close();
        if (!paletteDoc.isObject())
            return std::nullopt;

        const auto arr = paletteDoc.object().value("baseColors").toArray();
        if (arr.size() != AppGlobal::paletteColorCount)
            return std::nullopt;

        for (const auto &val : arr) {
            QColor c(val.toString());
            if (!c.isValid())
                return std::nullopt;
            paletteColors.append(c);
        }
    }

    // --- 5. Load lyricStyleSheet ---
    const auto lyricFileName = root.value("lyricStyleSheet").toString();
    QString lyricStyleSheet;
    if (!lyricFileName.isEmpty()) {
        QFile lyricFile(resourcePath(folderName, lyricFileName));
        if (!lyricFile.open(QIODevice::ReadOnly))
            return std::nullopt;

        lyricStyleSheet = QString::fromUtf8(lyricFile.readAll());
        lyricFile.close();
    }

    // --- 6. All-or-nothing: construct result ---
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