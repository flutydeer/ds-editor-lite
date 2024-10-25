//
// Created by fluty on 24-9-16.
//

#include "GenericInferModel.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>

InferPhoneme::InferPhoneme(QString token, const QString &language, bool is_onset, double start)
    : token(std::move(token)), language(language), is_onset(is_onset), start(start) {
}

QJsonObject InferPhoneme::serialize() const {
    return QJsonObject{
        {"token",    token   },
        {"language", language},
        {"start",    start   }
    };
}

bool InferPhoneme::deserialize(const QJsonObject &obj) {
    token = obj["token"].toString();
    language = obj["language"].toString();
    start = obj["start"].toDouble();
    return true;
}

InferNote::InferNote(int key, int cents, double duration, bool is_rest, QString glide)
    : key(key), cents(cents), duration(duration), is_rest(is_rest), glide(std::move(glide)) {
}

QJsonObject InferNote::serialize() const {
    return QJsonObject{
        {"key",      key     },
        {"cents",    cents   },
        {"duration", duration},
        {"glide",    glide   },
        {"is_rest",  is_rest }
    };
}

bool InferNote::deserialize(const QJsonObject &obj) {
    key = obj["key"].toInt();
    cents = obj["cents"].toInt();
    duration = obj["duration"].toDouble();
    glide = obj["glide"].toString();
    is_rest = obj["is_rest"].toBool();
    return true;
}

InferWord::InferWord(QList<InferPhoneme> phones, QList<InferNote> notes)
    : phones(std::move(phones)), notes(std::move(notes)) {
}

QJsonObject InferWord::serialize() const {
    return QJsonObject{
        {"phones", serializeJArray(phones)},
        {"notes",  serializeJArray(notes) }
    };
}

bool InferWord::deserialize(const QJsonObject &obj) {
    deserializeJArray(obj["phones"].toArray(), phones);
    deserializeJArray(obj["notes"].toArray(), notes);
    return true;
}

double InferWord::length() const {
    double result = 0.0;
    for (const auto &note : notes)
        result += note.duration;
    return result;
}

QJsonObject InferRetake::serialize() const {
    return QJsonObject{
        {"start", start},
        {"end",   end  }
    };
}

bool InferRetake::deserialize(const QJsonObject &obj) {
    start = obj["start"].toInt();
    end = obj["end"].toInt();
    return true;
}

QJsonObject InferParam::serialize() const {
    QJsonArray arrValues;
    for (const auto &value : values)
        arrValues.append(value);

    return QJsonObject{
        {"tag",      tag               },
        {"dynamic",  dynamic           },
        {"interval", interval          },
        {"values",   arrValues         },
        {"retake",   retake.serialize()}
    };
}

bool InferParam::deserialize(const QJsonObject &obj) {
    tag = obj["tag"].toString();
    dynamic = obj["dynamic"].toBool();
    interval = obj["interval"].toDouble();
    QList<double> newValues;
    for (const auto &value : obj["values"].toArray())
        newValues.append(value.toDouble());
    values = newValues;
    retake.deserialize(obj["retake"].toObject());
    return true;
}

QJsonObject GenericInferModel::serialize() const {
    return QJsonObject{
        {"offset",     offset                 },
        {"words",      serializeJArray(words) },
        {"parameters", serializeJArray(params)}
    };
}

bool GenericInferModel::deserialize(const QJsonObject &obj) {
    offset = obj["offset"].toDouble();
    deserializeJArray(obj["words"].toArray(), words);
    deserializeJArray(obj["parameters"].toArray(), params);
    return true;
}

QString GenericInferModel::serializeToJson() const {
    return QJsonDocument{serialize()}.toJson();
}

bool GenericInferModel::deserializeFromJson(const QString &json) {
    const QByteArray data = json.toUtf8();
    return deserialize(QJsonDocument::fromJson(data).object());
}

QString GenericInferModel::hashData() const {
    const QByteArray byteArray = serializeToJson().toUtf8();
    const QByteArray hashData = QCryptographicHash::hash(byteArray, QCryptographicHash::Sha1);
    return hashData.toHex();
}