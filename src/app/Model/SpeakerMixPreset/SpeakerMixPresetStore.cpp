#include "SpeakerMixPresetStore.h"

#include "Global/AppOptionsGlobal.h"
#include "Model/AppOptions/AppOptions.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace {

    constexpr int kSchemaVersion = 1;
    constexpr double kWeightEpsilon = 1e-6;

    using namespace SpeakerMixModel;

    bool matchesSinger(const SpeakerMixPreset &preset, const SingerIdentifier &identifier) {
        return preset.singerId == identifier.singerId && preset.packageId == identifier.packageId &&
               preset.packageVersion == identifier.packageVersion;
    }

    SpeakerInfo speakerById(const SingerInfo &singerInfo, const QString &id) {
        for (const auto &speaker : singerInfo.speakers()) {
            if (speaker.id() == id)
                return speaker;
        }
        return {};
    }

    bool hasSameFixedMixContent(const SpeakerMixData &lhs, const SpeakerMixData &rhs) {
        const auto left = normalizeSpeakerMixData(lhs);
        const auto right = normalizeSpeakerMixData(rhs);
        if (left.mode != SingerSourceMode::FixedMix || right.mode != SingerSourceMode::FixedMix)
            return false;
        if (!left.dynamicKeyframes.isEmpty() || !right.dynamicKeyframes.isEmpty())
            return false;
        if (left.sources.size() != right.sources.size() ||
            left.fixedWeights.size() != right.fixedWeights.size()) {
            return false;
        }

        for (int i = 0; i < left.sources.size(); ++i) {
            if (left.sources.at(i).speaker.id() != right.sources.at(i).speaker.id())
                return false;
        }
        for (int i = 0; i < left.fixedWeights.size(); ++i) {
            if (std::abs(left.fixedWeights.at(i) - right.fixedWeights.at(i)) > kWeightEpsilon)
                return false;
        }
        return true;
    }

    QJsonObject rootObject() {
        return appOptions->general()->speakerMixPresets.toObject();
    }

    bool writePresets(const QList<SpeakerMixPreset> &presets) {
        QJsonArray presetArray;
        for (const auto &preset : presets)
            presetArray.append(preset.toJson());

        QJsonObject root;
        root["schemaVersion"] = kSchemaVersion;
        root["presets"] = presetArray;
        appOptions->general()->speakerMixPresets = root;
        return appOptions->saveAndNotify(AppOptionsGlobal::Option::General);
    }

} // namespace

QList<SpeakerMixPreset> SpeakerMixPresetStore::allPresets() {
    const auto root = rootObject();
    if (root["schemaVersion"].toInt(kSchemaVersion) != kSchemaVersion)
        return {};

    QList<SpeakerMixPreset> presets;
    const auto presetArray = root["presets"].toArray();
    for (const auto &value : presetArray) {
        auto preset = SpeakerMixPreset::fromJson(value.toObject());
        if (!preset.id.isEmpty() && !preset.name.isEmpty() && !preset.singerId.isEmpty() &&
            !preset.packageId.isEmpty())
            presets.append(std::move(preset));
    }
    return presets;
}

QList<SpeakerMixPreset> SpeakerMixPresetStore::presetsForSinger(const SingerInfo &singerInfo) {
    return presetsForSinger(singerInfo.identifier());
}

QList<SpeakerMixPreset>
    SpeakerMixPresetStore::presetsForSinger(const SingerIdentifier &identifier) {
    QList<SpeakerMixPreset> result;
    for (const auto &preset : allPresets()) {
        if (matchesSinger(preset, identifier))
            result.append(preset);
    }
    return result;
}

std::optional<SpeakerMixPreset> SpeakerMixPresetStore::findPreset(const QString &id) {
    for (const auto &preset : allPresets()) {
        if (preset.id == id)
            return preset;
    }
    return std::nullopt;
}

std::optional<SpeakerMixPreset>
    SpeakerMixPresetStore::findPresetByName(const SingerInfo &singerInfo, const QString &name) {
    for (const auto &preset : presetsForSinger(singerInfo)) {
        if (preset.name == name)
            return preset;
    }
    return std::nullopt;
}

SpeakerMixData SpeakerMixPresetStore::speakerMixDataFromPreset(const SpeakerMixPreset &preset,
                                                               const SingerInfo &singerInfo) {
    SpeakerMixData data;
    data.mode = SingerSourceMode::FixedMix;
    for (const auto &source : preset.sources) {
        const auto speaker = speakerById(singerInfo, source.speaker.id());
        if (!speaker.isEmpty())
            data.sources.append({speaker});
    }
    data.fixedWeights = preset.fixedWeights;
    data.sourcePresetId = preset.id;
    data.sourcePresetName = preset.name;
    data.sourcePresetDirty = false;
    return normalizeSpeakerMixData(data);
}

bool SpeakerMixPresetStore::speakerMixDataMatchesPreset(const SpeakerMixPreset &preset,
                                                        const SingerInfo &singerInfo,
                                                        const SpeakerMixData &data) {
    return hasSameFixedMixContent(speakerMixDataFromPreset(preset, singerInfo), data);
}

std::optional<SpeakerMixPreset>
    SpeakerMixPresetStore::sourcePresetForData(const SingerInfo &singerInfo,
                                               const SpeakerMixData &data) {
    if (data.sourcePresetId.isEmpty())
        return std::nullopt;
    const auto preset = findPreset(data.sourcePresetId);
    if (!preset || !matchesSinger(*preset, singerInfo.identifier()))
        return std::nullopt;
    return preset;
}

bool SpeakerMixPresetStore::savePreset(SpeakerMixPreset preset) {
    const auto now = QDateTime::currentDateTimeUtc();
    if (preset.id.isEmpty())
        preset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!preset.createdAt.isValid())
        preset.createdAt = now;
    preset.updatedAt = now;

    if (preset.name.trimmed().isEmpty() || preset.singerId.isEmpty() || preset.packageId.isEmpty())
        return false;

    auto presets = allPresets();
    for (const auto &existing : std::as_const(presets)) {
        if (existing.id != preset.id && matchesSinger(existing, preset.singerIdentifier()) &&
            existing.name == preset.name)
            return false;
    }

    for (auto &existing : presets) {
        if (existing.id == preset.id) {
            if (existing.createdAt.isValid())
                preset.createdAt = existing.createdAt;
            existing = std::move(preset);
            return writePresets(presets);
        }
    }

    presets.append(std::move(preset));
    return writePresets(presets);
}

bool SpeakerMixPresetStore::deletePreset(const QString &id) {
    auto presets = allPresets();
    const auto oldSize = presets.size();
    presets.erase(std::remove_if(presets.begin(), presets.end(),
                                 [&id](const SpeakerMixPreset &preset) { return preset.id == id; }),
                  presets.end());
    if (presets.size() == oldSize)
        return false;
    return writePresets(presets);
}

bool SpeakerMixPresetStore::presetNameExists(const SingerInfo &singerInfo, const QString &name,
                                             const QString &excludingId) {
    for (const auto &preset : presetsForSinger(singerInfo)) {
        if (preset.id != excludingId && preset.name == name)
            return true;
    }
    return false;
}
