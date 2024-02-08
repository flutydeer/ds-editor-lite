//
// Created by fluty on 2024/2/8.
//

#ifndef APPENDTRACKSACTION_H
#define APPENDTRACKSACTION_H

#include "Controller/History/IAction.h"
#include "Model/AppModel.h"

class AppendTrackAction final : public IAction {
public:
    static AppendTrackAction *build(DsTrack *track, AppModel *model);
    void execute() override;
    void undo() override;

private:
    DsTrack *m_track = nullptr;
    AppModel *m_model = nullptr;
};



#endif // APPENDTRACKSACTION_H
