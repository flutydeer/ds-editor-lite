#include "DspxProjectConverterUi.h"

#include <utility>

DspxProjectConverterUi::DspxProjectConverterUi(LoopSettings loopSettings)
    : m_loopSettings(std::move(loopSettings)) {
}

void DspxProjectConverterUi::applyLoadedLoopSettings(const LoopSettings &loopSettings) {
    m_loopSettings = loopSettings;
}

LoopSettings DspxProjectConverterUi::loopSettingsToSave() const {
    return m_loopSettings;
}
