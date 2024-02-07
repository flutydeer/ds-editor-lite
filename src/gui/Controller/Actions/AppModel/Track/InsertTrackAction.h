//
// Created by fluty on 2024/2/8.
//

#ifndef ADDTRACKSACTION_H
#define ADDTRACKSACTION_H

#include "Controller/History/IAction.h"
#include "Model/AppModel.h"

class InsertTrackAction : public IAction {
public:
    static InsertTrackAction *build(DsTrack *track, int index, AppModel *model);
    void execute() override;
    void undo() override;

private:
    DsTrack *m_track = nullptr;
    int m_index = -1;
    AppModel *m_model = nullptr;
};



#endif // ADDTRACKSACTION_H
