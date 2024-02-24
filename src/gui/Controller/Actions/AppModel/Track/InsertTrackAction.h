//
// Created by fluty on 2024/2/8.
//

#ifndef ADDTRACKSACTION_H
#define ADDTRACKSACTION_H

#include "Controller/History/IAction.h"

class Track;
class AppModel;

class InsertTrackAction final: public IAction {
public:
    static InsertTrackAction *build(Track *track, int index, AppModel *model);
    void execute() override;
    void undo() override;

private:
    Track *m_track = nullptr;
    int m_index = -1;
    AppModel *m_model = nullptr;
};



#endif // ADDTRACKSACTION_H
