//
// Created by kuro on 2024/1/3.
//

#ifndef EDITLOOPSETTINGSACTION_H
#define EDITLOOPSETTINGSACTION_H

#include "Modules/History/IAction.h"
#include "Model/AppModel/LoopSettings.h"

class EditLoopSettingsAction final : public IAction {
public:
    static EditLoopSettingsAction *build(const LoopSettings &oldSettings, const LoopSettings &newSettings);
    void execute() override;
    void undo() override;

private:
    LoopSettings m_oldSettings;
    LoopSettings m_newSettings;
};

#endif // EDITLOOPSETTINGSACTION_H
