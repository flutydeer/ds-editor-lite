//
// Created by OrangeCat on 24-8-24.
//

#include "AppStatus.h"

AppStatus::AppStatus() {
    // Modules
    languageModuleStatus.onChanged(
        [=](auto value) { emit moduleStatusChanged(ModuleType::Language, value); });
    inferEngineEnvStatus.onChanged(
        [=](auto value) { emit moduleStatusChanged(ModuleType::Inference, value); });

    // Main Window
    trackPanelCollapsed.onChanged([=](auto value) { emit trackPanelCollapseStateChanged(value); });
    clipPanelCollapsed.onChanged([=](auto value) { emit clipPanelCollapseStateChanged(value); });

    // Project
    quantize.onChanged([=](auto value) { emit quantizeChanged(value); });
    projectEditableLength.onChanged([=](auto value) { emit projectEditableLengthChanged(value); });
    selectedTrackIndex.onChanged([=](auto value) { emit selectedTrackIndexChanged(value); });
    activeClipId.onChanged([=](auto value) { emit activeClipIdChanged(value); });
    selectedNotes.onChanged([=](const auto &value) { emit noteSelectionChanged(value); });
    currentEditObject.onChanged([=](auto value) { emit editingChanged(value); });
}