//
// Created by fluty on 2024/2/8.
//

#ifndef REMOVETRACKSACTION_H
#define REMOVETRACKSACTION_H

#include "Controller/History/IAction.h"
#include "Model/AppModel.h"

class RemoveTrackAction : public IAction {
public:
    static RemoveTrackAction *build(DsTrack *track, AppModel *model);
    void execute() override;
    void undo() override;

private:
    DsTrack *m_track = nullptr;
    AppModel *m_model = nullptr;
    QList<DsTrack *> m_originalTracks;
};

#endif //REMOVETRACKSACTION_H
