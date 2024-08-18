//
// Created by fluty on 24-3-21.
//

#include "AudioOption.h"

void AudioOption::load(const QJsonObject &object) {
    obj = object;
}

void AudioOption::save(QJsonObject &object) {
    object = obj;
}