//
// Created by OrangeCat on 24-8-24.
//

#include "AppStatus.h"

AppStatus::AppStatus() {
    // Modules
    languageModuleStatus.setNotifyCallback(
        [=](const ModuleStatus value) { emit moduleStatusChanged(ModuleType::Language, value); });

    // Main Window
    trackPanelCollapsed.setNotifyCallback(
        [=](const bool value) { emit trackPanelCollapseStateChanged(value); });
    clipPanelCollapsed.setNotifyCallback(
        [=](const bool value) { emit clipPanelCollapseStateChanged(value); });

    // Project
    quantize.setNotifyCallback([=](const int value) { emit quantizeChanged(value); });
    projectEditableLength.setNotifyCallback(
        [=](const int value) { emit projectEditableLengthChanged(value); });
    selectedTrackIndex.setNotifyCallback(
        [=](const int value) { emit selectedTrackIndexChanged(value); });
    activeClipId.setNotifyCallback([=](const int value) { emit activeClipIdChanged(value); });
}