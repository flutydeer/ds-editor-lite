#ifndef INFERENCEAUTOMATIONFACADE_H
#define INFERENCEAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "CommandCommitter.h"

#include "Modules/Inference/Models/InferInputNote.h"
#include "Modules/Inference/Models/InferParamCurve.h"

#include <lite/ProjectModel/AppModel/Phonemes.h>
#include <lite/ProjectModel/AppModel/Params.h>

#include <functional>

class AppModel;

namespace Automation {

    enum class InferenceMutationKind {
        ApplyPronunciations,
        ApplyPhonemeNames,
        ApplyDuration,
        ApplyPitch,
        ApplyVariance,
        ApplyAcoustic,
        ResetStage,
        InvalidateClip,
        ResegmentClip,
        RefreshSpeakerMix,
        RefreshParamInput,
        RebuildOriginalParams,
    };

    enum class InferenceStage {
        Duration,
        Pitch,
        Variance,
        Acoustic,
    };

    struct InferencePronunciationDto {
        NoteId noteId;
        QString pronunciation;
        QStringList candidates;
    };

    struct InferencePhonemeNamesDto {
        NoteId noteId;
        QList<PhonemeName> phonemeNames;
    };

    struct InferenceVarianceResultDto {
        InferParamCurve breathiness;
        InferParamCurve tension;
        InferParamCurve voicing;
        InferParamCurve energy;
        InferParamCurve mouthOpening;
    };

    struct InferencePieceTarget {
        ClipId clipId;
        PieceId pieceId;
    };

    struct InferenceMutationRequest {
        InferenceMutationKind kind = InferenceMutationKind::ApplyPronunciations;
        ClipId clipId;
        PieceId pieceId;
        QList<PieceId> pieceIds;
        QList<InferencePieceTarget> pieceTargets;
        QList<NoteId> noteIds;
        QList<InferencePronunciationDto> pronunciations;
        QList<InferencePhonemeNamesDto> phonemeNames;
        QList<InferInputNote> durationResult;
        InferParamCurve pitchResult;
        InferenceVarianceResultDto varianceResult;
        QString acousticPath;
        InferenceStage stage = InferenceStage::Duration;
        int pitchSmoothKernelSize = -1;
        bool bumpClipInferenceRevision = true;
        ParamInfo::Name parameterName = ParamInfo::Unknown;
    };

    struct InferenceMutationSideEffects {
        QList<PieceId> addedPieces;
        QList<PieceId> removedPieces;
        QList<PieceId> changedPieces;
    };

    struct InferenceMutationResultDto {
        MutationResult mutation;
        InferenceMutationSideEffects sideEffects;
    };

    struct PreparedInferenceMutation {
        bool changed = false;
        bool advancesRevision = false;
        QList<ObjectRef> affectedObjects;
        std::function<void(InferenceMutationSideEffects &)> apply;
    };

    struct InferenceRuntimeServices {
        std::function<AutomationResult<PreparedInferenceMutation>(AppModel *,
                                                                  const InferenceMutationRequest &)>
            prepareMutation;
    };

    class InferenceAutomationFacade final {
    public:
        InferenceAutomationFacade(OperationCatalog &catalog, AutomationDispatcher &dispatcher,
                                  CommandCommitter &committer,
                                  InferenceRuntimeServices services = {});

        AutomationResult<InferenceMutationResultDto>
            applyMutation(const CommandContext &context, const InferenceMutationRequest &request);

        [[nodiscard]] static OperationId operationId(InferenceMutationKind kind);
        [[nodiscard]] static QStringList supportedStages();

    private:
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        CommandCommitter &m_committer;
        InferenceRuntimeServices m_services;
    };

} // namespace Automation

#endif // INFERENCEAUTOMATIONFACADE_H
