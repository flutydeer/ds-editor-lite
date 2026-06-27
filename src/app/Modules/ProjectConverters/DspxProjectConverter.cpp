//
// Created by hrukalive on 2/7/24.
//

#include "DspxProjectConverter.h"

#include "Model/AppModel/AnchorCurve.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppStatus/AppStatus.h"

#include <opendspx/model.h>
#include <opendspx/singlesinger.h>
#include <opendspx/mixedsinger.h>
#include <opendspxserializer/serializer.h>

#include "Model/AppModel/Track.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/Params.h"
#include "Model/AppModel/Curve.h"
#include "Model/AppModel/DrawCurve.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppModel/SpeakerMixData.h"
#include "Modules/PackageManager/PackageManager.h"

#include <QDebug>
#include <QFile>

#include <algorithm>

namespace {
    using namespace SpeakerMixModel;

    void warnIfPackageMetadataNotReady(const SingerIdentifier &identifier) {
        if (appStatus->packageModuleStatus == AppStatus::ModuleStatus::Ready)
            return;

        qWarning() << "DspxProjectConverter is resolving singer before package metadata is ready:"
                   << identifier;
    }

    class JsonNlohmann {
    public:
        static nlohmann::json fromQJsonValue(const QJsonValue &v) {
            if (v.isString())
                return v.toString().toStdString();

            if (v.isBool())
                return v.toBool();

            if (v.isDouble())
                return v.toDouble();

            if (v.isArray()) {
                nlohmann::json ret = nlohmann::json::array();
                std::ranges::transform(v.toArray(), std::back_inserter(ret),
                                       &JsonNlohmann::fromQJsonValue);
                return ret;
            }

            if (v.isObject()) {
                nlohmann::json ret = nlohmann::json::object();
                for (auto [key, value] : v.toObject().asKeyValueRange()) {
                    ret[key.toString().toStdString()] = fromQJsonValue(value);
                }
                return ret;
            }

            return {};
        }

        static QJsonValue toQJsonValue(const nlohmann::json &v) {
            if (v.is_string())
                return QString::fromStdString(v.get<std::string>());

            if (v.is_boolean())
                return v.get<bool>();

            if (v.is_number())
                return v.get<double>();

            if (v.is_array()) {
                QJsonArray ret;
                std::ranges::transform(v, std::back_inserter(ret), &JsonNlohmann::toQJsonValue);
                return ret;
            }

            if (v.is_object()) {
                QJsonObject ret;
                for (auto it = v.begin(); it != v.end(); ++it) {
                    ret[it.key().c_str()] = toQJsonValue(it.value());
                }
                return ret;
            }

            return {};
        }
    };

    // ========== DS Editor Lite workspace helpers ==========

    constexpr const char *kDsWorkspaceKey = "ds-editor-lite";
    constexpr int kDsWorkspaceSchemaVersion = 1;

    // Read the "ds-editor-lite" key from an opendspx Workspace as QJsonObject
    QJsonObject dsWorkspaceFrom(const opendspx::Workspace &workspace) {
        auto it = workspace.find(kDsWorkspaceKey);
        if (it == workspace.end())
            return {};
        return JsonNlohmann::toQJsonValue(it->second).toObject();
    }

    // Write a QJsonObject into the "ds-editor-lite" key of an opendspx Workspace, preserving other
    // keys
    void writeDsWorkspace(opendspx::Workspace &workspace, const QJsonObject &obj) {
        if (obj.isEmpty())
            return;
        workspace[kDsWorkspaceKey] = JsonNlohmann::fromQJsonValue(obj);
    }

    // ---- Identifier helpers ----

    QJsonObject encodeSingerIdentifier(const SingerIdentifier &identifier, bool includeSingerId) {
        QJsonObject obj;
        if (includeSingerId && !identifier.singerId.isEmpty())
            obj["singerId"] = identifier.singerId;
        if (!identifier.packageId.isEmpty())
            obj["packageId"] = identifier.packageId;
        if (!identifier.packageVersion.isNull())
            obj["packageVersion"] = identifier.packageVersion.toString();
        return obj;
    }

    SingerIdentifier decodeSingerIdentifier(const QJsonObject &obj,
                                            const QString &officialSingerId = {}) {
        SingerIdentifier id;
        id.singerId = officialSingerId.isEmpty() ? obj["singerId"].toString() : officialSingerId;
        id.packageId = obj["packageId"].toString();
        id.packageVersion = QVersionNumber::fromString(obj["packageVersion"].toString());
        return id;
    }

    // ---- SingerInfo helpers ----

    QJsonObject encodeSingerInfoForWorkspace(const SingerInfo &singerInfo, bool includeSingerId) {
        if (singerInfo.isEmpty())
            return {};
        QJsonObject obj;
        obj["schemaVersion"] = kDsWorkspaceSchemaVersion;
        auto identifierObj = encodeSingerIdentifier(singerInfo.identifier(), includeSingerId);
        if (!identifierObj.isEmpty())
            obj["identifier"] = identifierObj;
        if (!singerInfo.name().isEmpty())
            obj["name"] = singerInfo.name();
        if (!singerInfo.defaultLanguage().isEmpty())
            obj["defaultLanguage"] = singerInfo.defaultLanguage();
        return obj;
    }

    SingerInfo decodeSingerInfoFromWorkspace(const QJsonObject &obj,
                                             const QString &officialSingerId = {}) {
        if (obj.isEmpty() && officialSingerId.isEmpty())
            return {};

        auto identifierObj = obj["identifier"].toObject();
        auto identifier = decodeSingerIdentifier(identifierObj, officialSingerId);

        if (identifier.isEmpty())
            return {};

        // Try to resolve from package manager
        warnIfPackageMetadataNotReady(identifier);
        auto resolved = packageManager->findSingerByIdentifier(identifier);
        if (!resolved.isEmpty())
            return resolved;

        // Construct fallback SingerInfo
        auto name = obj["name"].toString();
        auto defaultLanguage = obj["defaultLanguage"].toString();
        return SingerInfo(identifier, name, {}, {}, defaultLanguage, {});
    }

    // ---- SpeakerInfo helpers ----

    QJsonObject encodeSpeakerInfoForWorkspace(const SpeakerInfo &speakerInfo) {
        if (speakerInfo.isEmpty())
            return {};
        QJsonObject obj;
        if (!speakerInfo.id().isEmpty())
            obj["id"] = speakerInfo.id();
        if (!speakerInfo.name().isEmpty())
            obj["name"] = speakerInfo.name();
        if (!speakerInfo.toneMin().isEmpty())
            obj["toneMin"] = speakerInfo.toneMin();
        if (!speakerInfo.toneMax().isEmpty())
            obj["toneMax"] = speakerInfo.toneMax();
        return obj;
    }

    SpeakerInfo decodeSpeakerInfoFromWorkspace(const QJsonObject &obj) {
        if (obj.isEmpty())
            return {};
        auto id = obj["id"].toString();
        auto name = obj["name"].toString();
        auto toneMin = obj["toneMin"].toString();
        auto toneMax = obj["toneMax"].toString();
        if (id.isEmpty())
            return {};
        return SpeakerInfo(id, name, toneMin, toneMax);
    }

    SpeakerInfo resolveSpeakerInfo(const SingerInfo &singerInfo, const SpeakerInfo &fallback) {
        if (fallback.isEmpty())
            return {};
        // Try to find the speaker by id in the singer's speaker list
        for (const auto &speaker : singerInfo.speakers()) {
            if (speaker.id() == fallback.id())
                return speaker;
        }
        return fallback;
    }

    SpeakerInfo resolveSpeakerInfoStrict(const SingerInfo &singerInfo,
                                         const SpeakerInfo &fallback) {
        if (fallback.isEmpty())
            return {};
        for (const auto &speaker : singerInfo.speakers()) {
            if (speaker.id() == fallback.id())
                return speaker;
        }
        return {};
    }

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

    QJsonObject encodeSpeakerMixDataForWorkspace(const SpeakerMixData &data) {
        const auto normalized = normalizeSpeakerMixData(data);
        if (normalized.mode == SingerSourceMode::Single)
            return {};

        QJsonObject obj;
        obj["mode"] = normalized.mode == SingerSourceMode::FixedMix ? "fixed" : "dynamic";
        obj["dynamicBypassed"] = normalized.dynamicBypassed;

        QJsonArray sources;
        for (const auto &source : normalized.sources)
            sources.append(encodeSpeakerInfoForWorkspace(source.speaker));
        obj["sources"] = sources;

        if (!normalized.fixedWeights.isEmpty())
            obj["fixedWeights"] = encodeWeights(normalized.fixedWeights);

        if (!normalized.sourcePresetId.isEmpty()) {
            QJsonObject presetObj;
            presetObj["id"] = normalized.sourcePresetId;
            presetObj["name"] = normalized.sourcePresetName;
            presetObj["dirty"] = normalized.sourcePresetDirty;
            obj["sourcePreset"] = presetObj;
        }

        if (!normalized.dynamicKeyframes.isEmpty()) {
            QJsonArray keyframes;
            for (const auto &keyframe : normalized.dynamicKeyframes) {
                QJsonObject keyframeObj;
                keyframeObj["tick"] = keyframe.tick;
                keyframeObj["weights"] = encodeWeights(keyframe.weights);
                keyframes.append(keyframeObj);
            }
            obj["dynamicKeyframes"] = keyframes;
        }
        return obj;
    }

    SpeakerMixData decodeSpeakerMixDataFromWorkspace(const QJsonObject &obj,
                                                     const SingerInfo &effectiveSinger) {
        if (obj.isEmpty())
            return {};

        const auto modeText = obj["mode"].toString();
        SpeakerMixData data;
        if (modeText == "fixed") {
            data.mode = SingerSourceMode::FixedMix;
        } else if (modeText == "dynamic") {
            data.mode = SingerSourceMode::DynamicMix;
        } else {
            return {};
        }
        data.dynamicBypassed = obj["dynamicBypassed"].toBool();

        const auto sourcesArray = obj["sources"].toArray();
        if (sourcesArray.size() < 2)
            return {};

        for (const auto &sourceValue : sourcesArray) {
            const auto speaker = resolveSpeakerInfoStrict(
                effectiveSinger, decodeSpeakerInfoFromWorkspace(sourceValue.toObject()));
            if (speaker.isEmpty())
                return {};
            data.sources.append({speaker});
        }

        if (obj.contains("fixedWeights"))
            data.fixedWeights = decodeWeights(obj["fixedWeights"].toArray());

        const auto presetObj = obj["sourcePreset"].toObject();
        data.sourcePresetId = presetObj["id"].toString();
        data.sourcePresetName = presetObj["name"].toString();
        data.sourcePresetDirty = presetObj["dirty"].toBool();

        const auto keyframesArray = obj["dynamicKeyframes"].toArray();
        if (!keyframesArray.isEmpty()) {
            for (const auto &keyframeValue : keyframesArray) {
                const auto keyframeObj = keyframeValue.toObject();
                SpeakerMixKeyframe keyframe;
                keyframe.tick = keyframeObj["tick"].toInt();
                keyframe.weights = decodeWeights(keyframeObj["weights"].toArray());
                data.dynamicKeyframes.append(keyframe);
            }
        }

        return normalizeSpeakerMixData(data);
    }

    // ---- opendspx Singer helpers ----

    constexpr const char *kDiffSingerCategory = "diffscope-synth:diffsinger";

    struct DecodedSingerSource {
        SingerInfo singerInfo;
        SpeakerInfo speakerInfo;
        SpeakerMixData speakerMixData;
        bool hasSinger = false;
    };

    struct WeightedSingerSource {
        SingerInfo singerInfo;
        SpeakerInfo speakerInfo;
        double weight = 1.0;
    };

    struct SpeakerWeight {
        SpeakerInfo speaker;
        double weight = 0.0;
    };

    QString encodeDspxSingerId(const SingerIdentifier &identifier) {
        if (identifier.packageId.isEmpty() || identifier.packageVersion.isNull() ||
            identifier.singerId.isEmpty())
            return {};
        return QString("%1@%2[%3]")
            .arg(identifier.packageId, identifier.packageVersion.toString(), identifier.singerId);
    }

    SingerIdentifier decodeDspxSingerId(const QString &idText) {
        const auto atIndex = idText.indexOf('@');
        const auto leftBracketIndex = idText.indexOf('[', atIndex + 1);
        const auto rightBracketIndex = idText.lastIndexOf(']');
        if (atIndex <= 0 || leftBracketIndex <= atIndex + 1 ||
            rightBracketIndex != idText.size() - 1 || rightBracketIndex <= leftBracketIndex + 1)
            return {};

        SingerIdentifier identifier;
        identifier.packageId = idText.left(atIndex);
        identifier.packageVersion =
            QVersionNumber::fromString(idText.mid(atIndex + 1, leftBracketIndex - atIndex - 1));
        identifier.singerId =
            idText.mid(leftBracketIndex + 1, rightBracketIndex - leftBracketIndex - 1);
        if (identifier.packageId.isEmpty() || identifier.packageVersion.isNull() ||
            identifier.singerId.isEmpty())
            return {};
        return identifier;
    }

    SingerInfo resolveSingerInfo(const SingerIdentifier &identifier, const QJsonObject &fallback) {
        if (identifier.isEmpty())
            return {};

        warnIfPackageMetadataNotReady(identifier);
        auto resolved = packageManager->findSingerByIdentifier(identifier);
        if (!resolved.isEmpty())
            return resolved;

        return SingerInfo(identifier, fallback["singerName"].toString(), {}, {},
                          fallback["defaultLanguage"].toString(), {});
    }

    QJsonObject encodeSingerSourceWorkspace(const SingerInfo &singerInfo,
                                            const SpeakerInfo &speakerInfo) {
        QJsonObject obj;
        obj["schemaVersion"] = kDsWorkspaceSchemaVersion;
        if (!singerInfo.name().isEmpty())
            obj["singerName"] = singerInfo.name();
        if (!singerInfo.defaultLanguage().isEmpty())
            obj["defaultLanguage"] = singerInfo.defaultLanguage();

        auto speakerObj = encodeSpeakerInfoForWorkspace(speakerInfo);
        if (!speakerObj.isEmpty())
            obj["speaker"] = speakerObj;
        return obj;
    }

    QJsonObject encodeSingerExtra(const SpeakerInfo &speakerInfo) {
        QJsonObject obj;
        obj["speaker"] = speakerInfo.id();
        return obj;
    }

    opendspx::SingerRef encodeSingleSingerRef(const SingerInfo &singerInfo,
                                              const SpeakerInfo &speakerInfo) {
        auto singer = std::make_shared<opendspx::SingleSinger>();
        singer->id = encodeDspxSingerId(singerInfo.identifier()).toStdString();
        singer->extra = JsonNlohmann::fromQJsonValue(encodeSingerExtra(speakerInfo));
        writeDsWorkspace(singer->workspace, encodeSingerSourceWorkspace(singerInfo, speakerInfo));
        return singer;
    }

    opendspx::SourceMixingRatio encodeSourceMixingRatio(const QVector<double> &weights) {
        opendspx::SourceMixingRatio ratio;
        ratio.reserve(static_cast<size_t>(weights.size()));
        for (const auto weight : weights)
            ratio.push_back(weight);
        return ratio;
    }

    QVector<double> fullWeightsFromSourceRatio(const opendspx::SourceMixingRatio &ratio,
                                               const int sourceCount) {
        if (sourceCount <= 0)
            return {};

        QVector<double> weights;
        weights.reserve(sourceCount);
        double sum = 0.0;
        for (int i = 0; i < sourceCount - 1; ++i) {
            const auto weight = i < static_cast<int>(ratio.size()) ? ratio[i] : 0.0;
            weights.append(weight);
            sum += weight;
        }
        weights.append(1.0 - sum);
        return weights;
    }

    QVector<double> unitWeights(const int sourceCount) {
        QVector<double> weights;
        weights.resize(sourceCount);
        std::fill(weights.begin(), weights.end(), 1.0);
        return weights;
    }

    QJsonObject encodeSourceMixStateForWorkspace(const SpeakerMixData &data) {
        const auto normalized = normalizeSpeakerMixData(data);
        QJsonObject obj;

        if (normalized.mode == SingerSourceMode::DynamicMix) {
            obj["dynamicBypassed"] = normalized.dynamicBypassed;
            if (!normalized.fixedWeights.isEmpty())
                obj["fixedWeights"] = encodeWeights(normalized.fixedWeights);
        }

        if (!normalized.sourcePresetId.isEmpty()) {
            QJsonObject presetObj;
            presetObj["id"] = normalized.sourcePresetId;
            presetObj["name"] = normalized.sourcePresetName;
            presetObj["dirty"] = normalized.sourcePresetDirty;
            obj["sourcePreset"] = presetObj;
        }

        return obj;
    }

    void applySourceMixStateFromWorkspace(SpeakerMixData &data, const QJsonObject &obj) {
        if (obj.isEmpty())
            return;

        if (data.mode == SingerSourceMode::DynamicMix) {
            data.dynamicBypassed = obj["dynamicBypassed"].toBool();
            if (obj.contains("fixedWeights"))
                data.fixedWeights = decodeWeights(obj["fixedWeights"].toArray());
        }

        const auto presetObj = obj["sourcePreset"].toObject();
        data.sourcePresetId = presetObj["id"].toString();
        data.sourcePresetName = presetObj["name"].toString();
        data.sourcePresetDirty = presetObj["dirty"].toBool();
        data = normalizeSpeakerMixData(data);
    }

    std::optional<WeightedSingerSource> decodeSingleSingerSource(
        const std::shared_ptr<opendspx::SingleSinger> &single, const double weight) {
        const auto identifier = decodeDspxSingerId(QString::fromStdString(single->id));
        if (identifier.isEmpty())
            return std::nullopt;

        const auto sourceDsWs = dsWorkspaceFrom(single->workspace);
        const auto singerInfo = resolveSingerInfo(identifier, sourceDsWs);
        if (singerInfo.isEmpty())
            return std::nullopt;

        const auto extra = JsonNlohmann::toQJsonValue(single->extra).toObject();
        QString speakerId;
        if (extra.contains("speaker")) {
            speakerId = extra["speaker"].toString();
        } else {
            speakerId = sourceDsWs["speaker"].toObject()["id"].toString();
        }

        SpeakerInfo speakerInfo;
        if (!speakerId.isEmpty()) {
            const auto workspaceSpeaker =
                decodeSpeakerInfoFromWorkspace(sourceDsWs["speaker"].toObject());
            const auto fallbackSpeaker =
                workspaceSpeaker.id() == speakerId ? workspaceSpeaker : SpeakerInfo(speakerId);
            speakerInfo = resolveSpeakerInfo(singerInfo, fallbackSpeaker);
        }

        return WeightedSingerSource{singerInfo, speakerInfo, weight};
    }

    QVector<WeightedSingerSource> flattenSingerSource(const opendspx::SingerRef &singerRef,
                                                      const double weight) {
        QVector<WeightedSingerSource> result;
        if (!singerRef)
            return result;

        if (singerRef->type == opendspx::Singer::Type::Single) {
            auto single = std::static_pointer_cast<opendspx::SingleSinger>(singerRef);
            const auto source = decodeSingleSingerSource(single, weight);
            if (source.has_value())
                result.append(*source);
            return result;
        }

        auto mixed = std::static_pointer_cast<opendspx::MixedSinger>(singerRef);
        const auto weights =
            fullWeightsFromSourceRatio(mixed->ratio, static_cast<int>(mixed->singers.size()));
        for (int i = 0; i < static_cast<int>(mixed->singers.size()); ++i)
            result.append(flattenSingerSource(mixed->singers[i], weight * weights.value(i)));
        return result;
    }

    QVector<WeightedSingerSource> flattenSingerSources(
        const std::vector<opendspx::SingerRef> &singers, const QVector<double> &weights) {
        QVector<WeightedSingerSource> result;
        for (int i = 0; i < static_cast<int>(singers.size()); ++i)
            result.append(flattenSingerSource(singers[i], weights.value(i)));
        return result;
    }

    QVector<SpeakerWeight> mergeSpeakerWeights(const QVector<WeightedSingerSource> &sources,
                                               const SingerIdentifier &identifier) {
        QVector<SpeakerWeight> result;
        for (const auto &source : sources) {
            if (source.singerInfo.identifier() != identifier)
                continue;

            const auto speakerId = source.speakerInfo.id();
            auto it = std::find_if(result.begin(), result.end(), [&](const auto &item) {
                return item.speaker.id() == speakerId;
            });
            if (it == result.end()) {
                result.append({source.speakerInfo, source.weight});
            } else {
                it->weight += source.weight;
            }
        }
        return result;
    }

    bool canUseSpeakerMixSources(const QVector<SpeakerWeight> &speakerWeights) {
        if (speakerWeights.size() < 2)
            return false;
        return std::ranges::none_of(speakerWeights, [](const auto &source) {
            return source.speaker.isEmpty();
        });
    }

    SpeakerMixData fixedSpeakerMixFromWeights(const QVector<SpeakerWeight> &speakerWeights,
                                              const QJsonObject &sourceMixState) {
        SpeakerMixData data;
        if (!canUseSpeakerMixSources(speakerWeights))
            return data;

        data.mode = SingerSourceMode::FixedMix;
        QVector<double> fullWeights;
        fullWeights.reserve(speakerWeights.size());
        for (const auto &source : speakerWeights) {
            data.sources.append({source.speaker});
            fullWeights.append(source.weight);
        }
        data.fixedWeights = explicitWeightsFromFullWeights(fullWeights);
        applySourceMixStateFromWorkspace(data, sourceMixState);
        return normalizeSpeakerMixData(data);
    }

    DecodedSingerSource decodedSingleSourceFromFlattened(
        const QVector<WeightedSingerSource> &flattenedSources) {
        DecodedSingerSource result;
        if (flattenedSources.isEmpty())
            return result;

        result.singerInfo = flattenedSources.first().singerInfo;
        result.speakerInfo = flattenedSources.first().speakerInfo;
        result.hasSinger = true;
        return result;
    }

    DecodedSingerSource decodeFixedSingerSources(const opendspx::Sources &sources,
                                                 const QJsonObject &sourceMixState) {
        const auto flattenedSources = flattenSingerSource(sources.singers.front(), 1.0);
        auto result = decodedSingleSourceFromFlattened(flattenedSources);
        if (!result.hasSinger)
            return result;

        const auto speakerWeights =
            mergeSpeakerWeights(flattenedSources, result.singerInfo.identifier());
        result.speakerMixData = fixedSpeakerMixFromWeights(speakerWeights, sourceMixState);
        return result;
    }

    DecodedSingerSource decodeDynamicSingerSources(const opendspx::Sources &sources,
                                                   const QJsonObject &sourceMixState) {
        const auto sourceOrder = flattenSingerSources(
            sources.singers, unitWeights(static_cast<int>(sources.singers.size())));
        auto result = decodedSingleSourceFromFlattened(sourceOrder);
        if (!result.hasSinger)
            return result;

        const auto identifier = result.singerInfo.identifier();
        const auto speakerWeights = mergeSpeakerWeights(sourceOrder, identifier);
        if (!canUseSpeakerMixSources(speakerWeights))
            return result;

        SpeakerMixData data;
        data.mode = SingerSourceMode::DynamicMix;
        for (const auto &source : speakerWeights)
            data.sources.append({source.speaker});

        for (const auto &anchor : sources.mix) {
            const auto topLevelWeights =
                fullWeightsFromSourceRatio(anchor.ratio, static_cast<int>(sources.singers.size()));
            const auto flattenedAnchorSources = flattenSingerSources(sources.singers,
                                                                     topLevelWeights);
            const auto mergedWeights = mergeSpeakerWeights(flattenedAnchorSources, identifier);

            QVector<double> fullWeights;
            fullWeights.reserve(speakerWeights.size());
            for (const auto &source : speakerWeights) {
                auto it =
                    std::find_if(mergedWeights.begin(), mergedWeights.end(), [&](const auto &item) {
                        return item.speaker.id() == source.speaker.id();
                    });
                fullWeights.append(it == mergedWeights.end() ? 0.0 : it->weight);
            }

            SpeakerMixKeyframe keyframe;
            keyframe.tick = anchor.pos;
            keyframe.weights = explicitWeightsFromFullWeights(fullWeights);
            data.dynamicKeyframes.append(keyframe);
        }

        applySourceMixStateFromWorkspace(data, sourceMixState);
        result.speakerMixData = normalizeSpeakerMixData(data);
        return result;
    }

    DecodedSingerSource decodeDspxSingerSources(const opendspx::Sources &sources,
                                                const QJsonObject &sourceMixState) {
        if (sources.category != kDiffSingerCategory || sources.singers.empty())
            return {};

        if (!sources.mix.empty())
            return decodeDynamicSingerSources(sources, sourceMixState);

        if (sources.singers.size() == 1 &&
            sources.singers.front()->type == opendspx::Singer::Type::Mixed)
            return decodeFixedSingerSources(sources, sourceMixState);

        const auto flattenedSources = flattenSingerSource(sources.singers.front(), 1.0);
        return decodedSingleSourceFromFlattened(flattenedSources);
    }

    std::optional<opendspx::Sources> encodeDspxSingerSources(const SingerInfo &singerInfo,
                                                             const SpeakerInfo &speakerInfo,
                                                             const SpeakerMixData &mixData) {
        if (encodeDspxSingerId(singerInfo.identifier()).isEmpty())
            return std::nullopt;

        opendspx::Sources sources;
        sources.category = kDiffSingerCategory;
        const auto normalizedMix = normalizeSpeakerMixData(mixData);

        if (normalizedMix.mode == SingerSourceMode::FixedMix) {
            auto mixed = std::make_shared<opendspx::MixedSinger>();
            mixed->ratio = encodeSourceMixingRatio(normalizedMix.fixedWeights);
            for (const auto &source : normalizedMix.sources)
                mixed->singers.push_back(encodeSingleSingerRef(singerInfo, source.speaker));
            sources.singers.push_back(mixed);
            return sources;
        }

        if (normalizedMix.mode == SingerSourceMode::DynamicMix) {
            for (const auto &source : normalizedMix.sources)
                sources.singers.push_back(encodeSingleSingerRef(singerInfo, source.speaker));
            for (const auto &keyframe : normalizedMix.dynamicKeyframes) {
                opendspx::DynamicMixingAnchor anchor;
                anchor.pos = keyframe.tick;
                anchor.ratio = encodeSourceMixingRatio(keyframe.weights);
                sources.mix.push_back(anchor);
            }
            return sources;
        }

        sources.singers.push_back(encodeSingleSingerRef(singerInfo, speakerInfo));
        return sources;
    }
}

bool DspxProjectConverter::load(const QString &path, AppModel *model, QString &errMsg,
                                ImportMode mode) {
    auto decodeCurves = [&](const std::vector<opendspx::ParamCurveRef> &dspxCurveRefs,
                            const int offset) {
        QVector<Curve *> curves;
        for (const opendspx::ParamCurveRef &dspxCurveRef : dspxCurveRefs) {
            if (dspxCurveRef->type == opendspx::ParamCurve::Type::Free) {
                const auto castCurveRef =
                    std::static_pointer_cast<opendspx::ParamCurveFree>(dspxCurveRef);
                const auto curve = new DrawCurve;
                curve->setLocalStart(castCurveRef->start - offset);
                curve->step = castCurveRef->step;
                curve->setValues(QList(castCurveRef->values.begin(), castCurveRef->values.end()));
                curves.append(curve);
            } else if (dspxCurveRef->type == opendspx::ParamCurve::Type::Anchor) {
                const auto castCurveRef =
                    std::static_pointer_cast<opendspx::ParamCurveAnchor>(dspxCurveRef);
                const auto curve = new AnchorCurve;
                curve->setLocalStart(castCurveRef->start - offset);
                for (const auto &[interp, x, y] : castCurveRef->nodes) {
                    const auto node = new AnchorNode(x, y);
                    node->setInterpMode(AnchorNode::None);
                    if (interp == opendspx::AnchorNode::Interpolation::Linear) {
                        node->setInterpMode(AnchorNode::Linear);
                    } else if (interp == opendspx::AnchorNode::Interpolation::Hermite) {
                        node->setInterpMode(AnchorNode::Hermite);
                    } /*else if (dspxNode.interp == opendspx::AnchorPoint::Interpolation::Cubic) {
                        node->setInterpMode(DsAnchorNode::Cubic);
                    }*/
                    curve->insertNode(node);
                }
                curves.append(curve);
            }
        }
        return curves;
    };

    auto decodeSingingParam = [&](const opendspx::Param &dspxParam, const int offset,
                                  SingingClip *clip) {
        Param param;
        param.setCurves(Param::Original, decodeCurves(dspxParam.original, offset), clip);
        param.setCurves(Param::Edited, decodeCurves(dspxParam.edited, offset), clip);
        param.setCurves(Param::Envelope, decodeCurves(dspxParam.transform, offset), clip);
        return param;
    };

    auto decodeSingingParams = [&](const opendspx::Params &dspxParams, const int offset,
                                   SingingClip *clip) {
        ParamInfo params(clip);
        auto dspxParams_ = dspxParams;
        params.pitch = std::move(decodeSingingParam(dspxParams_["pitch"], offset, clip));
        params.expressiveness =
            std::move(decodeSingingParam(dspxParams_["expressiveness"], offset, clip));
        params.energy = std::move(decodeSingingParam(dspxParams_["energy"], offset, clip));
        params.breathiness =
            std::move(decodeSingingParam(dspxParams_["breathiness"], offset, clip));
        params.voicing = std::move(decodeSingingParam(dspxParams_["voicing"], offset, clip));
        params.tension = std::move(decodeSingingParam(dspxParams_["tension"], offset, clip));
        params.gender = std::move(decodeSingingParam(dspxParams_["gender"], offset, clip));
        params.velocity = std::move(decodeSingingParam(dspxParams_["velocity"], offset, clip));
        return params;
    };

    // auto decodePhonemes = [&](const QList<opendspx::Phoneme> &dspxPhonemes) {
    //     QList<Phoneme> phonemes;
    //     for (const opendspx::Phoneme &dspxPhoneme : dspxPhonemes) {
    //         Phoneme phoneme(Phoneme::PhonemeType::Normal, dspxPhoneme.token, dspxPhoneme.start);
    //         if (dspxPhoneme.type == opendspx::Phoneme::Type::Ahead) {
    //             phoneme.type = Phoneme::PhonemeType::Ahead;
    //         } else if (dspxPhoneme.type == opendspx::Phoneme::Type::Final) {
    //             phoneme.type = Phoneme::PhonemeType::Final;
    //         }
    //         phonemes.append(phoneme);
    //     }
    //     return phonemes;
    // };
    auto decodeNotes = [&](const std::vector<opendspx::Note> &dspxNotes, const int offset) {
        QList<Note *> notes;
        for (const opendspx::Note &dspxNote : dspxNotes) {
            const auto note = new Note;
            note->setLocalStart(dspxNote.pos - offset);
            note->setLength(dspxNote.length);
            note->setKeyIndex(dspxNote.keyNum);
            note->setCentShift(dspxNote.centShift);
            note->setLyric(QString::fromStdString(dspxNote.lyric));
            note->setLanguage(QString::fromStdString(dspxNote.language));
            note->setPronunciation(
                Pronunciation(QString::fromStdString(dspxNote.pronunciation.original),
                              QString::fromStdString(dspxNote.pronunciation.edited)));
            QMap<QString, QJsonObject> workspace;
            for (const auto &[key, value] : dspxNote.workspace) {
                workspace[QString::fromStdString(key)] =
                    JsonNlohmann::toQJsonValue(value).toObject();
            }
            note->setWorkspace(workspace);
            // note->setPhonemeInfo(Note::Original, decodePhonemes(dspxNote.phonemes.org));
            // note->setPhonemeInfo(Note::Edited, decodePhonemes(dspxNote.phonemes.edited));
            notes.append(note);
        }
        return notes;
    };
    auto decodeClips = [&](const std::vector<opendspx::ClipRef> &dspxClips, Track *track) {
        for (const auto &dspxClip : dspxClips) {
            if (dspxClip->type == opendspx::Clip::Type::Singing) {
                const auto castClip = std::static_pointer_cast<opendspx::SingingClip>(dspxClip);
                const auto clip = new SingingClip;
                clip->setName(QString::fromStdString(castClip->name));
                auto start = castClip->time.pos - castClip->time.clipStart;
                clip->setStart(start);
                clip->setClipStart(castClip->time.clipStart);
                clip->setLength(castClip->time.length);
                clip->setClipLen(castClip->time.clipLen);
                clip->setGain(castClip->control.gain);
                clip->setMute(castClip->control.mute);
                auto notes = decodeNotes(castClip->notes, start);
                for (const auto &note : notes)
                    clip->insertNote(note);
                clip->params = std::move(decodeSingingParams(castClip->params, start, clip));
                QMap<QString, QJsonObject> workspace;
                for (const auto &[key, value] : castClip->workspace) {
                    workspace[QString::fromStdString(key)] =
                        JsonNlohmann::toQJsonValue(value).toObject();
                }
                clip->workspace() = workspace;

                // Inject track voice context for inheritance
                clip->setTrackVoiceContext(track->singerInfo(), track->speakerInfo(),
                                           track->speakerMixData());

                // Read official sources for clip singer/speaker/mix
                DecodedSingerSource officialSource;
                auto clipDsWs = dsWorkspaceFrom(castClip->workspace);
                if (castClip->sources.has_value()) {
                    officialSource = decodeDspxSingerSources(
                        *castClip->sources, clipDsWs["sourceMixState"].toObject());
                }

                // Read DS workspace for clip flags/language

                // Determine useTrackSingerInfo
                bool useTrackSingerInfo = true;
                if (clipDsWs.contains("useTrackSingerInfo")) {
                    useTrackSingerInfo = clipDsWs["useTrackSingerInfo"].toBool();
                } else if (officialSource.hasSinger) {
                    // Third-party file with official singer but no DS workspace flag
                    useTrackSingerInfo = false;
                }

                if (useTrackSingerInfo) {
                    clip->useTrackSingerAndSpeaker();
                } else if (officialSource.hasSinger) {
                    clip->setOwnVoiceContext(officialSource.singerInfo, officialSource.speakerInfo,
                                             officialSource.speakerMixData);
                } else {
                    qWarning() << "DspxProjectConverter: clip requests own singer info but has no "
                                  "supported DiffSinger sources:"
                               << QString::fromStdString(castClip->name);
                    clip->setOwnVoiceContext({}, {}, {});
                }

                // Read clip defaultLanguage
                auto clipLang = clipDsWs["defaultLanguage"].toString();
                if (!clipLang.isEmpty())
                    clip->setDefaultLanguage(clipLang);

                track->insertClip(clip);
            } else if (dspxClip->type == opendspx::Clip::Type::Audio) {
                const auto castClip = std::static_pointer_cast<opendspx::AudioClip>(dspxClip);
                const auto clip = new AudioClip;
                clip->setName(QString::fromStdString(castClip->name));
                auto start = castClip->time.pos - castClip->time.clipStart;
                clip->setStart(start);
                clip->setClipStart(castClip->time.clipStart);
                clip->setLength(castClip->time.length);
                clip->setClipLen(castClip->time.clipLen);
                clip->setGain(castClip->control.gain);
                clip->setMute(castClip->control.mute);
                clip->setPath(QString::fromStdString(castClip->path));
                QMap<QString, QJsonObject> workspace;
                for (const auto &[key, value] : castClip->workspace) {
                    workspace[QString::fromStdString(key)] =
                        JsonNlohmann::toQJsonValue(value).toObject();
                }
                clip->workspace() = workspace;
                track->insertClip(clip);
            }
        }
    };

    auto decodeTracks = [&](const std::vector<opendspx::Track> &dspxTracks, AppModel *model_) {
        int i = 0;
        for (const auto &dspxTrack : dspxTracks) {
            const auto track = new Track;
            auto trackControl = TrackControl();
            trackControl.setGain(dspxTrack.control.gain);
            trackControl.setPan(dspxTrack.control.pan);
            trackControl.setMute(dspxTrack.control.mute);
            trackControl.setSolo(dspxTrack.control.solo);
            track->setName(QString::fromStdString(dspxTrack.name));
            track->setControl(trackControl);

            // Read track singer/speaker/language from DS workspace
            auto trackDsWs = dsWorkspaceFrom(dspxTrack.workspace);
            if (!trackDsWs.isEmpty()) {
                auto singerObj = trackDsWs["singer"].toObject();
                auto singerInfo = decodeSingerInfoFromWorkspace(singerObj);
                auto speakerObj = trackDsWs["speaker"].toObject();
                auto speakerInfo =
                    resolveSpeakerInfo(singerInfo, decodeSpeakerInfoFromWorkspace(speakerObj));
                if (!singerInfo.isEmpty() || !speakerInfo.isEmpty())
                    track->setSingerAndSpeakerInfo(singerInfo, speakerInfo);

                auto defaultLang = trackDsWs["defaultLanguage"].toString();
                if (!defaultLang.isEmpty())
                    track->setDefaultLanguage(defaultLang);

                if (trackDsWs.contains("colorIndex"))
                    track->setColorIndex(trackDsWs["colorIndex"].toInt());

                track->setSpeakerMixData(decodeSpeakerMixDataFromWorkspace(
                    trackDsWs["speakerMix"].toObject(), track->singerInfo()));
            }

            decodeClips(dspxTrack.clips, track);
            model_->insertTrack(track, i);
            i++;
        }
    };

    struct LoadResult {
        enum Type { Success, Failure } type;

        opendspx::SerializationErrorList errors;
    };

    auto loadModel = [](const QString &filePath, opendspx::Model &outModel) -> LoadResult {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            return LoadResult{LoadResult::Failure, {}};

        const QByteArray data = file.readAll();
        opendspx::SerializationErrorList errors;
        std::stringstream ss(data.toStdString(), std::ios::in);
        outModel = opendspx::Serializer::deserialize(ss, errors, opendspx::Serializer::CheckError);

        LoadResult result;
        if (errors.containsFatal()) {
            result.type = LoadResult::Failure;
        } else {
            result.type = LoadResult::Success;
        }
        result.errors = errors;
        return result;
    };

    opendspx::Model dspxModel;
    auto [type, errors] = loadModel(path, dspxModel);
    if (type == LoadResult::Success) {
        // dspxModel.content.global.centShift
        // TODO: where should I use centShift in the editor?
        const auto timeline = dspxModel.content.timeline;
        model->setTimeSignature(TimeSignature(timeline.timeSignatures[0].numerator,
                                              timeline.timeSignatures[0].denominator));
        model->setTempo(timeline.tempos[0].value);
        auto masterControl = TrackControl();
        masterControl.setGain(dspxModel.content.master.control.gain);
        masterControl.setPan(dspxModel.content.master.control.pan);
        masterControl.setMute(dspxModel.content.master.control.mute);
        model->setMasterControl(masterControl);
        decodeTracks(dspxModel.content.tracks, model);

        // Load loop settings from workspace
        auto workspace = dspxModel.content.workspace;
        if (workspace.contains("loop")) {
            LoopSettings loopSettings;
            loopSettings.deserialize(JsonNlohmann::toQJsonValue(workspace["loop"]).toObject());
            appStatus->loopSettings.set(loopSettings);
        } else {
            appStatus->loopSettings.set(LoopSettings());
        }

        return true;
    }

    QString errorDetails;
    for (const auto &err : errors)
        errorDetails += QString("Error type: %1\n").arg(err->type());

    errMsg = QString("Failed to load project file.\r\npath: %1\r\nerrors: %2")
                 .arg(path)
                 .arg(errorDetails);
    return false;
}

bool DspxProjectConverter::save(const QString &path, AppModel *model, QString &errMsg) {

    auto encodeCurves = [&](const QList<Curve *> &dsCurves,
                            std::vector<opendspx::ParamCurveRef> &curves) {
        for (const auto &dsCurve : dsCurves) {
            if (dsCurve->type() == Curve::CurveType::Draw) {
                const auto castCurve = dynamic_cast<DrawCurve *>(dsCurve);
                const auto curve = std::make_shared<opendspx::ParamCurveFree>();
                curve->start = castCurve->globalStart();
                curve->step = castCurve->step;
                for (const auto v : castCurve->values()) {
                    curve->values.push_back(v);
                }
                curves.push_back(curve);
            } else if (dsCurve->type() == Curve::CurveType::Anchor) {
                const auto castCurve = dynamic_cast<AnchorCurve *>(dsCurve);
                const auto curve = std::make_shared<opendspx::ParamCurveAnchor>();
                curve->start = dsCurve->globalStart();
                for (const auto dsNode : castCurve->nodes()) {
                    opendspx::AnchorNode node;
                    node.x = dsNode->pos();
                    node.y = dsNode->value();
                    if (dsNode->interpMode() == AnchorNode::None) {
                        node.interp = opendspx::AnchorNode::Interpolation::None;
                    } else if (dsNode->interpMode() == AnchorNode::Linear) {
                        node.interp = opendspx::AnchorNode::Interpolation::Linear;
                    } else if (dsNode->interpMode() == AnchorNode::Hermite) {
                        node.interp = opendspx::AnchorNode::Interpolation::Hermite;
                    } /*else if (dsNode->interpMode() == DsAnchorNode::Cubic) {
                        node.interp = opendspx::AnchorNode::Interpolation::Cubic;
                    }*/
                    curve->nodes.push_back(node);
                }
                curves.push_back(curve);
            }
        }
    };

    auto encodeSingingParam = [&](const Param &dsParam, opendspx::Param &param) {
        encodeCurves(dsParam.curves(Param::Original), param.original);
        encodeCurves(dsParam.curves(Param::Edited), param.edited);
        encodeCurves(dsParam.curves(Param::Envelope), param.transform);
    };

    auto encodeSingingParams = [&](const ParamInfo &dsParams, opendspx::Params &params) {
        encodeSingingParam(dsParams.pitch, params["pitch"]);
        encodeSingingParam(dsParams.expressiveness, params["expressiveness"]);
        encodeSingingParam(dsParams.energy, params["energy"]);
        encodeSingingParam(dsParams.breathiness, params["breathiness"]);
        encodeSingingParam(dsParams.voicing, params["voicing"]);
        encodeSingingParam(dsParams.tension, params["tension"]);
        encodeSingingParam(dsParams.gender, params["gender"]);
        encodeSingingParam(dsParams.velocity, params["velocity"]);
    };

    // auto encodePhonemes = [&](const QList<Phoneme> &dsPhonemes, QList<opendspx::Phoneme>
    // &phonemes)
    // {
    //     for (const auto &dsPhoneme : dsPhonemes) {
    //         opendspx::Phoneme phoneme;
    //         phoneme.start = dsPhoneme.start;
    //         phoneme.token = dsPhoneme.name;
    //         if (dsPhoneme.type == Phoneme::PhonemeType::Ahead) {
    //             phoneme.type = opendspx::Phoneme::Type::Ahead;
    //         } else if (dsPhoneme.type == Phoneme::PhonemeType::Final) {
    //             phoneme.type = opendspx::Phoneme::Type::Final;
    //         } else {
    //             phoneme.type = opendspx::Phoneme::Type::Normal;
    //         }
    //         phonemes.append(phoneme);
    //     }
    // };

    auto encodeNotes = [&](const OverlappableSerialList<Note> &dsNotes,
                           std::vector<opendspx::Note> &notes) {
        for (const auto dsNote : dsNotes) {
            opendspx::Note note;
            note.pos = dsNote->globalStart();
            note.length = dsNote->length();
            note.keyNum = dsNote->keyIndex();
            note.centShift = dsNote->centShift();
            note.lyric = dsNote->lyric().toStdString();
            note.language = dsNote->language().toStdString();
            note.pronunciation.original = dsNote->pronunciation().original.toStdString();
            note.pronunciation.edited = dsNote->pronunciation().edited.toStdString();
            // encodePhonemes(dsNote->phonemeInfo().original, note.phonemes.org);
            // encodePhonemes(dsNote->phonemeInfo().edited, note.phonemes.edited);
            for (const auto &[key, value] : dsNote->workspace().asKeyValueRange()) {
                note.workspace[key.toStdString()] = JsonNlohmann::fromQJsonValue(value);
            }
            notes.push_back(note);
        }
    };

    auto encodeClips = [&](const Track *dsTrack, opendspx::Track &track) {
        for (const auto clip : dsTrack->clips()) {
            if (clip->clipType() == Clip::Singing) {
                const auto singingClip = dynamic_cast<SingingClip *>(clip);
                auto singClip = std::make_shared<opendspx::SingingClip>();
                singClip->name = clip->name().toStdString();
                singClip->time.pos = clip->start() + clip->clipStart();
                singClip->time.clipStart = clip->clipStart();
                singClip->time.length = clip->length();
                singClip->time.clipLen = clip->clipLen();
                singClip->control.gain = clip->gain();
                singClip->control.mute = clip->mute();

                // Preserve existing workspace keys
                for (const auto &[key, value] : clip->workspace().asKeyValueRange()) {
                    singClip->workspace[key.toStdString()] = JsonNlohmann::fromQJsonValue(value);
                }

                // Write clip DS workspace (flags/speaker/language)
                QJsonObject clipDsWs;
                clipDsWs["schemaVersion"] = kDsWorkspaceSchemaVersion;
                const bool useTrackInfo = singingClip->useTrackSingerInfo.get();
                clipDsWs["useTrackSingerInfo"] = useTrackInfo;

                auto clipLang = singingClip->defaultLanguage();
                if (!clipLang.isEmpty() && clipLang != "unknown")
                    clipDsWs["defaultLanguage"] = clipLang;

                auto clipG2p = singingClip->defaultG2pId();
                if (!clipG2p.isEmpty() && clipG2p != "unknown")
                    clipDsWs["defaultG2pId"] = clipG2p;

                if (!useTrackInfo) {
                    auto sourceMixState =
                        encodeSourceMixStateForWorkspace(singingClip->ownSpeakerMixData());
                    if (!sourceMixState.isEmpty())
                        clipDsWs["sourceMixState"] = sourceMixState;
                }

                writeDsWorkspace(singClip->workspace, clipDsWs);

                // Write official sources
                const auto effectiveSinger = singingClip->singerInfo();
                auto sources = encodeDspxSingerSources(effectiveSinger, singingClip->speakerInfo(),
                                                       singingClip->speakerMixData());
                if (sources.has_value())
                    singClip->sources = sources;

                encodeNotes(singingClip->notes(), singClip->notes);
                encodeSingingParams(singingClip->params, singClip->params);
                track.clips.push_back(singClip);
            } else if (clip->clipType() == Clip::Audio) {
                const auto audioClip = dynamic_cast<AudioClip *>(clip);
                auto audioClipRef = std::make_shared<opendspx::AudioClip>();
                audioClipRef->name = clip->name().toStdString();
                audioClipRef->time.pos = clip->start() + clip->clipStart();
                audioClipRef->time.clipStart = clip->clipStart();
                audioClipRef->time.length = clip->length();
                audioClipRef->time.clipLen = clip->clipLen();
                audioClipRef->control.gain = clip->gain();
                audioClipRef->control.mute = clip->mute();
                audioClipRef->path = audioClip->path().toStdString();
                for (const auto &[key, value] : clip->workspace().asKeyValueRange()) {
                    audioClipRef->workspace[key.toStdString()] =
                        JsonNlohmann::fromQJsonValue(value);
                }
                track.clips.push_back(audioClipRef);
            }
        }
    };

    auto encodeTracks = [&](const AppModel *model_, opendspx::Model &dspx) {
        for (const auto dsTrack : model_->tracks()) {
            opendspx::Track track;
            track.name = dsTrack->name().toStdString();
            track.control.gain = dsTrack->control().gain();
            track.control.pan = dsTrack->control().pan();
            track.control.mute = dsTrack->control().mute();
            track.control.solo = dsTrack->control().solo();

            // Write track DS workspace (singer/speaker/language)
            QJsonObject trackDsWs;
            trackDsWs["schemaVersion"] = kDsWorkspaceSchemaVersion;

            auto singerObj = encodeSingerInfoForWorkspace(dsTrack->singerInfo(), true);
            if (!singerObj.isEmpty())
                trackDsWs["singer"] = singerObj;

            auto speakerObj = encodeSpeakerInfoForWorkspace(dsTrack->speakerInfo());
            if (!speakerObj.isEmpty())
                trackDsWs["speaker"] = speakerObj;

            auto trackLang = dsTrack->defaultLanguage();
            if (!trackLang.isEmpty() && trackLang != "unknown")
                trackDsWs["defaultLanguage"] = trackLang;

            auto trackG2p = dsTrack->defaultG2pId();
            if (!trackG2p.isEmpty() && trackG2p != "unknown")
                trackDsWs["defaultG2pId"] = trackG2p;

            trackDsWs["colorIndex"] = dsTrack->colorIndex();

            auto trackSpeakerMixObj = encodeSpeakerMixDataForWorkspace(dsTrack->speakerMixData());
            if (!trackSpeakerMixObj.isEmpty())
                trackDsWs["speakerMix"] = trackSpeakerMixObj;

            writeDsWorkspace(track.workspace, trackDsWs);

            encodeClips(dsTrack, track);
            dspx.content.tracks.push_back(track);
        }
    };

    opendspx::Model dspxModel;
    dspxModel.content.global.centShift = 0; // TODO: where should I use centShift in the editor?
    const auto masterControl = model->masterControl();
    dspxModel.content.master.control.gain = masterControl.gain();
    dspxModel.content.master.control.pan = masterControl.pan();
    dspxModel.content.master.control.mute = masterControl.mute();
    auto &timeline = dspxModel.content.timeline;
    timeline.tempos.push_back(opendspx::Tempo(0, model->tempo()));
    timeline.timeSignatures.push_back(opendspx::TimeSignature(0, model->timeSignature().numerator,
                                                              model->timeSignature().denominator));

    encodeTracks(model, dspxModel);

    const auto loopSettings = appStatus->loopSettings.get();
    dspxModel.content.workspace["loop"] = JsonNlohmann::fromQJsonValue(loopSettings.serialize());

    auto saveModelToFile = [](const opendspx::Model &model_, const QString &filePath,
                              QString &msg) -> bool {
        opendspx::SerializationErrorList errors;
        std::stringstream ss(std::ios::out);
        opendspx::Serializer::serialize(
            ss, model_, errors, opendspx::Serializer::CheckError);

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            msg += "Failed to open file for writing:" + filePath;
            return false;
        }

        auto jsonData = QByteArray::fromStdString(ss.str());

        const qint64 written = file.write(jsonData);
        file.close();

        if (written != jsonData.size()) {
            msg += "Failed to write all data to file:" + filePath;
            return false;
        }

        if (!errors.empty()) {
            QTextStream stream(&msg, QIODeviceBase::WriteOnly | QIODeviceBase::Text);
            stream << "Serialization errors occurred:\n";
            for (const auto &err : errors) {
                stream << "0x" << Qt::hex << err->type();
                QString jsonPath;
                if (err->type() == opendspx::SerializationError::RangeConstraintViolation) {
                    jsonPath = QString::fromStdString(std::static_pointer_cast<opendspx::RangeConstraintViolationError>(err)->path());
                } else if (err->type() == opendspx::SerializationError::InvalidRatioPartition) {
                    jsonPath = QString::fromStdString(std::static_pointer_cast<opendspx::InvalidRatioPartitionError>(err)->path());
                } else if (err->type() == opendspx::SerializationError::PartCountNotMatch) {
                    jsonPath = QString::fromStdString(std::static_pointer_cast<opendspx::PartCountNotMatchError>(err)->path());
                } else if (err->type() == opendspx::SerializationError::EmptySingerMixing) {
                    jsonPath = QString::fromStdString(std::static_pointer_cast<opendspx::EmptySingerMixingError>(err)->path());
                }
                if (!jsonPath.isEmpty()) {
                    stream << " at " << jsonPath;
                }
                stream << "\n";
            }
            return false;
        }

        return true;
    };

    QString errorMsg;
    const auto returnCode = saveModelToFile(dspxModel, path, errorMsg);

    if (!returnCode) {
        errMsg = errorMsg;
        return false;
    }
    return true;
}
