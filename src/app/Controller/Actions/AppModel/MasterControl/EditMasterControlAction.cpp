//
// Created by FlutyDeer on 2025/5/30.
//

#include "EditMasterControlAction.h"

EditMasterControlAction::EditMasterControlAction(const TrackControl &control, AppModel *model) {
    m_appModel = model;
    m_oldControl = model->masterControl();
    m_newControl = control;
}

void EditMasterControlAction::execute() {
    m_appModel->setMasterControl(m_newControl);
}

void EditMasterControlAction::undo() {
    m_appModel->setMasterControl(m_oldControl);
}
