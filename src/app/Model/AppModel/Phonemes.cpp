//
// Created by fluty on 24-9-10.
//

#include "Phonemes.h"

#include <QJsonArray>

QJsonObject PhonemeName::serialize() const {
    return {
        {"language", language},
        {"name",     name    },
        {"isOnset",  isOnset }
    };
}

bool PhonemeName::deserialize(const QJsonObject &obj) {
    language = obj.value("language").toString();
    name = obj.value("name").toString();
    isOnset = obj.value("isOnset").toBool();
    return true;
}

bool PhonemeName::operator==(const PhonemeName &other) const {
    return language == other.language && name == other.name && isOnset == other.isOnset;
}

bool PhonemeNameSeq::editedEqualsWith(const QList<PhonemeName> &other) const {
    if (edited.count() != other.count())
        return false;
    for (int i = 0; i < edited.count(); i++) {
        if (edited[i] != other[i])
            return false;
    }
    return true;
}

bool PhonemeNameSeq::isEdited() const {
    return !edited.isEmpty();
}

const QList<PhonemeName> &PhonemeNameSeq::result() const {
    return edited.isEmpty() ? original : edited;
}

QJsonObject PhonemeNameSeq::serialize() const {
    QJsonArray arrOriginal;
    for (const auto &str : original)
        arrOriginal.append(str.serialize());
    QJsonArray arrEdited;
    for (const auto &str : edited)
        arrEdited.append(str.serialize());
    return QJsonObject{
        {"original", arrOriginal},
        {"edited",   arrEdited  }
    };
}

bool PhonemeNameSeq::deserialize(const QJsonObject &obj) {
    original.clear();
    edited.clear();
    deserializeJArray(obj.value("original").toArray(), original);
    deserializeJArray(obj.value("edited").toArray(), edited);
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

QJsonObject Phonemes::serialize() const {
    return QJsonObject{
        {"name",   nameSeq.serialize()  },
        {"offset", offsetSeq.serialize()},
    };
}

bool Phonemes::deserialize(const QJsonObject &obj) {
    nameSeq.deserialize(obj.value("name").toObject());
    offsetSeq.deserialize(obj.value("offset").toObject());
    return true;
}