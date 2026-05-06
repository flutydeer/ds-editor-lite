//
// Created by fluty on 24-8-2.
//

#include <QJsonArray>

#include "GeneralOption.h"

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

#if false
    if (object.contains(defaultPackageKey))
        defaultPackage = object[defaultPackageKey].toString();
    if (object.contains(defaultPackageIdKey))
        defaultPackageId = object[defaultPackageIdKey].toString();
    if (object.contains(defaultPackageVersionKey))
        defaultPackageVersion = QVersionNumber::fromString(object[defaultPackageVersionKey].toString());
    if (object.contains(defaultSingerIdKey))
        defaultSingerId = object[defaultSingerIdKey].toString();
    if (object.contains(defaultSpeakerIdKey))
        defaultSpeakerId = object[defaultSpeakerIdKey].toString();
#endif
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
#if false
        serialize_defaultPackage(),
        serialize_defaultPackageId(),
        {defaultPackageVersionKey, defaultPackageVersion.toString()},
        serialize_defaultSingerId(),
        serialize_defaultSpeakerId(),
#endif
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