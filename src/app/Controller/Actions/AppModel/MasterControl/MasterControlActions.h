#ifndef MASTERCONTROLACTIONS_H
#define MASTERCONTROLACTIONS_H

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/History/ActionSequence.h>

class MasterControlActions : public ActionSequence {
public:
    void editMasterControl(const TrackControl &control, AppModel *model);
};



#endif //MASTERCONTROLACTIONS_H
