#include "ApplyClipSpeakerMixPresetAction.h"

#include "Model/AppModel/SingingClip.h"

using namespace SpeakerMixModel;

ApplyClipSpeakerMixPresetAction::ApplyClipSpeakerMixPresetAction(const SingerInfo &singerInfo,
                                                                 const SpeakerInfo &speakerInfo,
                                                                 const SpeakerMixData &data,
                                                                 SingingClip *clip)
    : m_oldUseTrack(clip ? clip->useTrackSingerInfo.get() : true),
      m_oldOwnSingerInfo(clip ? clip->ownSingerInfo() : SingerInfo()),
      m_oldOwnSpeakerInfo(clip ? clip->ownSpeakerInfo() : SpeakerInfo()),
      m_oldOwnSpeakerMixData(clip ? clip->ownSpeakerMixData() : SpeakerMixData()),
      m_newSingerInfo(singerInfo), m_newSpeakerInfo(speakerInfo),
      m_newSpeakerMixData(normalizeSpeakerMixData(data)), m_clip(clip) {
}

void ApplyClipSpeakerMixPresetAction::execute() {
    if (!m_clip)
        return;

    m_clip->setOwnSingerAndSpeaker(m_newSingerInfo, m_newSpeakerInfo);
    m_clip->setOwnSpeakerMixData(m_newSpeakerMixData);
}

void ApplyClipSpeakerMixPresetAction::undo() {
    if (!m_clip)
        return;

    m_clip->setOwnSingerAndSpeaker(m_oldOwnSingerInfo, m_oldOwnSpeakerInfo);
    m_clip->setOwnSpeakerMixData(m_oldOwnSpeakerMixData);
    if (m_oldUseTrack)
        m_clip->useTrackSingerAndSpeaker();
}
