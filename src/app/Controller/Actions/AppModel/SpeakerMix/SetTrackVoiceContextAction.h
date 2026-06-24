#ifndef SETTRACKVOICECONTEXTACTION_H
#define SETTRACKVOICECONTEXTACTION_H

#include "Model/AppModel/SpeakerMixData.h"
#include "Modules/History/IAction.h"
#include "Modules/PackageManager/Models/SingerInfo.h"

class Track;
class SpeakerInfo;
using SpeakerMixModel::SpeakerMixData;

class SetTrackVoiceContextAction final : public IAction {
public:
    SetTrackVoiceContextAction(const SingerInfo &newSingerInfo, const SpeakerInfo &newSpeakerInfo,
                               const SpeakerMixData &newSpeakerMixData, Track *track);

    void execute() override;
    void undo() override;

private:
    struct Snapshot {
        SingerInfo singerInfo;
        SpeakerInfo speakerInfo;
        SpeakerMixData speakerMixData;
    };

    static Snapshot snapshotFromTrack(const Track *track);
    void applySnapshot(const Snapshot &snapshot) const;

    Snapshot m_oldSnapshot;
    Snapshot m_newSnapshot;
    Track *m_track = nullptr;
};

#endif // SETTRACKVOICECONTEXTACTION_H
