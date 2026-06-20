//
// Created by FlutyDeer on 2026/6/20.
//

#include "SpeakerMixData.h"

#include <QtGlobal>

#include <algorithm>

namespace SpeakerMixModel {

    namespace {
        constexpr double kWeightMin = 0.0;
        constexpr double kWeightMax = 1.0;

        QVector<double> normalizeExplicitWeights(const QVector<double> &weights) {
            QVector<double> result;
            result.reserve(weights.size());

            double sum = 0.0;
            for (const double weight : weights) {
                const double clamped = qBound(kWeightMin, weight, kWeightMax);
                result.append(clamped);
                sum += clamped;
            }

            if (sum > kWeightMax && !qFuzzyIsNull(sum)) {
                for (auto &weight : result)
                    weight /= sum;
            }
            return result;
        }

        bool normalizeInactiveExplicitWeights(QVector<double> &weights,
                                              const int explicitWeightCount) {
            if (weights.isEmpty())
                return true;
            if (weights.size() != explicitWeightCount) {
                weights.clear();
                return false;
            }
            weights = normalizeExplicitWeights(weights);
            return true;
        }

        bool normalizeInactiveKeyframes(QList<SpeakerMixKeyframe> &keyframes,
                                        const int explicitWeightCount) {
            if (keyframes.isEmpty())
                return true;
            for (auto &keyframe : keyframes) {
                if (keyframe.weights.size() != explicitWeightCount) {
                    keyframes.clear();
                    return false;
                }
                keyframe.weights = normalizeExplicitWeights(keyframe.weights);
            }
            std::sort(keyframes.begin(), keyframes.end(),
                      [](const SpeakerMixKeyframe &a, const SpeakerMixKeyframe &b) {
                          return a.tick < b.tick;
                      });
            return true;
        }

        bool hasEmptySource(const QList<SpeakerMixSource> &sources) {
            for (const auto &source : sources) {
                if (source.speaker.isEmpty())
                    return true;
            }
            return false;
        }
    }

    bool SpeakerMixSource::operator==(const SpeakerMixSource &other) const {
        return speaker == other.speaker;
    }

    bool SpeakerMixSource::operator!=(const SpeakerMixSource &other) const {
        return !(*this == other);
    }

    bool SpeakerMixKeyframe::operator==(const SpeakerMixKeyframe &other) const {
        return tick == other.tick && weights == other.weights;
    }

    bool SpeakerMixKeyframe::operator!=(const SpeakerMixKeyframe &other) const {
        return !(*this == other);
    }

    bool SpeakerMixData::operator==(const SpeakerMixData &other) const {
        return mode == other.mode && sources == other.sources &&
               fixedWeights == other.fixedWeights && dynamicKeyframes == other.dynamicKeyframes;
    }

    bool SpeakerMixData::operator!=(const SpeakerMixData &other) const {
        return !(*this == other);
    }

    SpeakerMixData normalizeSpeakerMixData(const SpeakerMixData &data) {
        SpeakerMixData result = data;
        const int sourceCount = result.sources.size();
        const int explicitWeightCount = sourceCount - 1;

        if (result.mode == SingerSourceMode::Single || sourceCount < 2 ||
            hasEmptySource(result.sources)) {
            return {};
        }

        switch (result.mode) {
            case SingerSourceMode::FixedMix:
                if (result.fixedWeights.size() != explicitWeightCount)
                    return {};
                result.fixedWeights = normalizeExplicitWeights(result.fixedWeights);
                normalizeInactiveKeyframes(result.dynamicKeyframes, explicitWeightCount);
                return result;
            case SingerSourceMode::DynamicMix:
                if (result.dynamicKeyframes.isEmpty())
                    return {};
                for (auto &keyframe : result.dynamicKeyframes) {
                    if (keyframe.weights.size() != explicitWeightCount)
                        return {};
                    keyframe.weights = normalizeExplicitWeights(keyframe.weights);
                }
                std::sort(result.dynamicKeyframes.begin(), result.dynamicKeyframes.end(),
                          [](const SpeakerMixKeyframe &a, const SpeakerMixKeyframe &b) {
                              return a.tick < b.tick;
                          });
                normalizeInactiveExplicitWeights(result.fixedWeights, explicitWeightCount);
                return result;
            case SingerSourceMode::Single:
                break;
        }
        return {};
    }

    bool isSpeakerMixDataSingle(const SpeakerMixData &data) {
        return normalizeSpeakerMixData(data).mode == SingerSourceMode::Single;
    }

    QVector<double> normalizeSpeakerMixFullWeights(const QVector<double> &weights,
                                                   const int sourceCount) {
        if (sourceCount <= 0)
            return {};

        QVector<double> result;
        result.reserve(sourceCount);
        for (int i = 0; i < sourceCount; ++i)
            result.append(qBound(kWeightMin, weights.value(i), kWeightMax));

        double sum = 0.0;
        for (const double weight : result)
            sum += weight;

        if (qFuzzyIsNull(sum)) {
            const double equalWeight = kWeightMax / sourceCount;
            for (auto &weight : result)
                weight = equalWeight;
            return result;
        }

        for (auto &weight : result)
            weight /= sum;
        return result;
    }

    QVector<double> explicitWeightsFromFullWeights(const QVector<double> &weights) {
        QVector<double> result;
        if (weights.size() <= 1)
            return result;

        const auto normalized = normalizeSpeakerMixFullWeights(weights, weights.size());
        result.reserve(normalized.size() - 1);
        for (int i = 0; i < normalized.size() - 1; ++i)
            result.append(normalized[i]);
        return result;
    }

    QVector<double> fullWeightsFromExplicitWeights(const QVector<double> &weights) {
        const auto explicitWeights = normalizeExplicitWeights(weights);
        QVector<double> result;
        result.reserve(explicitWeights.size() + 1);

        double sum = 0.0;
        for (const double weight : explicitWeights) {
            result.append(weight);
            sum += weight;
        }
        result.append(qBound(kWeightMin, kWeightMax - sum, kWeightMax));
        return normalizeSpeakerMixFullWeights(result, result.size());
    }

} // namespace SpeakerMixModel
