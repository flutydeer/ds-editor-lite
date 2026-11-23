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
        QString inputLanguage;
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

    struct TrackPropertiesPatchDto {
        TrackId id;
        std::optional<QString> name;
        std::optional<double> gain;
        std::optional<double> pan;
        std::optional<bool> mute;
        std::optional<bool> solo;
    };

    struct ClipPropertiesPatchDto {
        ClipId id;
        std::optional<QString> name;
        std::optional<int> start;
        std::optional<int> length;
        std::optional<int> clipStart;
        std::optional<int> clipLen;
        std::optional<double> gain;
        std::optional<bool> mute;
        std::optional<TrackId> targetTrackId;
    };

    struct ClipMoveDto {
        ClipId id;
        TrackId targetTrackId;
        int start = 0;
    };

    // A clip's new visible start, for the moves whose destination track the
    // same command creates. `start` follows the ClipMoveDto convention and is
    // the visible start (properties.start + properties.clipStart).
    struct ClipStartDto {
        ClipId id;
        int start = 0;
    };

    struct ClipDuplicateDestinationDto {
        std::optional<TrackId> targetTrackId;
        int targetStart = 0;
    };

    class ProjectAutomationFacade final {
    public:
        ProjectAutomationFacade(AutomationDispatcher &dispatcher, CommandCommitter &committer,
                                DocumentObjectResolver &objects);

        AutomationResult<ProjectSnapshotDto> getProject(const DocumentId &documentId);

        AutomationResult<MutationResult> insertTrack(const CommandContext &context, qsizetype index,
                                                     const TrackDraftDto &track);
        AutomationResult<MutationResult> insertTracks(const CommandContext &context,
                                                      qsizetype index,
                                                      const QList<TrackDraftDto> &tracks);
        AutomationResult<MutationResult> removeTracks(const CommandContext &context,
                                                      QList<TrackId> trackIds);
        AutomationResult<MutationResult> moveTrack(const CommandContext &context, TrackId trackId,
                                                   qsizetype targetIndex);
        AutomationResult<MutationResult> moveTracks(const CommandContext &context,
                                                    QList<TrackId> trackIds, qsizetype targetIndex);
        AutomationResult<MutationResult> setTrackProperties(const CommandContext &context,
                                                            const TrackPropertiesDto &properties);
        AutomationResult<MutationResult> patchTrackProperties(const CommandContext &context,
                                                              const TrackPropertiesPatchDto &patch);
        AutomationResult<MutationResult> renameTrack(const CommandContext &context, TrackId trackId,
                                                     const QString &name);
        AutomationResult<MutationResult> setTrackGain(const CommandContext &context,
                                                      TrackId trackId, double gain);
        AutomationResult<MutationResult> setTrackPan(const CommandContext &context, TrackId trackId,
                                                     double pan);
        AutomationResult<MutationResult> setTrackMute(const CommandContext &context,
                                                      TrackId trackId, bool mute);
        AutomationResult<MutationResult> setTrackSolo(const CommandContext &context,
                                                      TrackId trackId, bool solo);
        AutomationResult<MutationResult> setTrackColor(const CommandContext &context,
                                                       TrackId trackId, int colorIndex);

        AutomationResult<MutationResult> insertClips(const CommandContext &context,
                                                     const QList<ClipInsertDto> &clips);
        AutomationResult<MutationResult> commitBatchImport(const CommandContext &context,
                                                           const BatchImportDraftDto &batch);
        AutomationResult<MutationResult> removeClips(const CommandContext &context,
                                                     QList<ClipId> clipIds);
        AutomationResult<MutationResult>
            duplicateClips(const CommandContext &context, QList<ClipId> clipIds,
                           const ClipDuplicateDestinationDto &destination);
        AutomationResult<MutationResult> moveClips(const CommandContext &context,
                                                   const QList<ClipMoveDto> &moves);
        // One history entry: appends `newTrack` at the project tail and applies
        // every move in the same command, so a gesture that spills one row past
        // the last track commits atomically. `ontoExistingTracks` keeps the
        // clips that still land on real tracks in that same entry.
        AutomationResult<MutationResult>
            moveClipsWithNewTrack(const CommandContext &context, const TrackDraftDto &newTrack,
                                  const QList<ClipStartDto> &intoNewTrack,
                                  const QList<ClipMoveDto> &ontoExistingTracks);
        AutomationResult<MutationResult> resizeClipLeft(const CommandContext &context,
                                                        ClipId clipId, int start);
        AutomationResult<MutationResult> resizeClipRight(const CommandContext &context,
                                                         ClipId clipId, int end);
        AutomationResult<MutationResult> splitClip(const CommandContext &context, ClipId clipId,
                                                   int splitTick);
        AutomationResult<MutationResult>
            setClipProperties(const CommandContext &context, const ClipPropertiesDto &properties,
                              std::optional<TrackId> targetTrackId = std::nullopt);
        // Applies every entry inside one command, so a multi-clip gesture edits all of them in a
        // single history entry. Track assignment is not part of this batch: use moveClips() when a
        // clip also changes track.
        AutomationResult<MutationResult>
            setClipsProperties(const CommandContext &context,
                               const QList<ClipPropertiesDto> &properties);
        AutomationResult<MutationResult> patchClipProperties(const CommandContext &context,
                                                             const ClipPropertiesPatchDto &patch);
        AutomationResult<MutationResult> renameClip(const CommandContext &context, ClipId clipId,
                                                    const QString &name);
        AutomationResult<MutationResult> setClipGain(const CommandContext &context, ClipId clipId,
                                                     double gain);
        AutomationResult<MutationResult> setClipMute(const CommandContext &context, ClipId clipId,
                                                     bool mute);
        AutomationResult<MutationResult> relocateAudioClip(const CommandContext &context,
                                                           ClipId clipId, const QString &path,
                                                           const AudioPathInfo &pathInfo,
                                                           const QJsonObject &formatData);
        AutomationResult<MutationResult> confirmAudioClipPath(const CommandContext &context,
                                                              ClipId clipId);
        AutomationResult<MutationResult> confirmAudioClipPath(const CommandContext &context,
                                                              ClipId clipId, const QString &path,
                                                              const AudioPathInfo &pathInfo,
                                                              const QJsonObject &formatData);
        AutomationResult<MutationResult>
            applyAudioDecodeCache(const CommandContext &context, ClipId clipId,
                                  const AudioAssetSnapshotDto &expectedAsset,
                                  const AudioInfoModel &audioInfo);
        AutomationResult<MutationResult>
            setAudioClipPathStatus(const CommandContext &context, ClipId clipId,
                                   const AudioAssetSnapshotDto &expectedAsset,
                                   AudioClip::PathStatus status);
        AutomationResult<MutationResult>
            applyResolvedAudioPath(const CommandContext &context, ClipId clipId,
                                   const AudioAssetSnapshotDto &expectedAsset,
                                   const QString &resolvedPath, AudioClip::PathStatus status);
        AutomationResult<MutationResult>
            setAudioClipHash(const CommandContext &context, ClipId clipId,
                             const AudioAssetSnapshotDto &expectedAsset, const QString &sha512);
        AutomationResult<MutationResult>
            setSingingClipInputLanguage(const CommandContext &context, ClipId clipId,
                                          const QString &language);

    private:
        AutomationResult<MutationResult> patchTrackProperties(const OperationId &operationId,
                                                              const CommandContext &context,
                                                              const TrackPropertiesPatchDto &patch);
        AutomationResult<MutationResult> patchClipProperties(const OperationId &operationId,
                                                             const CommandContext &context,
                                                             const ClipPropertiesPatchDto &patch);
        AutomationResult<MutationResult>
            setClipProperties(const OperationId &operationId, const CommandContext &context,
                              const ClipPropertiesDto &properties,
                              std::optional<TrackId> targetTrackId = std::nullopt);

        AutomationDispatcher &m_dispatcher;
        CommandCommitter &m_committer;
        DocumentObjectResolver &m_objects;
    };

} // namespace Automation

#endif // PROJECTAUTOMATIONFACADE_H
