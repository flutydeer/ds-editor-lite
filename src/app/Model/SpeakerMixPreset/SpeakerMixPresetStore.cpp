#include "SpeakerMixPresetStore.h"

#include "Global/AppOptionsGlobal.h"
#include "Model/AppOptions/AppOptions.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

#include <algorithm>

namespace {

    constexpr int kSchemaVersion = 1;

    bool matchesSinger(const SpeakerMixPreset &preset, const SingerIdentifier &identifier) {
        return preset.singerId == identifier.singerId && preset.packageId == identifier.packageId &&
               preset.packageVersion == identifier.packageVersion;
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
