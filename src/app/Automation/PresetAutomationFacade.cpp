#include "PresetAutomationFacade.h"
#include "OperationIds.h"

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

        AutomationResult<AutomationUnit> validatePreset(const SpeakerMixPresetDto &preset) {
            if (preset.name.trimmed().isEmpty() || preset.singerId.trimmed().isEmpty() ||
                preset.packageId.trimmed().isEmpty()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("preset"),
                    QStringLiteral("Preset name, singer ID and package ID are required"));
            }
            if (preset.sources.isEmpty() || preset.sources.size() != preset.fixedWeights.size()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("sources"),
                    QStringLiteral("Preset sources and weights must have the same non-zero size"));
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

    PresetAutomationFacade::PresetAutomationFacade(OperationCatalog &catalog,
                                                   AutomationDispatcher &dispatcher,
                                                   PresetRuntimeServices services)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_services(std::move(services)) {
        registerOperations();
    }

    AutomationResult<QList<SpeakerMixPresetDto>>
    PresetAutomationFacade::getSpeakerMixPresets() {
        return m_dispatcher.dispatchApplicationQuery<QList<SpeakerMixPresetDto>>(
            OperationIds::speaker_mix_presets::list, [this] {
                if (!m_services.speakerMixPresets)
                    return AutomationResult<QList<SpeakerMixPresetDto>>(unavailable());
                return AutomationResult<QList<SpeakerMixPresetDto>>(m_services.speakerMixPresets());
            });
    }

    AutomationResult<SpeakerMixPresetDto> PresetAutomationFacade::saveSpeakerMixPreset(
        const ApplicationCommandContext &context, SpeakerMixPresetDto preset) {
        return m_dispatcher.dispatchApplicationCommand<SpeakerMixPresetDto>(
            OperationIds::speaker_mix_presets::save, context,
            [this, preset = std::move(preset)](const bool validateOnly) mutable {
                if (!m_services.speakerMixPresets || !m_services.applySpeakerMixPresets)
                    return AutomationResult<SpeakerMixPresetDto>(unavailable());
                const auto validation = validatePreset(preset);
                if (!validation)
                    return AutomationResult<SpeakerMixPresetDto>(validation.getError());

                auto presets = m_services.speakerMixPresets();
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

                bool replaced = false;
                for (auto &existing : presets) {
                    if (existing.id != preset.id)
                        continue;
                    if (existing.createdAt.isValid())
                        preset.createdAt = existing.createdAt;
                    existing = preset;
                    replaced = true;
                    break;
                }
                if (!replaced)
                    presets.append(preset);
                if (!m_services.applySpeakerMixPresets(presets))
                    return AutomationResult<SpeakerMixPresetDto>(persistenceError());
                return AutomationResult<SpeakerMixPresetDto>(std::move(preset));
            });
    }

    AutomationResult<ApplicationMutationResult> PresetAutomationFacade::deleteSpeakerMixPreset(
        const ApplicationCommandContext &context, const QString &presetId) {
        if (presetId.trimmed().isEmpty()) {
            return AutomationError::invalidArgument(QStringLiteral("preset_id"),
                                                    QStringLiteral("Preset ID is empty"));
        }
        return m_dispatcher.dispatchApplicationCommand<ApplicationMutationResult>(
            OperationIds::speaker_mix_presets::delete_preset, context,
            [this, presetId](const bool validateOnly) {
                if (!m_services.speakerMixPresets || !m_services.applySpeakerMixPresets)
                    return AutomationResult<ApplicationMutationResult>(unavailable());
                auto presets = m_services.speakerMixPresets();
                const auto previousSize = presets.size();
                presets.removeIf(
                    [&presetId](const SpeakerMixPresetDto &preset) { return preset.id == presetId; });
                const bool changed = presets.size() != previousSize;
                if (!validateOnly && changed && !m_services.applySpeakerMixPresets(presets))
                    return AutomationResult<ApplicationMutationResult>(persistenceError());
                return AutomationResult<ApplicationMutationResult>({
                    .changed = changed,
                    .validatedOnly = validateOnly,
                });
            });
    }

    void PresetAutomationFacade::registerOperations() {
        const auto add = [this](OperationDescriptor descriptor) {
            const auto result = m_catalog.add(std::move(descriptor));
            Q_ASSERT(result);
        };
        add({
            .id = OperationIds::speaker_mix_presets::list,
            .category = QStringLiteral("speaker_mix_presets"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .documentPolicy = DocumentPolicy::None,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        const auto addCommand = [&add](const OperationId &id) {
            add({
                .id = id,
                .category = QStringLiteral("speaker_mix_presets"),
                .kind = OperationKind::Command,
                .syncMode = SyncMode::Synchronous,
                .documentPolicy = DocumentPolicy::None,
                .revisionPolicy = RevisionPolicy::None,
                .historyPolicy = HistoryPolicy::None,
                .fileAccess = FileAccessPolicy::Write,
                .hostAvailability = HostAvailability::Core,
                .safety = SafetyClass::Reversible,
                .exposure = ExposurePolicy::InternalOnly,
                .idempotency = IdempotencyPolicy::Unsupported,
            });
        };
        addCommand(OperationIds::speaker_mix_presets::save);
        addCommand(OperationIds::speaker_mix_presets::delete_preset);
    }

} // namespace Automation
