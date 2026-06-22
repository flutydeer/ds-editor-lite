#ifndef ENABLECLIPDYNAMICSPEAKERMIXACTION_H
#define ENABLECLIPDYNAMICSPEAKERMIXACTION_H

#include "Model/AppModel/SpeakerMixData.h"
#include "Modules/History/IAction.h"
#include "Modules/PackageManager/Models/SingerInfo.h"

class SingingClip;
using SpeakerMixModel::SpeakerMixData;

class EnableClipDynamicSpeakerMixAction final : public IAction {
public:
    EnableClipDynamicSpeakerMixAction(const SingerInfo &singerInfo,
                                      const SpeakerInfo &speakerInfo,
                                      const SpeakerMixData &data, SingingClip *clip);
    void execute() override;
    void undo() override;

private:
    bool m_oldUseTrack = true;
    SingerInfo m_oldOwnSingerInfo;
    SpeakerInfo m_oldOwnSpeakerInfo;
    SpeakerMixData m_oldOwnSpeakerMixData;

    SingerInfo m_newSingerInfo;
    SpeakerInfo m_newSpeakerInfo;
    SpeakerMixData m_newSpeakerMixData;
    SingingClip *m_clip = nullptr;
};

#endif // ENABLECLIPDYNAMICSPEAKERMIXACTION_H
