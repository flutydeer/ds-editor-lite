#include "MasterControlActions.h"

#include "EditMasterControlAction.h"

void MasterControlActions::editMasterControl(const TrackControl &control, AppModel* model) {
    addAction(new EditMasterControlAction(control, model));
}
