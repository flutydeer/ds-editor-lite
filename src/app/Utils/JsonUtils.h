//
// Created by fluty on 24-3-15.
//

#ifndef JSONUTILS_H
#define JSONUTILS_H

#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>

class JsonUtils {
public:
    static bool load(const QString &filename, QJsonObject &jsonObj);
    static bool save(const QString &filename, const QJsonObject &jsonObj);
};

inline bool JsonUtils::load(const QString &filename, QJsonObject &jsonObj) {
    {
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly))
            return false;

        auto data = file.readAll();
        file.close();
        QJsonParseError err;
        QJsonDocument json = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError)
            return false;
        if (json.isObject())
            jsonObj = json.object();
        return true;
    }
}
inline bool JsonUtils::save(const QString &filename, const QJsonObject &jsonObj) {
    QFile file(filename);
    file.open(QFile::WriteOnly);

    QJsonDocument doc;
    doc.setObject(jsonObj);
    file.write(doc.toJson());
    return true;
}

#endif // JSONUTILS_H
