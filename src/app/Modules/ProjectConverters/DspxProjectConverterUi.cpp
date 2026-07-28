#include "DspxProjectConverterUi.h"

#include "Model/AppStatus/AppStatus.h"

void DspxProjectConverterUi::applyLoadedLoopSettings(const LoopSettings &loopSettings) {
    appStatus->loopSettings.set(loopSettings);
}

LoopSettings DspxProjectConverterUi::loopSettingsToSave() const {
    return appStatus->loopSettings.get();
}
