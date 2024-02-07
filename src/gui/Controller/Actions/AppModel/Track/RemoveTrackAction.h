//
// Created by fluty on 2024/2/8.
//

#ifndef REMOVETRACKSACTION_H
#define REMOVETRACKSACTION_H

#include "Controller/History/IAction.h"
#include "Model/AppModel.h"

class RemoveTrackAction : public IAction {
public:
    static RemoveTrackAction *build(DsTrack *track, int index, AppModel *model);
    void execute() override;
    void undo() override;

private:
    DsTrack *m_track = nullptr;
    int m_index = -1;
    AppModel *m_model = nullptr;
};

#endif //REMOVETRACKSACTION_H
