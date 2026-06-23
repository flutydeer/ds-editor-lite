#include "InferSpeakerMix.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <utility>

namespace {
    constexpr double kStaticInterval = 0.0;

    InferSpeakerMixSource makeSource(const QString &speaker, const double weight) {
        InferSpeakerMixSource source;
        source.speaker = speaker;
        source.interval = kStaticInterval;
        source.proportions = {weight};
        return source;
    }

    QString chooseFallbackSpeaker(const QList<InferSpeakerMixSource> &sources,
                                  const QString &fallbackSpeaker) {
        QString result = fallbackSpeaker;
        double maxWeight = -1.0;
        for (const auto &source : sources) {
            if (source.speaker.isEmpty() || source.proportions.isEmpty())
                continue;
            const double weight = source.proportions.first();
            if (weight > maxWeight) {
                maxWeight = weight;
                result = source.speaker;
            }
        }
        return result;
    }
}

bool InferSpeakerMixSource::isValid() const {
    return !speaker.isEmpty() && interval >= 0 && !proportions.isEmpty();
}

bool InferSpeakerMixSource::operator==(const InferSpeakerMixSource &other) const {
    return speaker == other.speaker && interval == other.interval &&
           proportions == other.proportions;
}

bool InferSpeakerMixSource::operator!=(const InferSpeakerMixSource &other) const {
    return !(*this == other);
}

bool InferSpeakerMix::isEmpty() const {
    return fallbackSpeaker.isEmpty() && sources.isEmpty();
}

QString InferSpeakerMix::signature() const {
    QJsonArray sourceArray;
    for (const auto &source : sources) {
        QJsonArray proportions;
        for (const auto proportion : source.proportions)
            proportions.append(proportion);
        sourceArray.append(QJsonObject{
            {"speaker",     source.speaker },
            {"interval",    source.interval},
            {"proportions", proportions    }
        });
    }

    const QJsonObject object{
        {"fallbackSpeaker", fallbackSpeaker},
        {"sources",         sourceArray    }
    };
    const auto data = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex();
}

bool InferSpeakerMix::operator==(const InferSpeakerMix &other) const {
    return fallbackSpeaker == other.fallbackSpeaker && sources == other.sources;
}

bool InferSpeakerMix::operator!=(const InferSpeakerMix &other) const {
    return !(*this == other);
}

namespace InferSpeakerMixModel {
    InferSpeakerMix staticSpeakerMix(const QString &speaker) {
        InferSpeakerMix mix;
        if (speaker.isEmpty())
            return mix;
        mix.fallbackSpeaker = speaker;
        mix.sources = {makeSource(speaker, 1.0)};
        return mix;
    }

    InferSpeakerMix fixedSpeakerMixFromData(const SpeakerMixModel::SpeakerMixData &data,
                                            const QString &fallbackSpeaker) {
        const auto normalized = SpeakerMixModel::normalizeSpeakerMixData(data);
        if (normalized.mode == SpeakerMixModel::SingerSourceMode::Single)
            return staticSpeakerMix(fallbackSpeaker);

        QVector<double> explicitWeights = normalized.fixedWeights;
        if (explicitWeights.isEmpty() &&
            normalized.mode == SpeakerMixModel::SingerSourceMode::DynamicMix &&
            !normalized.dynamicKeyframes.isEmpty()) {
            explicitWeights = normalized.dynamicKeyframes.first().weights;
        }

        const auto fullWeights = SpeakerMixModel::fullWeightsFromExplicitWeights(explicitWeights);
        if (fullWeights.size() != normalized.sources.size())
            return staticSpeakerMix(fallbackSpeaker);

        QList<InferSpeakerMixSource> sources;
        sources.reserve(normalized.sources.size());
        for (int i = 0; i < normalized.sources.size(); ++i) {
            const auto speaker = normalized.sources.at(i).speaker.id();
            if (speaker.isEmpty())
                return staticSpeakerMix(fallbackSpeaker);
            sources.append(makeSource(speaker, fullWeights.value(i)));
        }

        InferSpeakerMix mix;
        mix.fallbackSpeaker = chooseFallbackSpeaker(sources, fallbackSpeaker);
        mix.sources = std::move(sources);
        return mix;
    }
}
