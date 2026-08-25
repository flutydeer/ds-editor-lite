#ifndef EDITMASTERCONTROLACTION_H
#define EDITMASTERCONTROLACTION_H

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/TrackControl.h>
#include <lite/History/IAction.h>

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



#endif // EDITMASTERCONTROLACTION_H
