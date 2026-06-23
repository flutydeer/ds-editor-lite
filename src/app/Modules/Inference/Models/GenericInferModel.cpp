//
// Created by fluty on 24-9-16.
//

#include "GenericInferModel.h"

#include "Utils/JsonUtils.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <utility>

namespace {
    QJsonArray serializeSpeakerMixSources(const InferSpeakerMix &mix) {
        QJsonArray sources;
        for (const auto &source : mix.sources) {
            QJsonArray proportions;
            for (const auto proportion : source.proportions)
                proportions.append(proportion);
            sources.append(QJsonObject{
                {"speaker",     source.speaker },
                {"interval",    source.interval},
                {"proportions", proportions    }
            });
        }
        return sources;
    }

    QJsonObject serializeSpeakerMix(const InferSpeakerMix &mix) {
        return QJsonObject{
            {"fallbackSpeaker", mix.fallbackSpeaker            },
            {"sources",         serializeSpeakerMixSources(mix)}
        };
    }

    InferSpeakerMix deserializeSpeakerMix(const QJsonObject &obj) {
        InferSpeakerMix mix;
        mix.fallbackSpeaker = obj["fallbackSpeaker"].toString();
        const auto sources = obj["sources"].toArray();
        mix.sources.reserve(sources.size());
        for (const auto &sourceValue : sources) {
            const auto sourceObj = sourceValue.toObject();
            InferSpeakerMixSource source;
            source.speaker = sourceObj["speaker"].toString();
            source.interval = sourceObj["interval"].toDouble();
            const auto proportions = sourceObj["proportions"].toArray();
            source.proportions.reserve(proportions.size());
            for (const auto &proportionValue : proportions)
                source.proportions.append(proportionValue.toDouble());
            if (source.isValid())
                mix.sources.append(std::move(source));
        }
        return mix;
    }
}

InferPhoneme::InferPhoneme(QString token, const QString &languageDictId, const bool is_onset,
                           const double start)
    : token(std::move(token)), languageDictId(languageDictId), is_onset(is_onset), start(start) {
}

QJsonObject InferPhoneme::serialize() const {
    return QJsonObject{
        {"token",    token         },
        {"language", languageDictId},
        {"start",    start         }
    };
}

bool InferPhoneme::deserialize(const QJsonObject &obj) {
    token = obj["token"].toString();
    languageDictId = obj["language"].toString();
    start = obj["start"].toDouble();
    return true;
}

InferNote::InferNote(const int key, const int cents, const double duration, const bool is_rest,
                     QString glide)
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
        {"phones", JsonUtils::serializeList(phones)},
        {"notes",  JsonUtils::serializeList(notes) }
    };
}

bool InferWord::deserialize(const QJsonObject &obj) {
    JsonUtils::deserializeList(obj["phones"].toArray(), phones);
    JsonUtils::deserializeList(obj["notes"].toArray(), notes);
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
    return QJsonObject{
        {"tag",      tag                                      },
        {"dynamic",  dynamic                                  },
        {"interval", interval                                 },
        {"values",   JsonUtils::serializePrimitiveList(values)},
        {"retake",   retake.serialize()                       }
    };
}

bool InferParam::deserialize(const QJsonObject &obj) {
    tag = obj["tag"].toString();
    dynamic = obj["dynamic"].toBool();
    interval = obj["interval"].toDouble();
    values = JsonUtils::deserializePrimitiveList<double>(obj["values"].toArray());
    retake.deserialize(obj["retake"].toObject());
    return true;
}

QJsonObject GenericInferModel::serialize() const {
    return QJsonObject{
        {"offset",     offset                          },
        {"steps",      steps                           },
        {"depth",      depth                           },
        {"singer",     identifier.singerId             },
        {"speakerMix", serializeSpeakerMix(speakerMix) },
        {"words",      JsonUtils::serializeList(words) },
        {"parameters", JsonUtils::serializeList(params)}
    };
}

bool GenericInferModel::deserialize(const QJsonObject &obj) {
    offset = obj["offset"].toDouble();
    steps = obj["steps"].toInt();
    depth = static_cast<float>(obj["depth"].toDouble());

    JsonUtils::deserializeList(obj["words"].toArray(), words);
    JsonUtils::deserializeList(obj["parameters"].toArray(), params);

    identifier.packageId = obj["packageId"].toString();
    identifier.packageVersion = QVersionNumber::fromString(obj["packageVersion"].toString());
    identifier.singerId = obj["singer"].toString();
    speaker = obj["speaker"].toString();
    speakerMix = deserializeSpeakerMix(obj["speakerMix"].toObject());
    if (speakerMix.isEmpty() && !speaker.isEmpty())
        speakerMix = InferSpeakerMixModel::staticSpeakerMix(speaker);
    return true;
}

QString GenericInferModel::serializeToJson(const bool useMetadata) const {
    QJsonObject object = serialize();
    if (useMetadata) {
        object["packageId"] = identifier.packageId;
        object["packageVersion"] = identifier.packageVersion.toString();
        object["speaker"] = speaker;
    }
    return QJsonDocument{object}.toJson();
}

bool GenericInferModel::deserializeFromJson(const QString &json) {
    const QByteArray data = json.toUtf8();
    return deserialize(QJsonDocument::fromJson(data).object());
}

QString GenericInferModel::hashData() const {
    const QByteArray byteArray = serializeToJson(true).toUtf8();
    const QByteArray hashData = QCryptographicHash::hash(byteArray, QCryptographicHash::Sha1);
    return hashData.toHex();
}
