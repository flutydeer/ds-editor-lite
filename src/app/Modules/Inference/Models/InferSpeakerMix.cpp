#include "InferSpeakerMix.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>
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
            double weight = 0.0;
            for (const auto proportion : source.proportions)
                weight += proportion;
            weight /= source.proportions.size();
            if (weight > maxWeight) {
                maxWeight = weight;
                result = source.speaker;
            }
        }
        return result;
    }

    QVector<double>
        interpolateExplicitWeights(const QList<SpeakerMixModel::SpeakerMixKeyframe> &keyframes,
                                   const int tick) {
        if (keyframes.isEmpty())
            return {};
        if (tick <= keyframes.first().tick)
            return keyframes.first().weights;
        if (tick >= keyframes.last().tick)
            return keyframes.last().weights;

        for (int i = 1; i < keyframes.size(); ++i) {
            const auto &right = keyframes.at(i);
            if (tick > right.tick)
                continue;

            const auto &left = keyframes.at(i - 1);
            const int span = right.tick - left.tick;
            if (span <= 0)
                return right.weights;

            const double ratio = static_cast<double>(tick - left.tick) / span;
            QVector<double> result;
            result.reserve(left.weights.size());
            for (int j = 0; j < left.weights.size(); ++j) {
                const double leftWeight = left.weights.value(j);
                const double rightWeight = right.weights.value(j);
                result.append(leftWeight + (rightWeight - leftWeight) * ratio);
            }
            return result;
        }
        return keyframes.last().weights;
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

    InferSpeakerMix
        effectiveSpeakerMixForFixedInference(const SpeakerMixModel::SpeakerMixData &data,
                                             const QString &fallbackSpeaker) {
        return fixedSpeakerMixFromData(data, fallbackSpeaker);
    }

    InferSpeakerMix dynamicSpeakerMixFromData(const SpeakerMixModel::SpeakerMixData &data,
                                              const QString &fallbackSpeaker, const int startTick,
                                              const int endTick, const Timeline &timeline,
                                              const double intervalSeconds) {
        const auto normalized = SpeakerMixModel::normalizeSpeakerMixData(data);
        if (!SpeakerMixModel::isDynamicMixActive(normalized) || endTick <= startTick ||
            intervalSeconds <= 0) {
            return fixedSpeakerMixFromData(normalized, fallbackSpeaker);
        }

        const double totalLengthSeconds = timeline.tickToSec(endTick - startTick);
        if (totalLengthSeconds <= 0)
            return fixedSpeakerMixFromData(normalized, fallbackSpeaker);

        const int frames = qMax(1, qRound(totalLengthSeconds / intervalSeconds));
        QList<InferSpeakerMixSource> sources;
        sources.reserve(normalized.sources.size());
        for (const auto &sourceData : normalized.sources) {
            const auto speaker = sourceData.speaker.id();
            if (speaker.isEmpty())
                return fixedSpeakerMixFromData(normalized, fallbackSpeaker);
            InferSpeakerMixSource source;
            source.speaker = speaker;
            source.interval = intervalSeconds;
            source.proportions.reserve(frames);
            sources.append(std::move(source));
        }

        const double intervalTicks = timeline.secToTick(intervalSeconds);
        for (int frame = 0; frame < frames; ++frame) {
            const int sampleTick = startTick + qRound(intervalTicks * frame);
            const auto fullWeights = SpeakerMixModel::fullWeightsFromExplicitWeights(
                interpolateExplicitWeights(normalized.dynamicKeyframes, sampleTick));
            if (fullWeights.size() != sources.size())
                return fixedSpeakerMixFromData(normalized, fallbackSpeaker);
            for (int i = 0; i < sources.size(); ++i)
                sources[i].proportions.append(fullWeights.value(i));
        }

        InferSpeakerMix mix;
        mix.fallbackSpeaker = chooseFallbackSpeaker(sources, fallbackSpeaker);
        mix.sources = std::move(sources);
        return mix;
    }

    InferSpeakerMix effectiveSpeakerMixFromData(const SpeakerMixModel::SpeakerMixData &data,
                                                const QString &fallbackSpeaker, const int startTick,
                                                const int endTick, const Timeline &timeline,
                                                const double intervalSeconds) {
        if (SpeakerMixModel::isDynamicMixActive(data)) {
            return dynamicSpeakerMixFromData(data, fallbackSpeaker, startTick, endTick, timeline,
                                             intervalSeconds);
        }
        return fixedSpeakerMixFromData(data, fallbackSpeaker);
    }
}
