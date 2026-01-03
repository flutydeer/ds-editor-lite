//
// Created by kuro on 2024/1/3.
//

#ifndef LOOPSETTINGSACTIONS_H
#define LOOPSETTINGSACTIONS_H

#include "Modules/History/ActionSequence.h"
#include "Model/AppModel/LoopSettings.h"

class LoopSettingsActions : public ActionSequence {
public:
    void editLoopSettings(const LoopSettings &oldSettings, const LoopSettings &newSettings);
};

#endif // LOOPSETTINGSACTIONS_H
