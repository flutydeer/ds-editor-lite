#include "AppStatus.h"

#include <algorithm>

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
    autoPageTurnEnabled.onChanged([this](auto value) { emit autoPageTurnEnabledChanged(value); });
    autoPageTurnAvailable.onChanged(
        [this](auto value) { emit autoPageTurnAvailabilityChanged(value); });
}

AppStatus::~AppStatus() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(AppStatus)

void AppStatus::reportAutoPageTurnAvailability(QObject *source, const bool participating,
                                               const bool available) {
    if (!source)
        return;

    if (!m_autoPageTurnSources.contains(source)) {
        m_autoPageTurnSources.insert(source);
        connect(source, &QObject::destroyed, this, [this](QObject *object) {
            m_autoPageTurnSources.remove(object);
            m_autoPageTurnAvailability.remove(object);
            updateAutoPageTurnAvailability();
        });
    }

    if (participating)
        m_autoPageTurnAvailability.insert(source, available);
    else
        m_autoPageTurnAvailability.remove(source);
    updateAutoPageTurnAvailability();
}

void AppStatus::updateAutoPageTurnAvailability() {
    const bool available =
        m_autoPageTurnAvailability.isEmpty() ||
        std::any_of(m_autoPageTurnAvailability.cbegin(), m_autoPageTurnAvailability.cend(),
                    [](const bool value) { return value; });
    autoPageTurnAvailable = available;
}
