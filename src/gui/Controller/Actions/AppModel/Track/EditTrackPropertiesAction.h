//
// Created by fluty on 2024/2/8.
//

#ifndef EDITETRACKACTION_H
#define EDITETRACKACTION_H

#include "Controller/History/IAction.h"
#include "Model/DsTrack.h"

class EditTrackPropertiesAction final : public IAction {
public:
    static EditTrackPropertiesAction *build(const DsTrack::TrackProperties &oldArgs,
                                            const DsTrack::TrackProperties &newArgs,
                                            DsTrack *track);
    void execute() override;
    void undo() override;

private:
    DsTrack::TrackProperties m_oldArgs;
    DsTrack::TrackProperties m_newArgs;
    DsTrack *m_track = nullptr;
};



#endif // EDITETRACKACTION_H
