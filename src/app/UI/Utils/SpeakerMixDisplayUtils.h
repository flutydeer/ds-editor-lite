#ifndef SPEAKERMIXDISPLAYUTILS_H
#define SPEAKERMIXDISPLAYUTILS_H

#include "Model/AppModel/SpeakerMixData.h"
#include "Model/SpeakerMixPreset/SpeakerMixPresetStore.h"
#include "Modules/PackageManager/Models/SingerInfo.h"

#include <QCoreApplication>
#include <QString>

namespace SpeakerMixDisplayUtils {

    inline QString translate(const char *sourceText) {
        return QCoreApplication::translate("SpeakerMixDisplay", sourceText);
    }

    inline QString mixDisplayName(const SingerInfo &singerInfo,
                                  const SpeakerMixModel::SpeakerMixData &data) {
        const auto normalized = SpeakerMixModel::normalizeSpeakerMixData(data);
        if (normalized.mode == SpeakerMixModel::SingerSourceMode::DynamicMix &&
            !normalized.dynamicKeyframes.isEmpty()) {
            return translate("Dynamic Mix");
        }
        if (normalized.mode == SpeakerMixModel::SingerSourceMode::FixedMix &&
            !normalized.dynamicKeyframes.isEmpty()) {
            return translate("Dynamic Mix (Bypassed)");
        }
        if (normalized.mode != SpeakerMixModel::SingerSourceMode::FixedMix ||
            normalized.sources.size() < 2) {
            return {};
        }

        QString presetName = normalized.sourcePresetName;
        if (const auto preset = SpeakerMixPresetStore::sourcePresetForData(singerInfo, normalized))
            presetName = preset->name;
        if (!presetName.isEmpty())
            return presetName + (normalized.sourcePresetDirty ? "*" : "");
        return translate("Custom Mix");
    }

    inline QString speakerDisplayName(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                      const SpeakerMixModel::SpeakerMixData &data) {
        if (const auto mixName = mixDisplayName(singerInfo, data); !mixName.isEmpty())
            return mixName;
        return speakerInfo.name();
    }

    inline QString comboDisplayText(const SingerInfo &singerInfo,
                                    const SpeakerMixModel::SpeakerMixData &data) {
        const auto mixName = mixDisplayName(singerInfo, data);
        if (mixName.isEmpty())
            return {};
        if (singerInfo.name().isEmpty())
            return mixName;
        return singerInfo.name() + " / " + mixName;
    }

} // namespace SpeakerMixDisplayUtils

#endif // SPEAKERMIXDISPLAYUTILS_H
