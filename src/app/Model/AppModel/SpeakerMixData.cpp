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

        enum class SpeakerMixValidationError {
            None,
            SingleMode,
            TooFewSources,
            EmptySource,
            InvalidFixedWeights,
            InvalidDynamicKeyframes,
            UnsupportedMode,
        };

        SpeakerMixData fallbackToSingleSpeakerMix() {
            return {};
        }

        void normalizePresetMetadata(SpeakerMixData &data) {
            data.sourcePresetId = data.sourcePresetId.trimmed();
            data.sourcePresetName = data.sourcePresetName.trimmed();
            if (data.sourcePresetId.isEmpty()) {
                data.sourcePresetName.clear();
                data.sourcePresetDirty = false;
            }
        }

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

        SpeakerMixValidationError validateSpeakerMixData(const SpeakerMixData &data) {
            if (data.mode == SingerSourceMode::Single)
                return SpeakerMixValidationError::SingleMode;

            const int sourceCount = data.sources.size();
            if (sourceCount < 2)
                return SpeakerMixValidationError::TooFewSources;
            if (hasEmptySource(data.sources))
                return SpeakerMixValidationError::EmptySource;

            const int explicitWeightCount = sourceCount - 1;
            switch (data.mode) {
                case SingerSourceMode::FixedMix:
                    if (data.fixedWeights.size() != explicitWeightCount)
                        return SpeakerMixValidationError::InvalidFixedWeights;
                    return SpeakerMixValidationError::None;
                case SingerSourceMode::DynamicMix:
                    if (data.dynamicKeyframes.isEmpty())
                        return SpeakerMixValidationError::InvalidDynamicKeyframes;
                    for (const auto &keyframe : data.dynamicKeyframes) {
                        if (keyframe.weights.size() != explicitWeightCount)
                            return SpeakerMixValidationError::InvalidDynamicKeyframes;
                    }
                    return SpeakerMixValidationError::None;
                case SingerSourceMode::Single:
                    break;
            }
            return SpeakerMixValidationError::UnsupportedMode;
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
               fixedWeights == other.fixedWeights && dynamicKeyframes == other.dynamicKeyframes &&
               sourcePresetId == other.sourcePresetId &&
               sourcePresetName == other.sourcePresetName &&
               sourcePresetDirty == other.sourcePresetDirty;
    }

    bool SpeakerMixData::operator!=(const SpeakerMixData &other) const {
        return !(*this == other);
    }

    SpeakerMixData normalizeSpeakerMixData(const SpeakerMixData &data) {
        SpeakerMixData result = data;
        normalizePresetMetadata(result);

        const auto validationError = validateSpeakerMixData(result);
        if (validationError != SpeakerMixValidationError::None)
            return fallbackToSingleSpeakerMix();

        const int explicitWeightCount = result.sources.size() - 1;
        switch (result.mode) {
            case SingerSourceMode::FixedMix:
                result.fixedWeights = normalizeExplicitWeights(result.fixedWeights);
                normalizeInactiveKeyframes(result.dynamicKeyframes, explicitWeightCount);
                return result;
            case SingerSourceMode::DynamicMix:
                for (auto &keyframe : result.dynamicKeyframes) {
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

    bool hasDynamicMixAutomation(const SpeakerMixData &data) {
        return !normalizeSpeakerMixData(data).dynamicKeyframes.isEmpty();
    }

    bool isDynamicMixActive(const SpeakerMixData &data) {
        const auto normalized = normalizeSpeakerMixData(data);
        return normalized.mode == SingerSourceMode::DynamicMix &&
               !normalized.dynamicKeyframes.isEmpty();
    }

    bool isDynamicMixBypassed(const SpeakerMixData &data) {
        const auto normalized = normalizeSpeakerMixData(data);
        return normalized.mode == SingerSourceMode::FixedMix &&
               !normalized.dynamicKeyframes.isEmpty();
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
