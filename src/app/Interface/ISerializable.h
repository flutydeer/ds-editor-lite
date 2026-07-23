//
// Created by fluty on 24-9-10.
//

#ifndef ISERIALIZABLE_H
#define ISERIALIZABLE_H

#include "Utils/JsonUtils.h"
#include <lite/Support/Macros.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QList>

LITE_INTERFACE ISerializable {
    I_DECL(ISerializable)
    I_NODSCD(QJsonObject serialize() const);
    I_METHOD(bool deserialize(const QJsonObject &obj));

    template <typename T>
    [[deprecated("Use JsonUtils::deserializeList instead")]]
    void deserializeJArray(const QJsonArray &jsonArray, QList<T> &list) {
        JsonUtils::deserializeList(jsonArray, list);
    }

    template <typename T>
    [[deprecated("Use JsonUtils::serializeList instead")]]
    QJsonArray serializeJArray(const QList<T> &list) const {
        return JsonUtils::serializeList(list);
    }
};

#endif // ISERIALIZABLE_H
