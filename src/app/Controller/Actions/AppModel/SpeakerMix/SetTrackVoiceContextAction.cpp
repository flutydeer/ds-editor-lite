#include "SetTrackVoiceContextAction.h"

#include "Model/AppModel/Track.h"

using namespace SpeakerMixModel;

SetTrackVoiceContextAction::SetTrackVoiceContextAction(const SingerInfo &newSingerInfo,
                                                       const SpeakerInfo &newSpeakerInfo,
                                                       const SpeakerMixData &newSpeakerMixData,
                                                       Track *track)
    : m_oldSnapshot(snapshotFromTrack(track)),
      m_newSnapshot{newSingerInfo, newSpeakerInfo, normalizeSpeakerMixData(newSpeakerMixData)},
      m_track(track) {
}

void SetTrackVoiceContextAction::execute() {
    applySnapshot(m_newSnapshot);
}

void SetTrackVoiceContextAction::undo() {
    applySnapshot(m_oldSnapshot);
}

SetTrackVoiceContextAction::Snapshot
    SetTrackVoiceContextAction::snapshotFromTrack(const Track *track) {
    if (!track)
        return {};

    return {track->singerInfo(), track->speakerInfo(), track->speakerMixData()};
}

void SetTrackVoiceContextAction::applySnapshot(const Snapshot &snapshot) const {
    if (!m_track)
        return;

    m_track->setVoiceContext(snapshot.singerInfo, snapshot.speakerInfo, snapshot.speakerMixData);
}
