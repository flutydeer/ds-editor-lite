#include "ApplyTrackSpeakerMixPresetAction.h"

#include "Model/AppModel/Track.h"

using namespace SpeakerMixModel;

ApplyTrackSpeakerMixPresetAction::ApplyTrackSpeakerMixPresetAction(const SingerInfo &singerInfo,
                                                                   const SpeakerInfo &speakerInfo,
                                                                   const SpeakerMixData &data,
                                                                   Track *track)
    : m_oldSingerInfo(track ? track->singerInfo() : SingerInfo()),
      m_oldSpeakerInfo(track ? track->speakerInfo() : SpeakerInfo()),
      m_oldSpeakerMixData(track ? track->speakerMixData() : SpeakerMixData()),
      m_newSingerInfo(singerInfo), m_newSpeakerInfo(speakerInfo),
      m_newSpeakerMixData(normalizeSpeakerMixData(data)), m_track(track) {
}

void ApplyTrackSpeakerMixPresetAction::execute() {
    if (!m_track)
        return;

    m_track->setSingerAndSpeakerInfo(m_newSingerInfo, m_newSpeakerInfo);
    m_track->setSpeakerMixData(m_newSpeakerMixData);
}

void ApplyTrackSpeakerMixPresetAction::undo() {
    if (!m_track)
        return;

    m_track->setSingerAndSpeakerInfo(m_oldSingerInfo, m_oldSpeakerInfo);
    m_track->setSpeakerMixData(m_oldSpeakerMixData);
}
