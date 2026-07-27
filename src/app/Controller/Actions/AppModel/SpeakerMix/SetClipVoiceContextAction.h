#ifndef SETCLIPVOICECONTEXTACTION_H
#define SETCLIPVOICECONTEXTACTION_H

#include <lite/ProjectModel/AppModel/SpeakerMixData.h>
#include <lite/History/IAction.h>
#include <lite/ProjectModel/Voice/SingerInfo.h>

class SingingClip;
class SpeakerInfo;
using SpeakerMixModel::SpeakerMixData;

class SetClipVoiceContextAction final : public IAction {
public:
    SetClipVoiceContextAction(bool newUseTrackVoiceContext, const SingerInfo &newOwnSingerInfo,
                              const SpeakerInfo &newOwnSpeakerInfo,
                              const SpeakerMixData &newOwnSpeakerMixData, SingingClip *clip);

    void execute() override;
    void undo() override;

private:
    struct Snapshot {
        bool useTrackVoiceContext = true;
        SingerInfo ownSingerInfo;
        SpeakerInfo ownSpeakerInfo;
        SpeakerMixData ownSpeakerMixData;
    };

    static Snapshot snapshotFromClip(const SingingClip *clip);
    void applySnapshot(const Snapshot &snapshot) const;

    Snapshot m_oldSnapshot;
    Snapshot m_newSnapshot;
    SingingClip *m_clip = nullptr;
};

#endif // SETCLIPVOICECONTEXTACTION_H
