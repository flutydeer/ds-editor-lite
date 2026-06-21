#ifndef REPLACETRACKSPEAKERMIXACTION_H
#define REPLACETRACKSPEAKERMIXACTION_H

#include "Model/AppModel/SpeakerMixData.h"
#include "Modules/History/IAction.h"

class Track;
using SpeakerMixModel::SpeakerMixData;

class ReplaceTrackSpeakerMixAction final : public IAction {
public:
    explicit ReplaceTrackSpeakerMixAction(const SpeakerMixData &data, Track *track);
    void execute() override;
    void undo() override;

private:
    SpeakerMixData m_oldData;
    SpeakerMixData m_newData;
    Track *m_track = nullptr;
};

#endif // REPLACETRACKSPEAKERMIXACTION_H
