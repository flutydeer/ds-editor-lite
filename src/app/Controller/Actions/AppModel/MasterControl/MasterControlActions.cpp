//
// Created by FlutyDeer on 2025/5/30.
//

#include "MasterControlActions.h"

#include "EditMasterControlAction.h"

void MasterControlActions::editMasterControl(const TrackControl &control, AppModel* model) {
    addAction(new EditMasterControlAction(control, model));
}
