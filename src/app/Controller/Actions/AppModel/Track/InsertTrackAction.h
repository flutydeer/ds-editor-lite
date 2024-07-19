//
// Created by fluty on 2024/2/8.
//

#ifndef ADDTRACKSACTION_H
#define ADDTRACKSACTION_H

#include "Modules/History/IAction.h"

#include <QtTypes>

class Track;
class AppModel;

class InsertTrackAction final: public IAction {
public:
    static InsertTrackAction *build(Track *track, qsizetype index, AppModel *model);
    void execute() override;
    void undo() override;

private:
    Track *m_track = nullptr;
    qsizetype m_index = -1;
    AppModel *m_model = nullptr;
};



#endif // ADDTRACKSACTION_H
