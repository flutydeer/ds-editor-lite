//
// Created by fluty on 24-9-10.
//

#include "Phonemes.h"

#include <QJsonArray>

bool PhonemeNameSeq::editedEqualsWith(const PhonemeNameSeq &other) const {
    if (edited.count() != other.edited.count())
        return false;
    for (int i = 0; i < edited.count(); i++) {
        if (edited[i] != other.edited[i])
            return false;
    }
    return true;
}

bool PhonemeNameSeq::isEdited() const {
    return !edited.isEmpty();
}

const QList<QString> &PhonemeNameSeq::result() const {
    return edited.isEmpty() ? original : edited;
}

QJsonObject PhonemeNameSeq::serialize() const {
    QJsonArray arrOriginal;
    for (const auto &str : original)
        arrOriginal.append(str);
    QJsonArray arrEdited;
    for (const auto &str : edited)
        arrEdited.append(str);
    return QJsonObject{
        {"original", arrOriginal},
        {"edited",   arrEdited  }
    };
}

bool PhonemeNameSeq::deserialize(const QJsonObject &obj) {
    original.clear();
    for (const auto &value : obj.value("original").toArray())
        original.append(value.toString());
    edited.clear();
    for (const auto &value : obj.value("edited").toArray())
        edited.append(value.toString());
    return true;
}

bool PhonemeNameInfo::isEmpty() const {
    bool result = ahead.result().isEmpty() && normal.result().isEmpty();
    if (result) {
        qDebug() << "PhonemeNameInfo::isEmpty()"
                 << "ahead:" << ahead.result() << " normal:" << normal.result();
    }
    return result;
}

QJsonObject PhonemeNameInfo::serialize() const {
    return QJsonObject{
        {"ahead",  ahead.serialize() },
        {"normal", normal.serialize()}
    };
}

bool PhonemeNameInfo::deserialize(const QJsonObject &obj) {
    ahead.deserialize(obj.value("ahead").toObject());
    normal.deserialize(obj.value("normal").toObject());
    return true;
}

void PhonemeOffsetSeq::clear() {
    original.clear();
    edited.clear();
}

bool PhonemeOffsetSeq::isEdited() const {
    return !edited.isEmpty();
}

const QList<int> &PhonemeOffsetSeq::result() const {
    return edited.isEmpty() ? original : edited;
}

QJsonObject PhonemeOffsetSeq::serialize() const {
    QJsonArray arrOriginal;
    for (const auto &value : original)
        arrOriginal.append(value);
    QJsonArray arrEdited;
    for (const auto &value : edited)
        arrEdited.append(value);
    return QJsonObject{
        {"original", arrOriginal},
        {"edited",   arrEdited  }
    };
}

bool PhonemeOffsetSeq::deserialize(const QJsonObject &obj) {
    original.clear();
    for (const auto &value : obj.value("original").toArray())
        original.append(value.toInt());
    edited.clear();
    for (const auto &value : obj.value("edited").toArray())
        edited.append(value.toInt());
    return true;
}

bool PhonemeOffsetInfo::isEmpty() const {
    return ahead.result().isEmpty() && normal.result().isEmpty();
}

QJsonObject PhonemeOffsetInfo::serialize() const {
    return QJsonObject{
        {"ahead",  ahead.serialize() },
        {"normal", normal.serialize()}
    };
}

bool PhonemeOffsetInfo::deserialize(const QJsonObject &obj) {
    ahead.deserialize(obj.value("ahead").toObject());
    normal.deserialize(obj.value("normal").toObject());
    return true;
}

QJsonObject Phonemes::serialize() const {
    return QJsonObject{
        {"name",   nameInfo.serialize()  },
        {"offset", offsetInfo.serialize()},
    };
}

bool Phonemes::deserialize(const QJsonObject &obj) {
    nameInfo.deserialize(obj.value("name").toObject());
    offsetInfo.deserialize(obj.value("offset").toObject());
    return true;
}