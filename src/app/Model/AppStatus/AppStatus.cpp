//
// Created by OrangeCat on 24-8-24.
//

#include "AppStatus.h"

AppStatus::AppStatus() {
    // Modules
    languageModuleStatus.setNotify(
        [=](auto value) { emit moduleStatusChanged(ModuleType::Language, value); });
    inferenceEngineStatus.setNotify(
        [=](auto value) { emit moduleStatusChanged(ModuleType::Inference, value); });

    // Main Window
    trackPanelCollapsed.setNotify([=](auto value) { emit trackPanelCollapseStateChanged(value); });
    clipPanelCollapsed.setNotify([=](auto value) { emit clipPanelCollapseStateChanged(value); });

    // Project
    quantize.setNotify([=](auto value) { emit quantizeChanged(value); });
    projectEditableLength.setNotify([=](auto value) { emit projectEditableLengthChanged(value); });
    selectedTrackIndex.setNotify([=](auto value) { emit selectedTrackIndexChanged(value); });
    activeClipId.setNotify([=](auto value) { emit activeClipIdChanged(value); });
    selectedNotes.setNotify([=](auto value) { emit noteSelectionChanged(value); });
    currentEditObject.setNotify([=](auto value) { emit editingChanged(value); });
}