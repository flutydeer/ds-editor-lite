#ifndef PROJECTAUTOMATIONFACADE_H
#define PROJECTAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "CommandCommitter.h"
#include "DocumentObjectResolver.h"
#include "ProjectAutomationDtos.h"

#include <optional>

namespace Automation {

    struct ClipSnapshotDto {
        ClipId id;
        TrackId trackId;
        ClipDraftDto data;
    };

    struct TrackSnapshotDto {
        TrackId id;
        TrackDraftDto data;
        QList<ClipSnapshotDto> clips;
    };

    struct ProjectSnapshotDto {
        DocumentVersion document;
        QList<TrackSnapshotDto> tracks;
    };

    class ProjectAutomationFacade final {
    public:
        ProjectAutomationFacade(OperationCatalog &catalog,
                                AutomationDispatcher &dispatcher,
                                CommandCommitter &committer,
                                DocumentObjectResolver &objects);

        AutomationResult<ProjectSnapshotDto> getProject(const DocumentId &documentId);

        AutomationResult<MutationResult> insertTrack(const CommandContext &context,
                                                     qsizetype index,
                                                     const TrackDraftDto &track);
        AutomationResult<MutationResult> removeTracks(const CommandContext &context,
                                                      QList<TrackId> trackIds);
        AutomationResult<MutationResult> moveTrack(const CommandContext &context,
                                                   TrackId trackId,
                                                   qsizetype targetIndex);
        AutomationResult<MutationResult> setTrackProperties(
            const CommandContext &context, const TrackPropertiesDto &properties);
        AutomationResult<MutationResult> setTrackColor(const CommandContext &context,
                                                       TrackId trackId,
                                                       int colorIndex);
        AutomationResult<MutationResult> setTrackDefaultLanguage(const CommandContext &context,
                                                                 TrackId trackId,
                                                                 const QString &language);

        AutomationResult<MutationResult> insertClips(const CommandContext &context,
                                                     const QList<ClipInsertDto> &clips);
        AutomationResult<MutationResult> commitBatchImport(
            const CommandContext &context, const BatchImportDraftDto &batch);
        AutomationResult<MutationResult> removeClips(const CommandContext &context,
                                                     QList<ClipId> clipIds);
        AutomationResult<MutationResult> setClipProperties(
            const CommandContext &context,
            const ClipPropertiesDto &properties,
            std::optional<TrackId> targetTrackId = std::nullopt);
        AutomationResult<MutationResult> relocateAudioClip(
            const CommandContext &context,
            ClipId clipId,
            const QString &path,
            const AudioPathInfo &pathInfo,
            const QJsonObject &formatData);
        AutomationResult<MutationResult> confirmAudioClipPath(const CommandContext &context,
                                                              ClipId clipId);
        AutomationResult<MutationResult> applyAudioDecodeCache(
            const CommandContext &context, ClipId clipId, const QString &expectedPath,
            const AudioInfoModel &audioInfo);
        AutomationResult<MutationResult> setAudioClipPathStatus(
            const CommandContext &context, ClipId clipId, const QString &expectedPath,
            AudioClip::PathStatus status);
        AutomationResult<MutationResult> applyResolvedAudioPath(
            const CommandContext &context, ClipId clipId, const QString &expectedPath,
            const QString &resolvedPath, AudioClip::PathStatus status);
        AutomationResult<MutationResult> setAudioClipHash(
            const CommandContext &context, ClipId clipId, const QString &expectedPath,
            const QString &sha512);
        AutomationResult<MutationResult> setSingingClipDefaultLanguage(
            const CommandContext &context, ClipId clipId, const QString &language);

    private:
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        CommandCommitter &m_committer;
        DocumentObjectResolver &m_objects;
    };

} // namespace Automation

#endif // PROJECTAUTOMATIONFACADE_H
