#include "EnableClipDynamicSpeakerMixAction.h"

#include "Model/AppModel/SingingClip.h"

using namespace SpeakerMixModel;

EnableClipDynamicSpeakerMixAction::EnableClipDynamicSpeakerMixAction(const SingerInfo &singerInfo,
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

void EnableClipDynamicSpeakerMixAction::execute() {
    if (!m_clip)
        return;

    m_clip->setOwnVoiceContext(m_newSingerInfo, m_newSpeakerInfo, m_newSpeakerMixData);
}

void EnableClipDynamicSpeakerMixAction::undo() {
    if (!m_clip)
        return;

    m_clip->setOwnVoiceContext(m_oldOwnSingerInfo, m_oldOwnSpeakerInfo, m_oldOwnSpeakerMixData);
    if (m_oldUseTrack)
        m_clip->useTrackVoiceContext();
}
