//
// Created by fluty on 24-8-2.
//

#include "GeneralOption.h"

void GeneralOption::load(const QJsonObject &object) {
    if (object.contains(defaultSingingLanguageKey))
        defaultSingingLanguage = object[defaultSingingLanguageKey].toString();

    if (object.contains(defaultLyricKey))
        defaultLyric = object[defaultLyricKey].toString();
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
        serialize_defaultPackage(),
        serialize_defaultSingerId(),
        serialize_defaultSpeakerId(),
        serialize_somePath(),
        serialize_rmvpePath()
    };
}