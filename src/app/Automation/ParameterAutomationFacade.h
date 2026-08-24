#ifndef PARAMETERAUTOMATIONFACADE_H
#define PARAMETERAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "CommandCommitter.h"
#include "DocumentObjectResolver.h"
#include "ProjectAutomationDtos.h"

#include <lite/ProjectModel/AppModel/EffectiveVoiceContext.h>

namespace Automation {

    struct ParameterSnapshotDto {
        DocumentVersion document;
        ClipId clipId;
        ParamInfo::Name name = ParamInfo::Unknown;
        Param::Type type = Param::Unknown;
        QList<CurveDraftDto> curves;
    };

    class ParameterAutomationFacade final {
    public:
        ParameterAutomationFacade(OperationCatalog &catalog, AutomationDispatcher &dispatcher,
                                  CommandCommitter &committer, DocumentObjectResolver &objects);

        AutomationResult<ParameterSnapshotDto> getParameter(const DocumentId &documentId,
                                                            ClipId clipId, ParamInfo::Name name,
                                                            Param::Type type);
        AutomationResult<MutationResult> replaceParameter(const CommandContext &context,
                                                          ClipId clipId, ParamInfo::Name name,
                                                          Param::Type type,
                                                          const QList<CurveDraftDto> &curves);

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
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        CommandCommitter &m_committer;
        DocumentObjectResolver &m_objects;
    };

} // namespace Automation

#endif // PARAMETERAUTOMATIONFACADE_H
