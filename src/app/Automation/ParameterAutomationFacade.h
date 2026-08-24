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

    class ParameterAutomationFacade final {
    public:
        ParameterAutomationFacade(OperationCatalog &catalog,
                                  AutomationDispatcher &dispatcher,
                                  CommandCommitter &committer,
                                  DocumentObjectResolver &objects);

        AutomationResult<ParameterSnapshotDto> getParameter(const DocumentId &documentId,
                                                            ClipId clipId,
                                                            ParamInfo::Name name,
                                                            Param::Type type);
        AutomationResult<ParameterCapabilitiesDto> getCapabilities(const DocumentId &documentId,
                                                                    ClipId clipId);
        AutomationResult<MutationResult> replaceParameter(
            const CommandContext &context,
            ClipId clipId,
            ParamInfo::Name name,
            Param::Type type,
            const QList<CurveDraftDto> &curves);
        AutomationResult<MutationResult> drawParameter(
            const CommandContext &context, ClipId clipId, ParamInfo::Name name, Param::Type type,
            int localStart, int step, QList<int> values, bool overlay);
        AutomationResult<MutationResult> eraseParameter(const CommandContext &context,
                                                       ClipId clipId, ParamInfo::Name name,
                                                       Param::Type type, int localStart,
                                                       int localEnd);
        AutomationResult<MutationResult> insertAnchor(
            const CommandContext &context, ClipId clipId, ParamInfo::Name name, Param::Type type,
            std::optional<CurveId> curveId, int position, int value,
            AnchorNode::InterpMode interpolation);
        AutomationResult<MutationResult> moveAnchor(const CommandContext &context, ClipId clipId,
                                                   ParamInfo::Name name, Param::Type type,
                                                   AnchorId anchorId, int position, int value);
        AutomationResult<MutationResult> removeAnchor(const CommandContext &context, ClipId clipId,
                                                     ParamInfo::Name name, Param::Type type,
                                                     AnchorId anchorId);
        AutomationResult<MutationResult> setAnchorInterpolation(
            const CommandContext &context, ClipId clipId, ParamInfo::Name name, Param::Type type,
            AnchorId anchorId, AnchorNode::InterpMode interpolation);
        AutomationResult<MutationResult> bakeParameter(const CommandContext &context, ClipId clipId,
                                                      ParamInfo::Name name,
                                                      std::optional<int> localStart = std::nullopt,
                                                      std::optional<int> localEnd = std::nullopt);

        AutomationResult<MutationResult> replaceClipSpeakerMix(
            const CommandContext &context,
            ClipId clipId,
            const SpeakerMixModel::SpeakerMixData &data);
        AutomationResult<MutationResult> useTrackVoiceContext(const CommandContext &context,
                                                              ClipId clipId);
        AutomationResult<MutationResult> selectClipSingleSpeaker(
            const CommandContext &context,
            ClipId clipId,
            const SingerInfo &singerInfo,
            const SpeakerInfo &speakerInfo);
        AutomationResult<MutationResult> enableClipDynamicSpeakerMix(
            const CommandContext &context,
            ClipId clipId,
            const SingerInfo &singerInfo,
            const SpeakerInfo &speakerInfo,
            const SpeakerMixModel::SpeakerMixData &data);
        AutomationResult<MutationResult> applyClipSpeakerMix(
            const CommandContext &context,
            ClipId clipId,
            const SingerInfo &singerInfo,
            const SpeakerInfo &speakerInfo,
            const SpeakerMixModel::SpeakerMixData &data);
        AutomationResult<MutationResult> selectTrackSingleSpeaker(
            const CommandContext &context,
            TrackId trackId,
            const SingerInfo &singerInfo,
            const SpeakerInfo &speakerInfo);
        AutomationResult<MutationResult> applyTrackSpeakerMix(
            const CommandContext &context,
            TrackId trackId,
            const SingerInfo &singerInfo,
            const SpeakerInfo &speakerInfo,
            const SpeakerMixModel::SpeakerMixData &data);
        AutomationResult<MutationResult> replaceTrackSpeakerMix(
            const CommandContext &context,
            TrackId trackId,
            const SpeakerMixModel::SpeakerMixData &data);

    private:
        enum class ClipVoiceAction {
            SelectSingle,
            EnableDynamic,
            ApplyPreset,
        };

        AutomationResult<MutationResult> setClipVoiceContext(
            const OperationId &operationId,
            ClipVoiceAction action,
            const CommandContext &context,
            ClipId clipId,
            const SingerInfo &singerInfo,
            const SpeakerInfo &speakerInfo,
            const SpeakerMixModel::SpeakerMixData &data);
        using CurveMutation =
            std::function<AutomationResult<bool>(QList<CurveDraftDto> &curves)>;
        AutomationResult<MutationResult> mutateParameter(
            const CommandContext &context, const QByteArray &operationTag,
            const QByteArray &requestFingerprint, ClipId clipId, ParamInfo::Name name,
            Param::Type type, CurveMutation mutation);
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        CommandCommitter &m_committer;
        DocumentObjectResolver &m_objects;
    };

} // namespace Automation

#endif // PARAMETERAUTOMATIONFACADE_H
