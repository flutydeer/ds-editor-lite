//
// Created by FlutyDeer on 2025/5/30.
//

#ifndef EDITMASTERCONTROLACTION_H
#define EDITMASTERCONTROLACTION_H

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/TrackControl.h"
#include "Modules/History/IAction.h"


class EditMasterControlAction : public IAction {
public:
    explicit EditMasterControlAction(const TrackControl &control, AppModel *model);
    void execute() override;
    void undo() override;

private:
    TrackControl m_oldControl;
    TrackControl m_newControl;
    AppModel *m_appModel = nullptr;
};



#endif //EDITMASTERCONTROLACTION_H
