//
// Created by fluty on 24-8-2.
//

#include <QJsonArray>

#include "GeneralOption.h"

void GeneralOption::load(const QJsonObject &object) {
    if (object.contains(defaultSingingLanguageKey))
        defaultSingingLanguage = object[defaultSingingLanguageKey].toString();

    if (object.contains(defaultLyricKey))
        defaultLyric = object[defaultLyricKey].toString();

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

    if (object.contains(defaultPackageKey))
        defaultPackage = object[defaultPackageKey].toString();
    if (object.contains(defaultSingerIdKey))
        defaultSingerId = object[defaultSingerIdKey].toString();
    if (object.contains(defaultSpeakerIdKey))
        defaultSpeakerId = object[defaultSpeakerIdKey].toString();

    if (object.contains(somePathKey))
        somePath = object[somePathKey].toString();
    if (object.contains(rmvpePathKey))
        rmvpePath = object[rmvpePathKey].toString();
}

void GeneralOption::save(QJsonObject &object) {
    object = {
        {defaultSingingLanguageKey, defaultSingingLanguage},
        {defaultLyricKey,           defaultLyric          },
        {packageSearchPathsKey, QJsonArray::fromStringList(packageSearchPaths)},
        serialize_defaultPackage(),
        serialize_defaultSingerId(),
        serialize_defaultSpeakerId(),
        serialize_somePath(),
        serialize_rmvpePath()
    };
}

void GeneralOption::setPackageSearchPathsAndNotify(QStringList paths) {
    packageSearchPaths = std::move(paths);
    //Q_EMIT packageSearchPathsChanged();
}