#include "SetClipVoiceContextAction.h"

#include "Model/AppModel/SingingClip.h"

using namespace SpeakerMixModel;

SetClipVoiceContextAction::SetClipVoiceContextAction(bool newUseTrackVoiceContext,
                                                     const SingerInfo &newOwnSingerInfo,
                                                     const SpeakerInfo &newOwnSpeakerInfo,
                                                     const SpeakerMixData &newOwnSpeakerMixData,
                                                     SingingClip *clip)
    : m_oldSnapshot(snapshotFromClip(clip)),
      m_newSnapshot{newUseTrackVoiceContext, newOwnSingerInfo, newOwnSpeakerInfo,
                    normalizeSpeakerMixData(newOwnSpeakerMixData)},
      m_clip(clip) {
}

void SetClipVoiceContextAction::execute() {
    applySnapshot(m_newSnapshot);
}

void SetClipVoiceContextAction::undo() {
    applySnapshot(m_oldSnapshot);
}

SetClipVoiceContextAction::Snapshot
    SetClipVoiceContextAction::snapshotFromClip(const SingingClip *clip) {
    if (!clip)
        return {};

    return {clip->useTrackSingerInfo.get(), clip->ownSingerInfo(), clip->ownSpeakerInfo(),
            clip->ownSpeakerMixData()};
}

void SetClipVoiceContextAction::applySnapshot(const Snapshot &snapshot) const {
    if (!m_clip)
        return;

    m_clip->setOwnVoiceContext(snapshot.ownSingerInfo, snapshot.ownSpeakerInfo,
                               snapshot.ownSpeakerMixData);
    if (snapshot.useTrackVoiceContext)
        m_clip->useTrackVoiceContext();
}
