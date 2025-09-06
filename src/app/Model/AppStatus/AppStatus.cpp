//
// Created by OrangeCat on 24-8-24.
//

#include "AppStatus.h"

AppStatus::AppStatus(QObject *parent) : QObject(parent) {
    // Modules
    languageModuleStatus.onChanged(
        [this](auto value) { emit moduleStatusChanged(ModuleType::Language, value); });
    inferEngineEnvStatus.onChanged(
        [this](auto value) { emit moduleStatusChanged(ModuleType::Inference, value); });

    // Main Window
    trackPanelCollapsed.onChanged([this](auto value) { emit trackPanelCollapseStateChanged(value); });
    clipPanelCollapsed.onChanged([this](auto value) { emit clipPanelCollapseStateChanged(value); });

    // Project
    quantize.onChanged([this](auto value) { emit quantizeChanged(value); });
    projectEditableLength.onChanged([this](auto value) { emit projectEditableLengthChanged(value); });
    selectedTrackIndex.onChanged([this](auto value) { emit selectedTrackIndexChanged(value); });
    activeClipId.onChanged([this](auto value) { emit activeClipIdChanged(value); });
    selectedNotes.onChanged([this](const auto &value) { emit noteSelectionChanged(value); });
    currentEditObject.onChanged([this](auto value) { emit editingChanged(value); });
}

AppStatus::~AppStatus() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(AppStatus)
