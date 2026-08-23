#ifndef SPEAKERMIXDISPLAYUTILS_H
#define SPEAKERMIXDISPLAYUTILS_H

#include <lite/ProjectModel/AppModel/SpeakerMixData.h>
#include "Model/SpeakerMixPreset/SpeakerMixPresetStore.h"
#include <lite/ProjectModel/Voice/SingerInfo.h>
#include "Utils/UiLanguageManager.h"

#include <QCoreApplication>
#include <QString>

namespace SpeakerMixDisplayUtils {

    inline QString translate(const char *sourceText) {
        return QCoreApplication::translate("SpeakerMixDisplay", sourceText);
    }

    /// Current UI language as a BCP 47 tag (e.g. "zh-CN"); empty when the
    /// language manager is not yet constructed.
    inline QStringList currentBcp47Candidates() {
        return UiLanguageManager::currentBcp47Candidates();
    }

    inline QString mixDisplayName(const SingerInfo &singerInfo,
                                  const SpeakerMixModel::SpeakerMixData &data) {
        const auto normalized = SpeakerMixModel::normalizeSpeakerMixData(data);
        if (SpeakerMixModel::isDynamicMixActive(normalized)) {
            return translate("Dynamic Mix");
        }
        if (SpeakerMixModel::isDynamicMixBypassed(normalized)) {
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
        return speakerInfo.displayName(currentBcp47Candidates());
    }

    inline QString comboDisplayText(const SingerInfo &singerInfo,
                                    const SpeakerMixModel::SpeakerMixData &data) {
        const auto mixName = mixDisplayName(singerInfo, data);
        if (mixName.isEmpty())
            return {};
        if (singerInfo.name().isEmpty())
            return mixName;
        return singerInfo.displayName(currentBcp47Candidates()) + " / " + mixName;
    }

} // namespace SpeakerMixDisplayUtils

#endif // SPEAKERMIXDISPLAYUTILS_H
