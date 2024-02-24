//
// Created by fluty on 2024/2/8.
//

#ifndef APPENDTRACKSACTION_H
#define APPENDTRACKSACTION_H

#include "Controller/History/IAction.h"

class Track;
class AppModel;

class AppendTrackAction final : public IAction {
public:
    static AppendTrackAction *build(Track *track, AppModel *model);
    void execute() override;
    void undo() override;

private:
    Track *m_track = nullptr;
    AppModel *m_model = nullptr;
};



#endif // APPENDTRACKSACTION_H
