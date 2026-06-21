#include "SpeakerMixPreset.h"

#include <QJsonArray>

using namespace SpeakerMixModel;

namespace {

    QJsonArray encodeWeights(const QVector<double> &weights) {
        QJsonArray array;
        for (const double weight : weights)
            array.append(weight);
        return array;
    }

    QVector<double> decodeWeights(const QJsonArray &array) {
        QVector<double> weights;
        weights.reserve(array.size());
        for (const auto &value : array)
            weights.append(value.toDouble());
        return weights;
    }

    QJsonObject encodeSource(const SpeakerMixSource &source) {
        QJsonObject obj;
        if (!source.speaker.id().isEmpty())
            obj["id"] = source.speaker.id();
        if (!source.speaker.name().isEmpty())
            obj["name"] = source.speaker.name();
        return obj;
    }

    SpeakerMixSource decodeSource(const QJsonObject &obj) {
        return {SpeakerInfo(obj["id"].toString(), obj["name"].toString())};
    }

} // namespace

SingerIdentifier SpeakerMixPreset::singerIdentifier() const {
    return {singerId, packageId, packageVersion};
}

QJsonObject SpeakerMixPreset::toJson() const {
    QJsonArray sourceArray;
    for (const auto &source : sources)
        sourceArray.append(encodeSource(source));

    QJsonObject obj;
    obj["id"] = id;
    obj["name"] = name;
    obj["packageId"] = packageId;
    obj["singerId"] = singerId;
    obj["packageVersion"] = packageVersion.toString();
    obj["sources"] = sourceArray;
    obj["fixedWeights"] = encodeWeights(fixedWeights);
    obj["createdAt"] = createdAt.toUTC().toString(Qt::ISODateWithMs);
    obj["updatedAt"] = updatedAt.toUTC().toString(Qt::ISODateWithMs);
    return obj;
}

SpeakerMixPreset SpeakerMixPreset::fromJson(const QJsonObject &obj) {
    SpeakerMixPreset preset;
    preset.id = obj["id"].toString();
    preset.name = obj["name"].toString();
    preset.packageId = obj["packageId"].toString();
    preset.singerId = obj["singerId"].toString();
    preset.packageVersion = QVersionNumber::fromString(obj["packageVersion"].toString());
    preset.fixedWeights = decodeWeights(obj["fixedWeights"].toArray());
    preset.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODateWithMs);
    preset.updatedAt = QDateTime::fromString(obj["updatedAt"].toString(), Qt::ISODateWithMs);

    const auto sourceArray = obj["sources"].toArray();
    for (const auto &value : sourceArray) {
        const auto source = decodeSource(value.toObject());
        if (!source.speaker.id().isEmpty())
            preset.sources.append(source);
    }
    return preset;
}
