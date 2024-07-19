//
// Created by fluty on 2024/2/8.
//

#ifndef EDITETRACKACTION_H
#define EDITETRACKACTION_H

#include "Model/Track.h"
#include "Modules/History/IAction.h"

class EditTrackPropertiesAction final : public IAction {
public:
    static EditTrackPropertiesAction *build(const Track::TrackProperties &oldArgs,
                                            const Track::TrackProperties &newArgs,
                                            Track *track);
    void execute() override;
    void undo() override;

private:
    Track::TrackProperties m_oldArgs;
    Track::TrackProperties m_newArgs;
    Track *m_track = nullptr;
};



#endif // EDITETRACKACTION_H
