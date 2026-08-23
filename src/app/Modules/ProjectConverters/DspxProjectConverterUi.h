#ifndef DSPXPROJECTCONVERTERUI_H
#define DSPXPROJECTCONVERTERUI_H

#include <lite/ProjectConverters/DspxProjectConverter.h>

// App-side converter carrying the GUI loop snapshot used by DSPX save/load.
class DspxProjectConverterUi final : public DspxProjectConverter {
public:
    explicit DspxProjectConverterUi(LoopSettings loopSettings = {});

protected:
    void applyLoadedLoopSettings(const LoopSettings &loopSettings) override;
    LoopSettings loopSettingsToSave() const override;

private:
    LoopSettings m_loopSettings;
};

#endif // DSPXPROJECTCONVERTERUI_H
