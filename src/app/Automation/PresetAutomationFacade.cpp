#include "PresetAutomationFacade.h"
#include "OperationIds.h"

#include <lite/ProjectModel/AppModel/SpeakerMixData.h>

#include <QUuid>

#include <cmath>

namespace Automation {
    namespace {
        AutomationError unavailable() {
            AutomationError error;
            error.code = AutomationErrorCode::HostCapabilityUnavailable;
            error.message = QStringLiteral("Speaker mix preset storage is unavailable");
            return error;
        }

        AutomationError persistenceError() {
            AutomationError error;
            error.code = AutomationErrorCode::IoError;
            error.message = QStringLiteral("Speaker mix presets could not be saved");
            return error;
        }

        AutomationError presetNotFound() {
            AutomationError error;
            error.code = AutomationErrorCode::NotFound;
            error.fieldPath = QStringLiteral("preset_id");
            error.message = QStringLiteral("Speaker mix preset was not found");
            return error;
        }

        AutomationResult<AutomationUnit> validatePreset(const SpeakerMixPresetDto &preset) {
            if (preset.name.trimmed().isEmpty() || preset.singerId.trimmed().isEmpty() ||
                preset.packageId.trimmed().isEmpty()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("preset"),
                    QStringLiteral("Preset name, singer ID and package ID are required"));
            }
            if (preset.sources.size() < 2 ||
                preset.fixedWeights.size() != preset.sources.size() - 1) {
                return AutomationError::invalidArgument(
                    QStringLiteral("sources"),
                    QStringLiteral("Preset weights must describe at least two sources"));
            }
            for (const auto &source : preset.sources) {
                if (source.speakerId.trimmed().isEmpty()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("sources.speaker_id"),
                        QStringLiteral("Preset speaker ID is empty"));
                }
            }
            for (const double weight : preset.fixedWeights) {
                if (!std::isfinite(weight)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("fixed_weights"),
                        QStringLiteral("Preset weight is not finite"));
                }
            }
            return AutomationUnit{};
        }

        bool sameSinger(const SpeakerMixPresetDto &lhs, const SpeakerMixPresetDto &rhs) {
            return lhs.singerId == rhs.singerId && lhs.packageId == rhs.packageId &&
                   lhs.packageVersion == rhs.packageVersion;
        }
    }

    PresetAutomationFacade::PresetAutomationFacade(AutomationDispatcher &dispatcher,
                                                   PresetRuntimeServices services)
        : m_dispatcher(dispatcher), m_services(std::move(services)) {
    }

    AutomationResult<QList<SpeakerMixPresetDto>> PresetAutomationFacade::getSpeakerMixPresets() {
        return m_dispatcher.dispatchApplicationQuery<QList<SpeakerMixPresetDto>>(
            OperationIds::speaker_mix_presets::list, [this] {
                if (!m_services.speakerMixPresets)
                    return AutomationResult<QList<SpeakerMixPresetDto>>(unavailable());
                return AutomationResult<QList<SpeakerMixPresetDto>>(m_services.speakerMixPresets());
            });
    }

    AutomationResult<SpeakerMixPresetDto>
        PresetAutomationFacade::saveSpeakerMixPreset(const ApplicationCommandContext &context,
                                                     SpeakerMixPresetDto preset) {
        return m_dispatcher.dispatchApplicationCommand<SpeakerMixPresetDto>(
            OperationIds::speaker_mix_presets::save, context,
            [this, preset = std::move(preset)](const bool validateOnly) mutable {
                if (!m_services.speakerMixPresets || !m_services.applySpeakerMixPresets)
                    return AutomationResult<SpeakerMixPresetDto>(unavailable());
                const auto validation = validatePreset(preset);
                if (!validation)
                    return AutomationResult<SpeakerMixPresetDto>(validation.getError());
                preset.fixedWeights = SpeakerMixModel::explicitWeightsFromFullWeights(
                    SpeakerMixModel::fullWeightsFromExplicitWeights(preset.fixedWeights));

                auto presets = m_services.speakerMixPresets();
                int existingIndex = -1;
                if (!preset.id.isEmpty()) {
                    for (int index = 0; index < presets.size(); ++index) {
                        if (presets.at(index).id == preset.id && existingIndex < 0)
                            existingIndex = index;
                    }
                    if (existingIndex < 0)
                        return AutomationResult<SpeakerMixPresetDto>(presetNotFound());
                }
                for (const auto &existing : std::as_const(presets)) {
                    if (existing.id != preset.id && sameSinger(existing, preset) &&
                        existing.name == preset.name) {
                        return AutomationResult<SpeakerMixPresetDto>(
                            AutomationError::invalidArgument(
                                QStringLiteral("name"),
                                QStringLiteral("A preset with this name already exists")));
                    }
                }

                if (validateOnly)
                    return AutomationResult<SpeakerMixPresetDto>(std::move(preset));

                const auto now = QDateTime::currentDateTimeUtc();
                if (preset.id.isEmpty())
                    preset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                if (!preset.createdAt.isValid())
                    preset.createdAt = now;
                preset.updatedAt = now;

                if (existingIndex < 0) {
                    presets.append(preset);
                } else {
                    if (presets.at(existingIndex).createdAt.isValid())
                        preset.createdAt = presets.at(existingIndex).createdAt;
                    for (int index = presets.size() - 1; index >= 0; --index) {
                        if (presets.at(index).id == preset.id)
                            presets.removeAt(index);
                    }
                    presets.insert(existingIndex, preset);
                }
                if (!m_services.applySpeakerMixPresets(presets))
                    return AutomationResult<SpeakerMixPresetDto>(persistenceError());
                return AutomationResult<SpeakerMixPresetDto>(std::move(preset));
            });
    }

    AutomationResult<ApplicationMutationResult>
        PresetAutomationFacade::deleteSpeakerMixPreset(const ApplicationCommandContext &context,
                                                       const QString &presetId) {
        return m_dispatcher.dispatchApplicationCommand<ApplicationMutationResult>(
            OperationIds::speaker_mix_presets::delete_preset, context,
            [this, presetId](const bool validateOnly) {
                if (presetId.trimmed().isEmpty()) {
                    return AutomationResult<ApplicationMutationResult>(
                        AutomationError::invalidArgument(QStringLiteral("preset_id"),
                                                         QStringLiteral("Preset ID is empty")));
                }
                if (!m_services.speakerMixPresets || !m_services.applySpeakerMixPresets)
                    return AutomationResult<ApplicationMutationResult>(unavailable());
                auto presets = m_services.speakerMixPresets();
                const auto previousSize = presets.size();
                presets.removeIf([&presetId](const SpeakerMixPresetDto &preset) {
                    return preset.id == presetId;
                });
                const bool changed = presets.size() != previousSize;
                if (!validateOnly && changed && !m_services.applySpeakerMixPresets(presets))
                    return AutomationResult<ApplicationMutationResult>(persistenceError());
                return AutomationResult<ApplicationMutationResult>({
                    .changed = changed,
                    .validatedOnly = validateOnly,
                });
            });
    }

} // namespace Automation
