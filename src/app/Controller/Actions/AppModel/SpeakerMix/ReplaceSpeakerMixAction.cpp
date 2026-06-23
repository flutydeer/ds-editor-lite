//
// Created by FlutyDeer on 2026/6/20.
//

#include "ReplaceSpeakerMixAction.h"

#include "Model/AppModel/SingingClip.h"

using namespace SpeakerMixModel;

namespace {

    SpeakerMixData preservePresetSourceAsDirty(const SpeakerMixData &oldData,
                                               SpeakerMixData newData) {
        if (!newData.sourcePresetId.isEmpty() || oldData.sourcePresetId.isEmpty())
            return newData;
        newData.sourcePresetId = oldData.sourcePresetId;
        newData.sourcePresetName = oldData.sourcePresetName;
        newData.sourcePresetDirty = true;
        return normalizeSpeakerMixData(newData);
    }

} // namespace

ReplaceSpeakerMixAction::ReplaceSpeakerMixAction(const SpeakerMixData &data, SingingClip *clip)
    : m_oldData(clip ? clip->speakerMixData() : SpeakerMixData()),
      m_newData(preservePresetSourceAsDirty(m_oldData, normalizeSpeakerMixData(data))),
      m_clip(clip) {
}

void ReplaceSpeakerMixAction::execute() {
    if (m_clip)
        m_clip->setSpeakerMixData(m_newData);
}

void ReplaceSpeakerMixAction::undo() {
    if (m_clip)
        m_clip->setSpeakerMixData(m_oldData);
}
