#ifndef APPLYTRACKSPEAKERMIXPRESETACTION_H
#define APPLYTRACKSPEAKERMIXPRESETACTION_H

#include "Model/AppModel/SpeakerMixData.h"
#include "Modules/History/IAction.h"
#include "Modules/PackageManager/Models/SingerInfo.h"

class Track;
using SpeakerMixModel::SpeakerMixData;

class ApplyTrackSpeakerMixPresetAction final : public IAction {
public:
    ApplyTrackSpeakerMixPresetAction(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                     const SpeakerMixData &data, Track *track);
    void execute() override;
    void undo() override;

private:
    SingerInfo m_oldSingerInfo;
    SpeakerInfo m_oldSpeakerInfo;
    SpeakerMixData m_oldSpeakerMixData;

    SingerInfo m_newSingerInfo;
    SpeakerInfo m_newSpeakerInfo;
    SpeakerMixData m_newSpeakerMixData;
    Track *m_track = nullptr;
};

#endif // APPLYTRACKSPEAKERMIXPRESETACTION_H
