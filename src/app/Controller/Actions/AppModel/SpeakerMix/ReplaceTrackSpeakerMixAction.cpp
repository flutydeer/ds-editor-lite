#include "ReplaceTrackSpeakerMixAction.h"

#include "Model/AppModel/Track.h"

using namespace SpeakerMixModel;

ReplaceTrackSpeakerMixAction::ReplaceTrackSpeakerMixAction(const SpeakerMixData &data, Track *track)
    : m_oldData(track ? track->speakerMixData() : SpeakerMixData()),
      m_newData(normalizeSpeakerMixData(data)), m_track(track) {
}

void ReplaceTrackSpeakerMixAction::execute() {
    if (m_track)
        m_track->setSpeakerMixData(m_newData);
}

void ReplaceTrackSpeakerMixAction::undo() {
    if (m_track)
        m_track->setSpeakerMixData(m_oldData);
}
