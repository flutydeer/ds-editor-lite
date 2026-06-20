//
// Created by FlutyDeer on 2026/6/20.
//

#ifndef REPLACESPEAKERMIXACTION_H
#define REPLACESPEAKERMIXACTION_H

#include "Model/AppModel/SpeakerMixData.h"
#include "Modules/History/IAction.h"

class SingingClip;
using SpeakerMixModel::SpeakerMixData;

class ReplaceSpeakerMixAction final : public IAction {
public:
    explicit ReplaceSpeakerMixAction(const SpeakerMixData &data, SingingClip *clip);
    void execute() override;
    void undo() override;

private:
    SpeakerMixData m_oldData;
    SpeakerMixData m_newData;
    SingingClip *m_clip = nullptr;
};

#endif // REPLACESPEAKERMIXACTION_H
