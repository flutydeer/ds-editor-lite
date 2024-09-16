//
// Created by OrangeCat on 24-8-24.
//

#include "AppStatus.h"

AppStatus::AppStatus() {
    // Modules
    languageModuleStatus.setNotify(
        [=](const ModuleStatus value) { emit moduleStatusChanged(ModuleType::Language, value); });

    // Main Window
    trackPanelCollapsed.setNotify(
        [=](const bool value) { emit trackPanelCollapseStateChanged(value); });
    clipPanelCollapsed.setNotify(
        [=](const bool value) { emit clipPanelCollapseStateChanged(value); });

    // Project
    quantize.setNotify([=](const int value) { emit quantizeChanged(value); });
    projectEditableLength.setNotify(
        [=](const int value) { emit projectEditableLengthChanged(value); });
    selectedTrackIndex.setNotify(
        [=](const int value) { emit selectedTrackIndexChanged(value); });
    activeClipId.setNotify([=](const int value) { emit activeClipIdChanged(value); });
    selectedNotes.setNotify(
        [=](const QList<int> &value) { emit noteSelectionChanged(value); });
    currentEditObject.setNotify(
        [=](const EditObjectType value) { emit editingChanged(value); });
}