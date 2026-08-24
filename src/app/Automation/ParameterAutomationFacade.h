#ifndef PARAMETERAUTOMATIONFACADE_H
#define PARAMETERAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "CommandCommitter.h"
#include "DocumentObjectResolver.h"
#include "ProjectAutomationDtos.h"

#include <lite/ProjectModel/AppModel/EffectiveVoiceContext.h>

#include <functional>

namespace Automation {

    struct ParameterSnapshotDto {
        DocumentVersion document;
        ClipId clipId;
        ParamInfo::Name name = ParamInfo::Unknown;
        Param::Type type = Param::Unknown;
        QList<CurveDraftDto> curves;
    };

    struct ParameterCapabilityDto {
        ParamInfo::Name name = ParamInfo::Unknown;
        QList<Param::Type> types;
        bool supportsDraw = false;
        bool supportsAnchor = false;
        QList<AnchorNode::InterpMode> interpolations;
        bool editable = false;
        ParamInfo::ValueSpec valueSpec;
    };

    struct ParameterCapabilitiesDto {
        DocumentVersion document;
        ClipId clipId;
        QList<ParameterCapabilityDto> parameters;
    };

    enum class SpeakerMixTargetKind {
        Track,
        Clip,
    };

    struct SpeakerMixTargetDto {
        SpeakerMixTargetKind kind = SpeakerMixTargetKind::Track;
        int id = -1;
    };

    struct SpeakerMixSnapshotDto {
        DocumentVersion document;
        SpeakerMixTargetDto target;
        SingerInfo singer;
        SpeakerInfo speaker;
        SpeakerMixModel::SpeakerMixData mix;
        bool inherited = false;
    };

    struct AnchorInsertDto {
        int position = 0;
        int value = 0;
        AnchorNode::InterpMode interpolation = AnchorNode::Hermite;
    };

    struct AnchorMoveDto {
        AnchorId anchorId;
        int position = 0;
        int value = 0;
    };

    class ParameterAutomationFacade final {
    public:
        ParameterAutomationFacade(OperationCatalog &catalog, AutomationDispatcher &dispatcher,
                                  CommandCommitter &committer, DocumentObjectResolver &objects);

        AutomationResult<ParameterSnapshotDto> getParameter(const DocumentId &documentId,
                                                            ClipId clipId, ParamInfo::Name name,
                                                            Param::Type type);
        AutomationResult<ParameterCapabilitiesDto> getCapabilities(const DocumentId &documentId,
                                                                   ClipId clipId);
        AutomationResult<MutationResult> replaceParameter(const CommandContext &context,
                                                          ClipId clipId, ParamInfo::Name name,
                                                          Param::Type type,
                                                          const QList<CurveDraftDto> &curves);
        AutomationResult<MutationResult> drawParameter(const CommandContext &context, ClipId clipId,
                                                       ParamInfo::Name name, Param::Type type,
                                                       int localStart, int step, QList<int> values,
                                                       bool overlay);
        AutomationResult<MutationResult> eraseParameter(const CommandContext &context,
                                                        ClipId clipId, ParamInfo::Name name,
                                                        Param::Type type, int localStart,
                                                        int localEnd);
        AutomationResult<MutationResult> insertAnchor(const CommandContext &context, ClipId clipId,
                                                      ParamInfo::Name name, Param::Type type,
                                                      std::optional<CurveId> curveId, int position,
                                                      int value,
                                                      AnchorNode::InterpMode interpolation);
        AutomationResult<MutationResult> insertAnchors(const CommandContext &context, ClipId clipId,
                                                       ParamInfo::Name name, Param::Type type,
                                                       std::optional<CurveId> curveId,
                                                       const QList<AnchorInsertDto> &anchors);
        AutomationResult<MutationResult> moveAnchor(const CommandContext &context, ClipId clipId,
                                                    ParamInfo::Name name, Param::Type type,
                                                    AnchorId anchorId, int position, int value);
        AutomationResult<MutationResult> moveAnchors(const CommandContext &context, ClipId clipId,
                                                     ParamInfo::Name name, Param::Type type,
                                                     const QList<AnchorMoveDto> &moves);
        AutomationResult<MutationResult> removeAnchor(const CommandContext &context, ClipId clipId,
                                                      ParamInfo::Name name, Param::Type type,
                                                      AnchorId anchorId);
        AutomationResult<MutationResult> removeAnchors(const CommandContext &context, ClipId clipId,
                                                       ParamInfo::Name name, Param::Type type,
                                                       QList<AnchorId> anchorIds);
        AutomationResult<MutationResult>
            setAnchorInterpolation(const CommandContext &context, ClipId clipId,
                                   ParamInfo::Name name, Param::Type type, AnchorId anchorId,
                                   AnchorNode::InterpMode interpolation);
        AutomationResult<MutationResult> setAnchorInterpolations(
            const CommandContext &context, ClipId clipId, ParamInfo::Name name, Param::Type type,
            QList<AnchorId> anchorIds, AnchorNode::InterpMode interpolation);
        AutomationResult<MutationResult> bakeParameter(const CommandContext &context, ClipId clipId,
                                                       ParamInfo::Name name,
                                                       std::optional<int> localStart = std::nullopt,
                                                       std::optional<int> localEnd = std::nullopt);

        AutomationResult<MutationResult>
            replaceClipSpeakerMix(const CommandContext &context, ClipId clipId,
                                  const SpeakerMixModel::SpeakerMixData &data);
        AutomationResult<MutationResult> useTrackVoiceContext(const CommandContext &context,
                                                              ClipId clipId);
        AutomationResult<MutationResult> selectClipSingleSpeaker(const CommandContext &context,
                                                                 ClipId clipId,
                                                                 const SingerInfo &singerInfo,
                                                                 const SpeakerInfo &speakerInfo);
        AutomationResult<MutationResult> enableClipDynamicSpeakerMix(
            const CommandContext &context, ClipId clipId, const SingerInfo &singerInfo,
            const SpeakerInfo &speakerInfo, const SpeakerMixModel::SpeakerMixData &data);
        AutomationResult<MutationResult> enableClipDynamicSpeakerMix(const CommandContext &context,
                                                                     ClipId clipId);
        AutomationResult<MutationResult>
            applyClipSpeakerMix(const CommandContext &context, ClipId clipId,
                                const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                const SpeakerMixModel::SpeakerMixData &data);
        AutomationResult<MutationResult> selectTrackSingleSpeaker(const CommandContext &context,
                                                                  TrackId trackId,
                                                                  const SingerInfo &singerInfo,
                                                                  const SpeakerInfo &speakerInfo);
        AutomationResult<MutationResult>
            applyTrackSpeakerMix(const CommandContext &context, TrackId trackId,
                                 const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                 const SpeakerMixModel::SpeakerMixData &data);
        AutomationResult<MutationResult>
            replaceTrackSpeakerMix(const CommandContext &context, TrackId trackId,
                                   const SpeakerMixModel::SpeakerMixData &data);
        AutomationResult<SpeakerMixSnapshotDto> getSpeakerMix(const DocumentId &documentId,
                                                              SpeakerMixTargetDto target);
        AutomationResult<MutationResult>
            setFixedSpeakerMix(const CommandContext &context, SpeakerMixTargetDto target,
                               const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                               const SpeakerMixModel::SpeakerMixData &data);
        AutomationResult<MutationResult> disableClipDynamicSpeakerMix(const CommandContext &context,
                                                                      ClipId clipId);
        AutomationResult<MutationResult>
            setClipDynamicSpeakerMixBypassed(const CommandContext &context, ClipId clipId,
                                             bool bypassed);
        AutomationResult<MutationResult>
            insertSpeakerMixKeyframe(const CommandContext &context, ClipId clipId, int position,
                                     std::optional<QVector<double>> weights = std::nullopt);
        AutomationResult<MutationResult>
            moveSpeakerMixKeyframes(const CommandContext &context, ClipId clipId,
                                    const QList<QPair<SpeakerMixKeyframeId, int>> &moves);
        AutomationResult<MutationResult>
            setSpeakerMixKeyframeWeights(const CommandContext &context, ClipId clipId,
                                         SpeakerMixKeyframeId keyframeId, QVector<double> weights);
        AutomationResult<MutationResult>
            removeSpeakerMixKeyframes(const CommandContext &context, ClipId clipId,
                                      QList<SpeakerMixKeyframeId> keyframeIds);
        AutomationResult<MutationResult> clearTrackVoice(const CommandContext &context,
                                                         TrackId trackId);
        AutomationResult<MutationResult> clearClipVoice(const CommandContext &context,
                                                        ClipId clipId);

    private:
        enum class ClipVoiceAction {
            SelectSingle,
            EnableDynamic,
            ApplyPreset,
        };

        AutomationResult<MutationResult>
            setClipVoiceContext(const OperationId &operationId, ClipVoiceAction action,
                                const CommandContext &context, ClipId clipId,
                                const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                const SpeakerMixModel::SpeakerMixData &data);
        using CurveMutation = std::function<AutomationResult<bool>(QList<CurveDraftDto> &curves)>;
        AutomationResult<MutationResult> mutateParameter(const OperationId &operationId,
                                                         const CommandContext &context,
                                                         const QByteArray &operationTag,
                                                         const QByteArray &requestFingerprint,
                                                         ClipId clipId, ParamInfo::Name name,
                                                         Param::Type type, CurveMutation mutation);
        using SpeakerMixMutation =
            std::function<AutomationResult<bool>(SpeakerMixModel::SpeakerMixData &data)>;
        AutomationResult<MutationResult> mutateClipSpeakerMix(const OperationId &operationId,
                                                              const CommandContext &context,
                                                              ClipId clipId,
                                                              const QByteArray &requestFingerprint,
                                                              SpeakerMixMutation mutation);
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        CommandCommitter &m_committer;
        DocumentObjectResolver &m_objects;
    };

} // namespace Automation

#endif // PARAMETERAUTOMATIONFACADE_H
