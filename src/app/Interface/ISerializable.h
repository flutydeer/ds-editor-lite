//
// Created by fluty on 24-9-10.
//

#ifndef ISERIALIZABLE_H
#define ISERIALIZABLE_H

#include "Utils/Macros.h"

#include <QJsonArray>
#include <QJsonObject>

interface ISerializable {
    I_DECL(ISerializable)
    I_NODSCD(QJsonObject serialize() const);
    I_METHOD(bool deserialize(const QJsonObject &obj));

    template <typename T>
    void deserializeJArray(const QJsonArray &jsonArray, QList<T> &list) {
        list.clear();
        for (const auto &item : jsonArray) {
            T obj;
            obj.deserialize(item.toObject());
            list.append(obj);
        }
    }

    template <typename T>
    QJsonArray serializeJArray(const QList<T> &list) const {
        QJsonArray jsonArray;
        for (const auto &item : list)
            jsonArray.append(item.serialize());
        return jsonArray;
    }
};

#endif // ISERIALIZABLE_H
