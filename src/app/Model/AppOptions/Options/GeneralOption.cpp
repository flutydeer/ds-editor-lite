//
// Created by fluty on 24-8-2.
//

#include "GeneralOption.h"

void GeneralOption::load(const QJsonObject &object) {
    if (object.contains(defaultSingingLanguageKey))
        defaultSingingLanguage = object[defaultSingingLanguageKey].toString();

    if (object.contains(defaultLyricKey))
        defaultLyric = object[defaultLyricKey].toString();
    if (object.contains(defaultSingerKey))
        defaultSinger = object[defaultSingerKey].toString();
}

void GeneralOption::save(QJsonObject &object) {
    object = {
        {defaultSingingLanguageKey, defaultSingingLanguage},
        {defaultLyricKey,           defaultLyric          },
        serialize_defaultSinger()
    };
}