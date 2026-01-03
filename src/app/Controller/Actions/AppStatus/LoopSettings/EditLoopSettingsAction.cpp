//
// Created by kuro on 2024/1/3.
//

#include "EditLoopSettingsAction.h"
#include "Model/AppStatus/AppStatus.h"

EditLoopSettingsAction *EditLoopSettingsAction::build(const LoopSettings &oldSettings, const LoopSettings &newSettings) {
    const auto a = new EditLoopSettingsAction;
    a->m_oldSettings = oldSettings;
    a->m_newSettings = newSettings;
    return a;
}

void EditLoopSettingsAction::execute() {
    appStatus->loopSettings.set(m_newSettings);
}

void EditLoopSettingsAction::undo() {
    appStatus->loopSettings.set(m_oldSettings);
}
