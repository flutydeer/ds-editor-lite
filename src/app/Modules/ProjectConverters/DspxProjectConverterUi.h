#ifndef DSPXPROJECTCONVERTERUI_H
#define DSPXPROJECTCONVERTERUI_H

#include <lite/ProjectConverters/DspxProjectConverter.h>

// App-side DspxProjectConverter: bridges the loop region to AppStatus so the
// converter domain logic itself stays free of app-runtime state.
class DspxProjectConverterUi final : public DspxProjectConverter {
protected:
    void applyLoadedLoopSettings(const LoopSettings &loopSettings) override;
    LoopSettings loopSettingsToSave() const override;
};

#endif // DSPXPROJECTCONVERTERUI_H
