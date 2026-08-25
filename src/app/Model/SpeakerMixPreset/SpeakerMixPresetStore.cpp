#include "SpeakerMixPresetStore.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"

#include <algorithm>
#include <cmath>

namespace {

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

    Automation::SpeakerMixPresetDto toDto(const SpeakerMixPreset &preset) {
        Automation::SpeakerMixPresetDto result{
            .id = preset.id,
            .name = preset.name,
            .packageId = preset.packageId,
            .singerId = preset.singerId,
            .packageVersion = preset.packageVersion,
            .fixedWeights = preset.fixedWeights,
            .createdAt = preset.createdAt,
            .updatedAt = preset.updatedAt,
        };
        for (const auto &source : preset.sources) {
            result.sources.append({
                .speakerId = source.speaker.id(),
                .speakerName = source.speaker.name(),
            });
        }
        return result;
    }

    SpeakerMixPreset fromDto(const Automation::SpeakerMixPresetDto &preset) {
        SpeakerMixPreset result{
            .id = preset.id,
            .name = preset.name,
            .packageId = preset.packageId,
            .singerId = preset.singerId,
            .packageVersion = preset.packageVersion,
            .fixedWeights = preset.fixedWeights,
            .createdAt = preset.createdAt,
            .updatedAt = preset.updatedAt,
        };
        for (const auto &source : preset.sources)
            result.sources.append({SpeakerInfo(source.speakerId, source.speakerName)});
        return result;
    }

} // namespace

QList<SpeakerMixPreset> SpeakerMixPresetStore::allPresets() {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return {};
    const auto result = runtime->presets().getSpeakerMixPresets();
    if (!result)
        return {};
    QList<SpeakerMixPreset> presets;
    for (const auto &preset : result.get())
        presets.append(fromDto(preset));
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

std::optional<SpeakerMixPreset> SpeakerMixPresetStore::savePreset(SpeakerMixPreset preset) {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return std::nullopt;
    const auto result = runtime->presets().saveSpeakerMixPreset({}, toDto(preset));
    return result ? std::optional<SpeakerMixPreset>(fromDto(result.get())) : std::nullopt;
}

bool SpeakerMixPresetStore::deletePreset(const QString &id) {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return false;
    const auto result = runtime->presets().deleteSpeakerMixPreset({}, id);
    return result && result.get().changed;
}

bool SpeakerMixPresetStore::presetNameExists(const SingerInfo &singerInfo, const QString &name,
                                             const QString &excludingId) {
    for (const auto &preset : presetsForSinger(singerInfo)) {
        if (preset.id != excludingId && preset.name == name)
            return true;
    }
    return false;
}
