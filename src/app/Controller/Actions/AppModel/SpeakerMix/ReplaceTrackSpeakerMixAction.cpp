#include "ReplaceTrackSpeakerMixAction.h"

#include "Model/AppModel/Track.h"

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

ReplaceTrackSpeakerMixAction::ReplaceTrackSpeakerMixAction(const SpeakerMixData &data, Track *track)
    : m_oldData(track ? track->speakerMixData() : SpeakerMixData()),
      m_newData(preservePresetSourceAsDirty(m_oldData, normalizeSpeakerMixData(data))),
      m_track(track) {
}

void ReplaceTrackSpeakerMixAction::execute() {
    if (m_track)
        m_track->setSpeakerMixData(m_newData);
}

void ReplaceTrackSpeakerMixAction::undo() {
    if (m_track)
        m_track->setSpeakerMixData(m_oldData);
}
