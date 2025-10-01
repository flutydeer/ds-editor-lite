//
// Created by FlutyDeer on 2025/5/30.
//

#ifndef MASTERCONTROLACTIONS_H
#define MASTERCONTROLACTIONS_H

#include "Model/AppModel/AppModel.h"
#include "Modules/History/ActionSequence.h"

class MasterControlActions : public ActionSequence {
public:
    void editMasterControl(const TrackControl &control, AppModel *model);
};



#endif // MASTERCONTROLACTIONS_H
