//
// Created by fluty on 24-3-15.
//

#ifndef JSONUTILS_H
#define JSONUTILS_H

#include <lite/Support/Log.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

class JsonUtils {
public:
    static bool load(const QString &filename, QJsonObject &jsonObj);
    static bool save(const QString &filename, const QJsonObject &jsonObj);

    template <typename T>
    static QJsonArray serializeList(const QList<T> &list) {
        QJsonArray arr;
        for (const auto &item : list)
            arr.append(item.serialize());
        return arr;
    }

    template <typename T>
    static void deserializeList(const QJsonArray &arr, QList<T> &list) {
        list.clear();
        for (const auto &v : arr) {
            T obj;
            obj.deserialize(v.toObject());
            list.append(std::move(obj));
        }
    }

    template <typename T>
    static QJsonArray serializeListToJson(const QList<T> &list) {
        QJsonArray arr;
        for (const auto &item : list)
            arr.append(item.toJson());
        return arr;
    }

    template <typename T>
    static QList<T> deserializeListFromJson(const QJsonArray &arr) {
        QList<T> list;
        for (const auto &v : arr)
            list.append(T::fromJson(v.toObject()));
        return list;
    }

    template <typename T>
    static QJsonArray serializePrimitiveList(const QList<T> &list) {
        QJsonArray arr;
        for (const auto &v : list)
            arr.append(v);
        return arr;
    }

    template <typename T>
    static QList<T> deserializePrimitiveList(const QJsonArray &arr) {
        QList<T> list;
        for (const auto &v : arr)
            list.append(v.toVariant().value<T>());
        return list;
    }

    static QJsonArray serializeStringList(const QStringList &list);
    static QStringList deserializeStringList(const QJsonArray &arr);
};

inline bool JsonUtils::load(const QString &filename, QJsonObject &jsonObj) {
    {
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly)) {
            Log::e("JsonUtils", "Failed to open file for reading: " + filename);
            return false;
        }

        const auto data = file.readAll();
        file.close();
        QJsonParseError err;
        const QJsonDocument json = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError) {
            Log::e("JsonUtils", QString("Failed to parse json file: %1 error: %2")
                                    .arg(filename, err.errorString()));
            return false;
        }
        if (json.isObject())
            jsonObj = json.object();
        else {
            Log::e("JsonUtils", "JSON is not an object: " + filename);
            return false;
        }
        return true;
    }
}

inline bool JsonUtils::save(const QString &filename, const QJsonObject &jsonObj) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        Log::e("JsonUtils", "Failed to open file for writing: " + filename);
        return false;
    }

    QJsonDocument doc;
    doc.setObject(jsonObj);
    file.write(doc.toJson());
    return true;
}

inline QJsonArray JsonUtils::serializeStringList(const QStringList &list) {
    return QJsonArray::fromStringList(list);
}

inline QStringList JsonUtils::deserializeStringList(const QJsonArray &arr) {
    QStringList list;
    for (const auto &v : arr)
        list.append(v.toString());
    return list;
}

#endif // JSONUTILS_H
