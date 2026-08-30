#ifndef APPOPTIONSAUTOMATIONADAPTER_H
#define APPOPTIONSAUTOMATIONADAPTER_H

#include "SettingsAutomationFacade.h"
#include "PresetAutomationFacade.h"

class AppOptions;

namespace Automation {

    SettingsRuntimeServices createAppOptionsAutomationServices(AppOptions *options);
    PresetRuntimeServices createAppOptionsPresetAutomationServices(AppOptions *options);

} // namespace Automation

#endif // APPOPTIONSAUTOMATIONADAPTER_H
