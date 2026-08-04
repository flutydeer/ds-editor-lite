#include "AppStatus.h"

AppStatus::AppStatus(QObject *parent) : QObject(parent) {
    // Modules
    languageModuleStatus.onChanged(
        [this](auto value) { emit moduleStatusChanged(ModuleType::Language, value); });
    languageModuleError.onChanged(
        [this](const auto &value) { emit languageModuleErrorChanged(value); });
    inferEngineEnvStatus.onChanged(
        [this](auto value) { emit moduleStatusChanged(ModuleType::Inference, value); });
    packageModuleStatus.onChanged(
        [this](auto value) { emit moduleStatusChanged(ModuleType::Package, value); });

    // Main Window
    trackPanelCollapsed.onChanged(
        [this](auto value) { emit trackPanelCollapseStateChanged(value); });
    bottomPanelCollapsed.onChanged(
        [this](auto value) { emit bottomPanelCollapseStateChanged(value); });

    // Project
    pianoRollQuantize.onChanged([this](auto value) { emit pianoRollQuantizeChanged(value); });
    projectEditableLength.onChanged(
        [this](auto value) { emit projectEditableLengthChanged(value); });
    selectedTrackIndex.onChanged([this](auto value) { emit selectedTrackIndexChanged(value); });
    activeClipId.onChanged([this](auto value) { emit activeClipIdChanged(value); });
    selectedNotes.onChanged([this](const auto &value) { emit noteSelectionChanged(value); });
    selectedClips.onChanged([this](const auto &value) { emit clipSelectionChanged(value); });
    currentEditObject.onChanged([this](auto value) { emit editingChanged(value); });

    // Loop
    loopSettings.onChanged([this](const auto &value) { emit loopSettingsChanged(value); });

    // Playback viewport
    trackAutoPageTurnEnabled.onChanged(
        [this](auto value) { emit trackAutoPageTurnEnabledChanged(value); });
    trackAutoPageTurnAvailable.onChanged(
        [this](auto value) { emit trackAutoPageTurnAvailabilityChanged(value); });
    pianoRollAutoPageTurnEnabled.onChanged(
        [this](auto value) { emit pianoRollAutoPageTurnEnabledChanged(value); });
    pianoRollAutoPageTurnAvailable.onChanged(
        [this](auto value) { emit pianoRollAutoPageTurnAvailabilityChanged(value); });
}

AppStatus::~AppStatus() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(AppStatus)
