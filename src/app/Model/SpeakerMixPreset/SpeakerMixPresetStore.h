#ifndef SPEAKERMIXPRESETSTORE_H
#define SPEAKERMIXPRESETSTORE_H

#include "SpeakerMixPreset.h"
#include "Modules/PackageManager/Models/SingerInfo.h"

#include <optional>

class SpeakerMixPresetStore {
public:
    static QList<SpeakerMixPreset> allPresets();
    static QList<SpeakerMixPreset> presetsForSinger(const SingerInfo &singerInfo);
    static QList<SpeakerMixPreset> presetsForSinger(const SingerIdentifier &identifier);
    static std::optional<SpeakerMixPreset> findPreset(const QString &id);
    static std::optional<SpeakerMixPreset> findPresetByName(const SingerInfo &singerInfo,
                                                            const QString &name);
    static SpeakerMixModel::SpeakerMixData speakerMixDataFromPreset(
        const SpeakerMixPreset &preset, const SingerInfo &singerInfo);
    static bool speakerMixDataMatchesPreset(const SpeakerMixPreset &preset,
                                            const SingerInfo &singerInfo,
                                            const SpeakerMixModel::SpeakerMixData &data);
    static std::optional<SpeakerMixPreset> sourcePresetForData(
        const SingerInfo &singerInfo, const SpeakerMixModel::SpeakerMixData &data);
    static bool savePreset(SpeakerMixPreset preset);
    static bool deletePreset(const QString &id);
    static bool presetNameExists(const SingerInfo &singerInfo, const QString &name,
                                 const QString &excludingId = {});
};

#endif // SPEAKERMIXPRESETSTORE_H
