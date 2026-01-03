//
// Created by kuro on 2024/1/3.
//

#include "LoopSettingsActions.h"
#include "EditLoopSettingsAction.h"

void LoopSettingsActions::editLoopSettings(const LoopSettings &oldSettings, const LoopSettings &newSettings) {
    addAction(EditLoopSettingsAction::build(oldSettings, newSettings));
    setName(tr("Edit Loop Region"));
}
