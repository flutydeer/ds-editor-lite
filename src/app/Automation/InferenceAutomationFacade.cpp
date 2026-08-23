#include "InferenceAutomationFacade.h"
#include "OperationIds.h"

namespace Automation {
    namespace {
        AutomationError unavailable() {
            AutomationError error;
            error.code = AutomationErrorCode::ModuleNotReady;
            error.message = QStringLiteral("Inference mutation services are unavailable");
            return error;
        }

        bool canAdvanceRevision(const InferenceMutationKind kind) {
            switch (kind) {
                case InferenceMutationKind::ApplyPronunciations:
                case InferenceMutationKind::ApplyPhonemeNames:
                case InferenceMutationKind::ApplyDuration:
                case InferenceMutationKind::ApplyPitch:
                case InferenceMutationKind::ApplyVariance:
                case InferenceMutationKind::ResetStage:
                case InferenceMutationKind::RefreshSpeakerMix:
                case InferenceMutationKind::RebuildOriginalParams:
                    return true;
                case InferenceMutationKind::ApplyAcoustic:
                case InferenceMutationKind::InvalidateClip:
                case InferenceMutationKind::ResegmentClip:
                case InferenceMutationKind::RefreshParamInput:
                    return false;
            }
            return false;
        }
    }

    InferenceAutomationFacade::InferenceAutomationFacade(OperationCatalog &catalog,
                                                         AutomationDispatcher &dispatcher,
                                                         CommandCommitter &committer,
                                                         InferenceRuntimeServices services)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_committer(committer),
          m_services(std::move(services)) {
        registerOperations();
    }

    AutomationResult<InferenceMutationResultDto>
        InferenceAutomationFacade::applyMutation(const CommandContext &context,
                                                 const InferenceMutationRequest &request) {
        const auto operation = operationId(request.kind);
        return m_dispatcher.dispatchDocumentCommandResult<InferenceMutationResultDto>(
            operation, context, {},
            [this, request](DocumentSession &session, const bool validateOnly) {
                if (!m_services.prepareMutation)
                    return AutomationResult<InferenceMutationResultDto>(unavailable());
                auto prepared = m_services.prepareMutation(session.model(), request);
                if (!prepared)
                    return AutomationResult<InferenceMutationResultDto>(prepared.getError());

                InferenceMutationResultDto result;
                if (validateOnly) {
                    if (prepared.get().advancesRevision) {
                        result.mutation = m_committer.preview(session, prepared.get().changed,
                                                              prepared.get().affectedObjects);
                    } else {
                        result.mutation = m_committer.unchanged(session);
                        result.mutation.changed = prepared.get().changed;
                        result.mutation.validatedOnly = true;
                        result.mutation.affectedObjects = prepared.get().affectedObjects;
                    }
                    return AutomationResult<InferenceMutationResultDto>(std::move(result));
                }

                const auto apply = [&prepared, &result] {
                    if (prepared.get().apply)
                        prepared.get().apply(result.sideEffects);
                };
                if (prepared.get().advancesRevision) {
                    result.mutation = m_committer.commitStateChange(
                        session, prepared.get().changed, apply, prepared.get().affectedObjects);
                } else {
                    result.mutation = m_committer.commitDerivedStateChange(
                        session, prepared.get().changed, apply, prepared.get().affectedObjects);
                }
                return AutomationResult<InferenceMutationResultDto>(std::move(result));
            });
    }

    OperationId InferenceAutomationFacade::operationId(const InferenceMutationKind kind) {
        switch (kind) {
            case InferenceMutationKind::ApplyPronunciations:
                return OperationIds::inference::apply_pronunciations;
            case InferenceMutationKind::ApplyPhonemeNames:
                return OperationIds::inference::apply_phoneme_names;
            case InferenceMutationKind::ApplyDuration:
                return OperationIds::inference::apply_duration;
            case InferenceMutationKind::ApplyPitch:
                return OperationIds::inference::apply_pitch;
            case InferenceMutationKind::ApplyVariance:
                return OperationIds::inference::apply_variance;
            case InferenceMutationKind::ApplyAcoustic:
                return OperationIds::inference::apply_acoustic;
            case InferenceMutationKind::ResetStage:
                return OperationIds::inference::reset_stage;
            case InferenceMutationKind::InvalidateClip:
                return OperationIds::inference::invalidate_clip;
            case InferenceMutationKind::ResegmentClip:
                return OperationIds::inference::resegment_clip;
            case InferenceMutationKind::RefreshSpeakerMix:
                return OperationIds::inference::refresh_speaker_mix;
            case InferenceMutationKind::RefreshParamInput:
                return OperationIds::inference::refresh_param_input;
            case InferenceMutationKind::RebuildOriginalParams:
                return OperationIds::inference::rebuild_original_params;
        }
        Q_UNREACHABLE_RETURN({});
    }

    void InferenceAutomationFacade::registerOperations() {
        const auto add = [this](const InferenceMutationKind kind) {
            const auto result = m_catalog.add({
                .id = operationId(kind),
                .category = QStringLiteral("inference"),
                .kind = OperationKind::Command,
                .syncMode = SyncMode::Synchronous,
                .documentPolicy = DocumentPolicy::Write,
                .revisionPolicy =
                    canAdvanceRevision(kind) ? RevisionPolicy::Increment : RevisionPolicy::Check,
                .historyPolicy = HistoryPolicy::None,
                .fileAccess = FileAccessPolicy::None,
                .hostAvailability = HostAvailability::Core,
                .safety = SafetyClass::Reversible,
                .exposure = ExposurePolicy::InternalOnly,
                .idempotency = IdempotencyPolicy::Unsupported,
            });
            Q_ASSERT(result);
        };
        add(InferenceMutationKind::ApplyPronunciations);
        add(InferenceMutationKind::ApplyPhonemeNames);
        add(InferenceMutationKind::ApplyDuration);
        add(InferenceMutationKind::ApplyPitch);
        add(InferenceMutationKind::ApplyVariance);
        add(InferenceMutationKind::ApplyAcoustic);
        add(InferenceMutationKind::ResetStage);
        add(InferenceMutationKind::InvalidateClip);
        add(InferenceMutationKind::ResegmentClip);
        add(InferenceMutationKind::RefreshSpeakerMix);
        add(InferenceMutationKind::RefreshParamInput);
        add(InferenceMutationKind::RebuildOriginalParams);
    }

} // namespace Automation
