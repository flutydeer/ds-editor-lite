//
// Created by fluty on 24-9-16.
//

#include "GenericInferModel.h"

#include <QJsonArray>

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
        {"tag",      tag      },
        {"dynamic",  dynamic  },
        {"interval", interval },
        {"values",   arrValues}
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
    return true;
}

QJsonObject GenericInferModel::serialize() const {
    return QJsonObject{
        {"offset", offset                 },
        {"words",  serializeJArray(words) },
        {"params", serializeJArray(params)}
    };
}

bool GenericInferModel::deserialize(const QJsonObject &obj) {
    offset = obj["offset"].toDouble();
    deserializeJArray(obj["words"].toArray(), words);
    deserializeJArray(obj["params"].toArray(), params);
    return true;
}