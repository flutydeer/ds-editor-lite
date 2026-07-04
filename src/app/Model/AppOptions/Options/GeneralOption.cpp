//
// Created by fluty on 24-8-2.
//

#include <QJsonArray>

#include "GeneralOption.h"

namespace {
    constexpr int maxRecentProjectFiles = 10;

    bool recentProjectPathsEqual(const QString &lhs, const QString &rhs) {
#ifdef Q_OS_WIN
        return QString::compare(lhs, rhs, Qt::CaseInsensitive) == 0;
#else
        return lhs == rhs;
#endif
    }
}

void GeneralOption::load(const QJsonObject &object) {
    if (object.contains(defaultSingingLanguageKey))
        defaultSingingLanguage = object[defaultSingingLanguageKey].toString();

    if (object.contains(defaultLyricsKey)) {
        const auto obj = object[defaultLyricsKey].toObject();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
            defaultLyrics[it.key()] = it.value().toString();
    }

    if (const auto it = object.constFind(packageSearchPathsKey); it != object.constEnd()) {
        QStringList paths;
        if (it->isArray()) {
            const auto arr = it->toArray();
            for (auto item : std::as_const(arr)) {
                if (item.isString()) {
                    paths.append(item.toString());
                }
            }
        } else if (it->isString()) {
            paths.append(it->toString());
        }
        packageSearchPaths = std::move(paths);
    } else {
        packageSearchPaths = {};
    }

    recentProjectFiles.clear();
    if (const auto it = object.constFind(recentProjectFilesKey);
        it != object.constEnd() && it->isArray()) {
        const auto arr = it->toArray();
        for (auto item : arr) {
            if (!item.isString())
                continue;
            const auto path = item.toString();
            if (path.isEmpty())
                continue;
            bool duplicated = false;
            for (const auto &file : std::as_const(recentProjectFiles)) {
                if (recentProjectPathsEqual(file, path)) {
                    duplicated = true;
                    break;
                }
            }
            if (duplicated)
                continue;
            recentProjectFiles.append(path);
            if (recentProjectFiles.size() >= maxRecentProjectFiles)
                break;
        }
    }

    if (object.contains(speakerMixPresetsKey))
        speakerMixPresets = object[speakerMixPresetsKey];

    if (object.contains(gameDirKey))
        gameDir = object[gameDirKey].toString();
    if (object.contains(rmvpePathKey))
        rmvpePath = object[rmvpePathKey].toString();
}

void GeneralOption::save(QJsonObject &object) {
    QJsonObject lyricsObj;
    for (auto it = defaultLyrics.constBegin(); it != defaultLyrics.constEnd(); ++it)
        lyricsObj[it.key()] = it.value();

    object = {
        {defaultSingingLanguageKey, defaultSingingLanguage                        },
        {defaultLyricsKey,          lyricsObj                                     },
        {packageSearchPathsKey,     QJsonArray::fromStringList(packageSearchPaths)},
        {recentProjectFilesKey,     QJsonArray::fromStringList(recentProjectFiles)},
        {speakerMixPresetsKey,      speakerMixPresets                             },
        serialize_gameDir(),
        serialize_rmvpePath()
    };
}

void GeneralOption::setPackageSearchPathsAndNotify(QStringList paths) {
    packageSearchPaths = std::move(paths);
    // Q_EMIT packageSearchPathsChanged();
}

QString GeneralOption::defaultLyricForLanguage(const QString &language) const {
    if (auto it = defaultLyrics.find(language); it != defaultLyrics.end())
        return it.value();
    return "la";
}
