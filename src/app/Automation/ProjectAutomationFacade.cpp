#include "ProjectAutomationFacade.h"
#include "OperationIds.h"

#include "Controller/Actions/AppModel/Clip/ClipActions.h"
#include "Controller/Actions/AppModel/Import/BatchImportActions.h"
#include "Controller/Actions/AppModel/Track/TrackActions.h"
#include "Global/AppGlobal.h"
#include "UI/Views/TrackEditor/AudioClipDragState.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/AutomationWire/PublicConstants.h>
#include <lite/ProjectModel/Utils/DiffscopeAudioWorkspace.h>
#include <lite/ProjectModel/Utils/ClipResizeUtils.h>

#include <QDataStream>
#include <QFileInfo>
#include <QIODevice>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <memory>
#include <limits>
#include <vector>

namespace Automation {
    namespace {
        constexpr auto kAudioFormatDataKey = "diffscope.audio.formatData";

        template <typename Target>
        class SetDefaultLanguageAction final : public IAction {
        public:
            SetDefaultLanguageAction(Target *target, QString previous, QString current)
                : m_target(target), m_previous(std::move(previous)), m_current(std::move(current)) {
            }

            void execute() override {
                m_target->setDefaultLanguage(m_current);
            }

            void undo() override {
                m_target->setDefaultLanguage(m_previous);
            }

        private:
            Target *m_target = nullptr;
            QString m_previous;
            QString m_current;
        };

        class DefaultLanguageActions final : public ActionSequence {
        public:
            template <typename Target>
            void setDefaultLanguage(Target *target, const QString &language) {
                addAction(new SetDefaultLanguageAction<Target>(target, target->defaultLanguage(),
                                                               language));
            }
        };

        QByteArray withIntegerPrefix(const qint64 value, const QByteArray &payload) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << value << payload;
            return result;
        }

        QByteArray integerListFingerprint(const QList<int> &values) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << values;
            return result;
        }

        QByteArray trackInsertFingerprint(const qsizetype index,
                                          const QList<TrackDraftDto> &tracks) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << index << tracks.size();
            for (const auto &track : tracks)
                stream << fingerprint(track);
            return result;
        }

        QByteArray clipMovesFingerprint(const QList<ClipMoveDto> &moves) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            for (const auto &move : moves)
                stream << move.id.value() << move.targetTrackId.value() << move.start;
            return result;
        }

        QByteArray clipDuplicateFingerprint(const QList<ClipId> &ids,
                                            const ClipDuplicateDestinationDto &destination) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            for (const auto id : ids)
                stream << id.value();
            stream << destination.targetTrackId.has_value()
                   << destination.targetTrackId.value_or(TrackId()).value()
                   << destination.targetStart;
            return result;
        }

        QByteArray trackPatchFingerprint(const TrackPropertiesPatchDto &patch) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << patch.id.value() << patch.name.has_value();
            if (patch.name)
                stream << *patch.name;
            stream << patch.gain.has_value();
            if (patch.gain)
                stream << *patch.gain;
            stream << patch.pan.has_value();
            if (patch.pan)
                stream << *patch.pan;
            stream << patch.mute.has_value();
            if (patch.mute)
                stream << *patch.mute;
            stream << patch.solo.has_value();
            if (patch.solo)
                stream << *patch.solo;
            return result;
        }

        QByteArray clipPatchFingerprint(const ClipPropertiesPatchDto &patch) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << patch.id.value() << patch.name.has_value();
            if (patch.name)
                stream << *patch.name;
            stream << patch.start.has_value();
            if (patch.start)
                stream << *patch.start;
            stream << patch.length.has_value();
            if (patch.length)
                stream << *patch.length;
            stream << patch.clipStart.has_value();
            if (patch.clipStart)
                stream << *patch.clipStart;
            stream << patch.clipLen.has_value();
            if (patch.clipLen)
                stream << *patch.clipLen;
            stream << patch.gain.has_value();
            if (patch.gain)
                stream << *patch.gain;
            stream << patch.mute.has_value();
            if (patch.mute)
                stream << *patch.mute;
            stream << patch.targetTrackId.has_value();
            if (patch.targetTrackId)
                stream << patch.targetTrackId->value();
            return result;
        }

        QByteArray relocateFingerprint(const ClipId clipId, const QString &path,
                                       const AudioPathInfo &pathInfo,
                                       const QJsonObject &formatData) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << clipId.value() << path << pathInfo.relativeDir << pathInfo.sha512
                   << formatData;
            return result;
        }

        QByteArray audioCacheFingerprint(const ClipId clipId, const AudioAssetSnapshotDto &asset,
                                         const AudioInfoModel &audioInfo) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << clipId.value() << asset.path << asset.formatData << asset.sourceGeneration;
            stream << audioInfo.sampleRate << audioInfo.channels << audioInfo.frames
                   << audioInfo.chunkSize << audioInfo.mipmapScale << audioInfo.peakCache.size();
            for (const auto &[minimum, maximum] : audioInfo.peakCache)
                stream << minimum << maximum;
            stream << audioInfo.peakCacheMipmap.size();
            for (const auto &[minimum, maximum] : audioInfo.peakCacheMipmap)
                stream << minimum << maximum;
            return result;
        }

        bool audioInfoEqual(const AudioInfoModel &left, const AudioInfoModel &right) {
            return left.chunkSize == right.chunkSize && left.mipmapScale == right.mipmapScale &&
                   left.sampleRate == right.sampleRate && left.channels == right.channels &&
                   left.frames == right.frames && left.peakCache == right.peakCache &&
                   left.peakCacheMipmap == right.peakCacheMipmap;
        }

        QByteArray audioPathFingerprint(const ClipId clipId, const AudioAssetSnapshotDto &asset,
                                        const QString &value, const int state = -1) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << clipId.value() << asset.path << asset.sourceGeneration << value << state;
            return result;
        }

        QByteArray audioResolveFingerprint(const ClipId clipId, const AudioAssetSnapshotDto &asset,
                                           const QString &resolvedPath, const int state) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << clipId.value() << asset.path << asset.pathInfo.relativeDir
                   << asset.pathInfo.sha512 << asset.sourceGeneration << resolvedPath << state;
            return result;
        }

        bool decodeSourceMatches(const AudioClip &clip, const AudioAssetSnapshotDto &asset) {
            return clip.sourceGeneration() == asset.sourceGeneration && clip.path() == asset.path &&
                   clip.workspace().value(kAudioFormatDataKey) == asset.formatData;
        }

        bool resolveSourceMatches(const AudioClip &clip, const AudioAssetSnapshotDto &asset) {
            const auto pathInfo = clip.pathInfo();
            return clip.sourceGeneration() == asset.sourceGeneration && clip.path() == asset.path &&
                   pathInfo.relativeDir == asset.pathInfo.relativeDir &&
                   pathInfo.sha512 == asset.pathInfo.sha512;
        }

        MutationResult cacheMutationResult(DocumentSession &session, const bool changed,
                                           const bool validateOnly) {
            MutationResult result;
            result.previous = session.version();
            result.current = result.previous;
            result.changed = changed;
            result.validatedOnly = validateOnly;
            return result;
        }

        AutomationError staleAudioAsset(const ClipId clipId) {
            auto error = AutomationError::invalidArgument(
                QStringLiteral("expected_asset"), QStringLiteral("Audio clip source changed"));
            error.object = ObjectRef{ObjectKind::Clip, clipId.value()};
            return error;
        }

        AutomationError invalidResolvedAudioPath(const ClipId clipId, QString message) {
            auto error = AutomationError::invalidArgument(QStringLiteral("resolved_path"),
                                                          std::move(message));
            error.object = ObjectRef{ObjectKind::Clip, clipId.value()};
            return error;
        }

        bool validClipProperties(const ClipPropertiesDto &properties) {
            return static_cast<qint64>(properties.start) + properties.clipStart >= 0 &&
                   properties.length >= 0 && properties.clipStart >= 0 && properties.clipLen >= 0 &&
                   static_cast<qint64>(properties.clipStart) + properties.clipLen <=
                       properties.length &&
                   std::isfinite(properties.gain) && std::isfinite(properties.trimStartMs) &&
                   std::isfinite(properties.playLengthMs) &&
                   std::isfinite(properties.materialLengthMs);
        }

        bool trackPropertiesEqual(const Track::TrackProperties &left,
                                  const TrackPropertiesDto &right) {
            return left.id == right.id.value() && left.name == right.name &&
                   left.gain == right.gain && left.pan == right.pan && left.mute == right.mute &&
                   left.solo == right.solo;
        }

        bool clipPropertiesEqual(const Clip::ClipCommonProperties &left,
                                 const ClipPropertiesDto &right) {
            return left.id == right.id.value() && left.name == right.name &&
                   left.start == right.start && left.length == right.length &&
                   left.clipStart == right.clipStart && left.clipLen == right.clipLen &&
                   left.gain == right.gain && left.mute == right.mute &&
                   left.trimStartMs == right.trimStartMs &&
                   left.playLengthMs == right.playLengthMs &&
                   left.materialLengthMs == right.materialLengthMs;
        }

        Clip::ClipCommonProperties toModelProperties(const ClipPropertiesDto &properties) {
            Clip::ClipCommonProperties result;
            result.id = properties.id.value();
            result.name = properties.name;
            result.start = properties.start;
            result.length = properties.length;
            result.clipStart = properties.clipStart;
            result.clipLen = properties.clipLen;
            result.gain = properties.gain;
            result.mute = properties.mute;
            result.trimStartMs = properties.trimStartMs;
            result.playLengthMs = properties.playLengthMs;
            result.materialLengthMs = properties.materialLengthMs;
            return result;
        }

        ClipPropertiesDto toDto(const Clip::ClipCommonProperties &properties) {
            return {ClipId(properties.id),
                    properties.name,
                    properties.start,
                    properties.length,
                    properties.clipStart,
                    properties.clipLen,
                    properties.gain,
                    properties.mute,
                    properties.trimStartMs,
                    properties.playLengthMs,
                    properties.materialLengthMs};
        }

        AutomationError invalidModelError() {
            AutomationError error;
            error.code = AutomationErrorCode::InternalError;
            error.message = QStringLiteral("Document session has no AppModel");
            return error;
        }
    }

    ProjectAutomationFacade::ProjectAutomationFacade(OperationCatalog &catalog,
                                                     AutomationDispatcher &dispatcher,
                                                     CommandCommitter &committer,
                                                     DocumentObjectResolver &objects)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_committer(committer), m_objects(objects) {
        registerOperations();
    }

    AutomationResult<ProjectSnapshotDto>
        ProjectAutomationFacade::getProject(const DocumentId &documentId) {
        return m_dispatcher.dispatchDocumentQuery<ProjectSnapshotDto>(
            OperationIds::project::get, documentId, [](DocumentSession &session) {
                auto *model = session.model();
                if (!model)
                    return AutomationResult<ProjectSnapshotDto>(invalidModelError());
                ProjectSnapshotDto result;
                result.document = session.version();
                for (const auto *track : model->tracks()) {
                    TrackSnapshotDto trackSnapshot;
                    trackSnapshot.id = TrackId(track->id());
                    trackSnapshot.data = trackDraftDto(*track);
                    trackSnapshot.data.clips.clear();
                    for (const auto *clip : track->clips()) {
                        trackSnapshot.clips.append(
                            {ClipId(clip->id()), trackSnapshot.id, clipDraftDto(*clip)});
                    }
                    result.tracks.append(std::move(trackSnapshot));
                }
                return AutomationResult<ProjectSnapshotDto>(std::move(result));
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::insertTrack(const CommandContext &context, const qsizetype index,
                                             const TrackDraftDto &trackDraft) {
        return insertTracks(context, index, {trackDraft});
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::insertTracks(const CommandContext &context, const qsizetype index,
                                              const QList<TrackDraftDto> &trackDrafts) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::tracks::insert, context, trackInsertFingerprint(index, trackDrafts),
            [this, index, trackDrafts](DocumentSession &session, const bool validateOnly) {
                auto *model = session.model();
                if (!model)
                    return AutomationResult<MutationResult>(invalidModelError());
                if (index < 0 || index > model->tracks().size()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("index"), QStringLiteral("Track index is out of range")));
                }
                for (const auto &trackDraft : trackDrafts) {
                    auto validation = validate(trackDraft);
                    if (!validation)
                        return AutomationResult<MutationResult>(validation.getError());
                }
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, !trackDrafts.isEmpty()));
                if (trackDrafts.isEmpty())
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                QList<CreatedObjectRef> createdObjects;
                QList<ObjectRef> affected;
                auto actions = std::make_unique<TrackActions>();
                for (qsizetype offset = 0; offset < trackDrafts.size(); ++offset) {
                    auto track =
                        buildTrack(trackDrafts.at(offset), model->timeline(), &createdObjects);
                    affected.append({ObjectKind::Track, track->id()});
                    actions->insertTrack(track.release(), index + offset, model);
                }
                return m_committer.commit(session, std::move(actions), affected,
                                          std::move(createdObjects));
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::removeTracks(const CommandContext &context,
                                              QList<TrackId> trackIds) {
        std::sort(trackIds.begin(), trackIds.end(), [](const TrackId left, const TrackId right) {
            return left.value() < right.value();
        });
        const auto hasDuplicate =
            std::adjacent_find(trackIds.cbegin(), trackIds.cend()) != trackIds.cend();
        QList<int> rawIds;
        rawIds.reserve(trackIds.size());
        for (const auto id : trackIds)
            rawIds.append(id.value());
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::tracks::remove, context, integerListFingerprint(rawIds),
            [this, trackIds = std::move(trackIds), hasDuplicate](DocumentSession &session,
                                                                 const bool validateOnly) {
                if (hasDuplicate) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("track_ids"), QStringLiteral("Track IDs must be unique")));
                }
                QList<Track *> tracks;
                QList<ObjectRef> affected;
                for (const auto id : trackIds) {
                    auto resolved = m_objects.track(session, id);
                    if (!resolved)
                        return AutomationResult<MutationResult>(resolved.getError());
                    tracks.append(resolved.get());
                    affected.append({ObjectKind::Track, id.value()});
                }
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, !tracks.isEmpty(), affected));
                if (tracks.isEmpty())
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<TrackActions>();
                actions->removeTracks(tracks, session.model());
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::moveTrack(const CommandContext &context, const TrackId trackId,
                                           const qsizetype targetIndex) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::tracks::move, context,
            withIntegerPrefix(trackId.value(), QByteArray::number(targetIndex)),
            [this, trackId, targetIndex](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.track(session, trackId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *model = session.model();
                const auto currentIndex = model->tracks().indexOf(resolved.get());
                if (targetIndex < 0 || targetIndex > model->tracks().size()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("target_index"),
                        QStringLiteral("Target track index is out of range")));
                }
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Track, trackId.value()}
                };
                const bool changed = targetIndex != currentIndex && targetIndex != currentIndex + 1;
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<TrackActions>();
                actions->moveTrack(currentIndex, targetIndex, model);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::moveTracks(const CommandContext &context, QList<TrackId> trackIds,
                                            const qsizetype targetIndex) {
        QList<int> rawIds;
        rawIds.reserve(trackIds.size() + 1);
        for (const auto id : trackIds)
            rawIds.append(id.value());
        rawIds.append(static_cast<int>(targetIndex));
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::tracks::move, context, integerListFingerprint(rawIds),
            [this, trackIds = std::move(trackIds), targetIndex](DocumentSession &session,
                                                                const bool validateOnly) {
                auto *model = session.model();
                if (targetIndex < 0 || targetIndex > model->tracks().size()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("target_index"),
                        QStringLiteral("Target track index is out of range")));
                }
                QSet<int> selected;
                QList<Track *> moving;
                for (const auto id : trackIds) {
                    if (selected.contains(id.value())) {
                        return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                            QStringLiteral("track_ids"),
                            QStringLiteral("Track IDs must be unique")));
                    }
                    auto track = m_objects.track(session, id);
                    if (!track)
                        return AutomationResult<MutationResult>(track.getError());
                    selected.insert(id.value());
                    moving.append(track.get());
                }
                std::sort(moving.begin(), moving.end(),
                          [model](const Track *left, const Track *right) {
                              return model->tracks().indexOf(const_cast<Track *>(left)) <
                                     model->tracks().indexOf(const_cast<Track *>(right));
                          });
                QList<Track *> desired = model->tracks();
                qsizetype adjustedTarget = targetIndex;
                for (auto *track : moving) {
                    const auto index = desired.indexOf(track);
                    if (index >= 0 && index < adjustedTarget)
                        --adjustedTarget;
                    desired.removeOne(track);
                }
                adjustedTarget = qBound<qsizetype>(0, adjustedTarget, desired.size());
                for (qsizetype index = 0; index < moving.size(); ++index)
                    desired.insert(adjustedTarget + index, moving.at(index));
                const bool changed = desired != model->tracks();
                QList<ObjectRef> affected;
                for (const auto id : trackIds)
                    affected.append({ObjectKind::Track, id.value()});
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                auto simulated = model->tracks();
                auto actions = std::make_unique<TrackActions>();
                for (qsizetype finalIndex = 0; finalIndex < desired.size(); ++finalIndex) {
                    auto *track = desired.at(finalIndex);
                    const auto currentIndex = simulated.indexOf(track);
                    if (currentIndex == finalIndex)
                        continue;
                    const auto insertionIndex =
                        finalIndex > currentIndex ? finalIndex + 1 : finalIndex;
                    actions->moveTrack(currentIndex, insertionIndex, model);
                    simulated.move(currentIndex, finalIndex);
                }
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::setTrackProperties(const CommandContext &context,
                                                    const TrackPropertiesDto &properties) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::tracks::set_properties, context, fingerprint(properties),
            [this, properties](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.track(session, properties.id);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                if (!std::isfinite(properties.gain) || !std::isfinite(properties.pan)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("properties.control"),
                        QStringLiteral("Track control is invalid")));
                }
                auto *track = resolved.get();
                const Track::TrackProperties oldProperties(*track);
                const bool changed = !trackPropertiesEqual(oldProperties, properties);
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Track, properties.id.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                auto newProperties = oldProperties;
                newProperties.name = properties.name;
                newProperties.gain = properties.gain;
                newProperties.pan = properties.pan;
                newProperties.mute = properties.mute;
                newProperties.solo = properties.solo;
                auto actions = std::make_unique<TrackActions>();
                actions->editTrackProperties(oldProperties, newProperties, track);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::patchTrackProperties(const CommandContext &context,
                                                      const TrackPropertiesPatchDto &patch) {
        return patchTrackProperties(OperationIds::tracks::set_properties, context, patch);
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::renameTrack(const CommandContext &context, const TrackId trackId,
                                             const QString &name) {
        return patchTrackProperties(OperationIds::tracks::rename, context,
                                    {.id = trackId, .name = name});
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::setTrackGain(const CommandContext &context, const TrackId trackId,
                                              const double gain) {
        return patchTrackProperties(OperationIds::tracks::set_gain, context,
                                    {.id = trackId, .gain = gain});
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::setTrackPan(const CommandContext &context, const TrackId trackId,
                                             const double pan) {
        return patchTrackProperties(OperationIds::tracks::set_pan, context,
                                    {.id = trackId, .pan = pan});
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::setTrackMute(const CommandContext &context, const TrackId trackId,
                                              const bool mute) {
        return patchTrackProperties(OperationIds::tracks::set_mute, context,
                                    {.id = trackId, .mute = mute});
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::setTrackSolo(const CommandContext &context, const TrackId trackId,
                                              const bool solo) {
        return patchTrackProperties(OperationIds::tracks::set_solo, context,
                                    {.id = trackId, .solo = solo});
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::patchTrackProperties(const OperationId &operationId,
                                                      const CommandContext &context,
                                                      const TrackPropertiesPatchDto &patch) {
        return m_dispatcher.dispatchDocumentCommand(
            operationId, context, trackPatchFingerprint(patch),
            [this, patch](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.track(session, patch.id);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *track = resolved.get();
                const Track::TrackProperties previous(*track);
                auto next = previous;
                if (patch.name)
                    next.name = *patch.name;
                if (patch.gain)
                    next.gain = *patch.gain;
                if (patch.pan)
                    next.pan = *patch.pan;
                if (patch.mute)
                    next.mute = *patch.mute;
                if (patch.solo)
                    next.solo = *patch.solo;
                if (!std::isfinite(next.gain) || !std::isfinite(next.pan) || next.pan < -1.0 ||
                    next.pan > 1.0) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("properties"), QStringLiteral("Track control is invalid")));
                }
                const bool changed = previous != next;
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Track, patch.id.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<TrackActions>();
                actions->editTrackProperties(previous, next, track);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::setTrackColor(const CommandContext &context, const TrackId trackId,
                                               const int colorIndex) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::tracks::set_color, context,
            withIntegerPrefix(trackId.value(), QByteArray::number(colorIndex)),
            [this, trackId, colorIndex](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.track(session, trackId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                if (colorIndex < 0 || colorIndex >= AutomationWire::TrackPaletteColorCount) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("color_index"), QStringLiteral("Color index is invalid")));
                }
                auto *track = resolved.get();
                const bool changed = track->colorIndex() != colorIndex;
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Track, trackId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                const Track::TrackProperties oldProperties(*track);
                auto newProperties = oldProperties;
                newProperties.colorIndex = colorIndex;
                auto actions = std::make_unique<TrackActions>();
                actions->editTrackProperties(oldProperties, newProperties, track);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult> ProjectAutomationFacade::setTrackDefaultLanguage(
        const CommandContext &context, const TrackId trackId, const QString &language) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::tracks::set_default_language, context,
            withIntegerPrefix(trackId.value(), language.toUtf8()),
            [this, trackId, language](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.track(session, trackId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                if (language.trimmed().isEmpty()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("language"), QStringLiteral("Language is empty")));
                }
                auto *track = resolved.get();
                const bool changed = track->defaultLanguage() != language;
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Track, trackId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<DefaultLanguageActions>();
                actions->setDefaultLanguage(track, language);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::insertClips(const CommandContext &context,
                                             const QList<ClipInsertDto> &clips) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::clips::insert, context, fingerprint(clips),
            [this, clips](DocumentSession &session, const bool validateOnly) {
                QList<Track *> tracks;
                tracks.reserve(clips.size());
                for (const auto &item : clips) {
                    auto track = m_objects.track(session, item.trackId);
                    if (!track)
                        return AutomationResult<MutationResult>(track.getError());
                    auto validation = validate(item.clip);
                    if (!validation)
                        return AutomationResult<MutationResult>(validation.getError());
                    tracks.append(track.get());
                }
                auto clientRefValidation = validateClientRefs(clips);
                if (!clientRefValidation)
                    return AutomationResult<MutationResult>(clientRefValidation.getError());
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, !clips.isEmpty()));
                if (clips.isEmpty())
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                std::vector<std::unique_ptr<Clip>> ownedClips;
                ownedClips.reserve(static_cast<size_t>(clips.size()));
                QList<Clip *> rawClips;
                QList<ObjectRef> affected;
                QList<CreatedObjectRef> createdObjects;
                for (int index = 0; index < clips.size(); ++index) {
                    auto clip = buildClip(clips.at(index).clip, tracks.at(index),
                                          session.model()->timeline(), &createdObjects);
                    rawClips.append(clip.get());
                    affected.append({ObjectKind::Clip, clip->id()});
                    ownedClips.push_back(std::move(clip));
                }
                auto actions = std::make_unique<ClipActions>();
                actions->insertClips(rawClips, tracks);
                auto result = m_committer.commit(session, std::move(actions), affected,
                                                 std::move(createdObjects));
                if (result) {
                    for (auto &clip : ownedClips)
                        clip.release();
                }
                return result;
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::commitBatchImport(const CommandContext &context,
                                                   const BatchImportDraftDto &batch) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::imports::commit_batch, context, fingerprint(batch),
            [this, batch](DocumentSession &session, const bool validateOnly) {
                auto *model = session.model();
                if (!model)
                    return AutomationResult<MutationResult>(invalidModelError());
                if (batch.timeline.tempos().isEmpty() ||
                    batch.timeline.timeSignatures().isEmpty()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("timeline"),
                        QStringLiteral("Import timeline is incomplete")));
                }

                QList<Track *> existingTracks;
                existingTracks.reserve(batch.items.size());
                for (const auto &item : batch.items) {
                    if (item.clips.isEmpty()) {
                        return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                            QStringLiteral("items.clips"),
                            QStringLiteral("An import item must contain at least one clip")));
                    }
                    if (item.existingTrackId) {
                        auto track = m_objects.track(session, *item.existingTrackId);
                        if (!track)
                            return AutomationResult<MutationResult>(track.getError());
                        existingTracks.append(track.get());
                    } else {
                        auto trackValidation = validate(item.newTrack);
                        if (!trackValidation)
                            return AutomationResult<MutationResult>(trackValidation.getError());
                        existingTracks.append(nullptr);
                    }
                    for (const auto &clip : item.clips) {
                        auto clipValidation = validate(clip);
                        if (!clipValidation)
                            return AutomationResult<MutationResult>(clipValidation.getError());
                    }
                }
                auto clientRefValidation = validateClientRefs(batch);
                if (!clientRefValidation)
                    return AutomationResult<MutationResult>(clientRefValidation.getError());

                const bool timelineChanged =
                    model->timeline().tempos() != batch.timeline.tempos() ||
                    model->timeline().timeSignatures() != batch.timeline.timeSignatures();
                const bool changed = timelineChanged || !batch.items.isEmpty();
                if (validateOnly)
                    return AutomationResult<MutationResult>(m_committer.preview(session, changed));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                QList<BatchImportActions::Item> actionsItems;
                QList<ObjectRef> affected;
                QList<CreatedObjectRef> createdObjects;
                std::vector<std::unique_ptr<Track>> ownedTracks;
                std::vector<std::unique_ptr<Clip>> ownedClips;
                for (int index = 0; index < batch.items.size(); ++index) {
                    const auto &item = batch.items.at(index);
                    if (item.existingTrackId) {
                        auto *targetTrack = existingTracks.at(index);
                        for (const auto &clipDraft : item.clips) {
                            auto clip =
                                buildClip(clipDraft, targetTrack, batch.timeline, &createdObjects);
                            affected.append({ObjectKind::Clip, clip->id()});
                            actionsItems.append({clip.get(), targetTrack, nullptr});
                            ownedClips.push_back(std::move(clip));
                        }
                        continue;
                    }

                    auto trackDraft = item.newTrack;
                    trackDraft.clips.clear();
                    auto track = buildTrack(trackDraft, batch.timeline, &createdObjects);
                    affected.append({ObjectKind::Track, track->id()});
                    for (const auto &clipDraft : item.clips) {
                        auto clip =
                            buildClip(clipDraft, track.get(), batch.timeline, &createdObjects);
                        affected.append({ObjectKind::Clip, clip->id()});
                        track->insertClip(clip.release());
                    }
                    actionsItems.append({nullptr, nullptr, track.get()});
                    ownedTracks.push_back(std::move(track));
                }

                auto actions = std::unique_ptr<BatchImportActions>(BatchImportActions::build(
                    model->timeline().tempos(), batch.timeline.tempos(),
                    model->timeline().timeSignatures(), batch.timeline.timeSignatures(),
                    actionsItems, model));
                auto result = m_committer.commit(session, std::move(actions), affected,
                                                 std::move(createdObjects));
                if (result) {
                    for (auto &track : ownedTracks)
                        track.release();
                    for (auto &clip : ownedClips)
                        clip.release();
                }
                return result;
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::removeClips(const CommandContext &context, QList<ClipId> clipIds) {
        std::sort(clipIds.begin(), clipIds.end(), [](const ClipId left, const ClipId right) {
            return left.value() < right.value();
        });
        const auto hasDuplicate =
            std::adjacent_find(clipIds.cbegin(), clipIds.cend()) != clipIds.cend();
        QList<int> rawIds;
        rawIds.reserve(clipIds.size());
        for (const auto id : clipIds)
            rawIds.append(id.value());
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::clips::remove, context, integerListFingerprint(rawIds),
            [this, clipIds = std::move(clipIds), hasDuplicate](DocumentSession &session,
                                                               const bool validateOnly) {
                if (hasDuplicate) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("clip_ids"), QStringLiteral("Clip IDs must be unique")));
                }
                QList<Clip *> clips;
                QList<Track *> tracks;
                QList<ObjectRef> affected;
                for (const auto id : clipIds) {
                    auto resolved = m_objects.clip(session, id);
                    if (!resolved)
                        return AutomationResult<MutationResult>(resolved.getError());
                    clips.append(resolved.get().clip);
                    tracks.append(resolved.get().track);
                    affected.append({ObjectKind::Clip, id.value()});
                }
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, !clips.isEmpty(), affected));
                if (clips.isEmpty())
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<ClipActions>();
                actions->removeClips(clips, tracks);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::setClipProperties(const CommandContext &context,
                                                   const ClipPropertiesDto &properties,
                                                   const std::optional<TrackId> targetTrackId) {
        return setClipProperties(OperationIds::clips::set_properties, context, properties,
                                 targetTrackId);
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::duplicateClips(const CommandContext &context,
                                                QList<ClipId> clipIds,
                                                const ClipDuplicateDestinationDto &destination) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::clips::duplicate, context, clipDuplicateFingerprint(clipIds, destination),
            [this, clipIds = std::move(clipIds), destination](DocumentSession &session,
                                                              const bool validateOnly) {
                if (clipIds.isEmpty())
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                QSet<int> seen;
                QList<ResolvedClip> sources;
                int minimumStart = std::numeric_limits<int>::max();
                for (const auto id : clipIds) {
                    if (seen.contains(id.value())) {
                        return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                            QStringLiteral("clip_ids"), QStringLiteral("Clip IDs must be unique")));
                    }
                    seen.insert(id.value());
                    auto resolved = m_objects.clip(session, id);
                    if (!resolved)
                        return AutomationResult<MutationResult>(resolved.getError());
                    sources.append(resolved.get());
                    minimumStart = std::min(minimumStart, resolved.get().clip->start());
                }
                Track *fixedTarget = nullptr;
                if (destination.targetTrackId) {
                    auto target = m_objects.track(session, *destination.targetTrackId);
                    if (!target)
                        return AutomationResult<MutationResult>(target.getError());
                    fixedTarget = target.get();
                }
                if (destination.targetStart < 0) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("destination.target_start"),
                        QStringLiteral("Destination start must be non-negative")));
                }
                if (validateOnly)
                    return AutomationResult<MutationResult>(m_committer.preview(session, true));

                const auto delta = destination.targetStart - minimumStart;
                QList<Track *> targets;
                QList<Clip *> rawClips;
                QList<ObjectRef> affected;
                QList<CreatedObjectRef> createdObjects;
                std::vector<std::unique_ptr<Clip>> owned;
                for (const auto &source : sources) {
                    auto draft = clipDraftDto(*source.clip);
                    draft.properties.start += delta;
                    for (auto &keyframe : draft.ownSpeakerMixData.dynamicKeyframes)
                        keyframe.id = IdGenerator::instance()->next();
                    for (auto &parameter : draft.params) {
                        for (auto &curve : parameter.curves) {
                            curve.id = {};
                            for (auto &node : curve.nodes)
                                node.id = {};
                        }
                    }
                    auto *target = fixedTarget ? fixedTarget : source.track;
                    auto clip =
                        buildClip(draft, target, session.model()->timeline(), &createdObjects);
                    targets.append(target);
                    rawClips.append(clip.get());
                    affected.append({ObjectKind::Clip, clip->id()});
                    owned.push_back(std::move(clip));
                }
                auto actions = std::make_unique<ClipActions>();
                actions->insertClips(rawClips, targets);
                auto result = m_committer.commit(session, std::move(actions), affected,
                                                 std::move(createdObjects));
                if (result) {
                    for (auto &clip : owned)
                        clip.release();
                }
                return result;
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::moveClips(const CommandContext &context,
                                           const QList<ClipMoveDto> &moves) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::clips::move, context, clipMovesFingerprint(moves),
            [this, moves](DocumentSession &session, const bool validateOnly) {
                QSet<int> seen;
                struct PreparedMove {
                    ResolvedClip resolved;
                    Track *target = nullptr;
                    Clip::ClipCommonProperties previous;
                    Clip::ClipCommonProperties next;
                };
                QList<PreparedMove> prepared;
                QList<ObjectRef> affected;
                bool changed = false;
                for (const auto &move : moves) {
                    if (seen.contains(move.id.value())) {
                        return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                            QStringLiteral("moves"), QStringLiteral("Clip IDs must be unique")));
                    }
                    seen.insert(move.id.value());
                    if (move.start < 0) {
                        return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                            QStringLiteral("moves.start"),
                            QStringLiteral("Clip start must be non-negative")));
                    }
                    auto resolved = m_objects.clip(session, move.id);
                    if (!resolved)
                        return AutomationResult<MutationResult>(resolved.getError());
                    auto target = m_objects.track(session, move.targetTrackId);
                    if (!target)
                        return AutomationResult<MutationResult>(target.getError());
                    PreparedMove item{resolved.get(),
                                      target.get(),
                                      Clip::ClipCommonProperties(*resolved.get().clip),
                                      {}};
                    item.next = item.previous;
                    if (auto *audio = qobject_cast<AudioClip *>(resolved.get().clip);
                        audio && audio->hasRealTimeAnchor()) {
                        auto drag = AudioClipDragState::begin(
                            audio->trimStartMs(), audio->playLengthMs(), audio->materialLengthMs(),
                            item.previous.start + item.previous.clipStart,
                            item.previous.start + item.previous.clipStart,
                            session.model()->timeline());
                        drag.moveTo(move.start, item.next, session.model()->timeline());
                        drag.writeTruth(item.next);
                    } else {
                        item.next.start = move.start - item.next.clipStart;
                    }
                    changed |= item.target != item.resolved.track ||
                               !clipPropertiesEqual(item.previous, toDto(item.next));
                    affected.append({ObjectKind::Clip, move.id.value()});
                    prepared.append(std::move(item));
                }
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                auto actions = std::make_unique<ClipActions>();
                for (const auto &item : prepared) {
                    if (item.target != item.resolved.track) {
                        actions->moveClipToTrack(item.previous, item.next, item.resolved.clip,
                                                 item.resolved.track, item.target);
                    } else if (item.resolved.clip->clipType() == Clip::Audio) {
                        actions->editAudioClipProperties(
                            {item.previous}, {item.next},
                            {static_cast<AudioClip *>(item.resolved.clip)}, {item.resolved.track});
                    } else {
                        actions->editSingingClipProperties(
                            {item.previous}, {item.next},
                            {static_cast<SingingClip *>(item.resolved.clip)},
                            {item.resolved.track});
                    }
                }
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::resizeClipLeft(const CommandContext &context, const ClipId clipId,
                                                const int start) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::clips::resize_left, context,
            withIntegerPrefix(clipId.value(), QByteArray::number(start)),
            [this, clipId, start](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.clip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = resolved.get().clip;
                const Clip::ClipCommonProperties previous(*clip);
                auto next = previous;
                const auto right = previous.start + previous.clipStart + previous.clipLen;
                bool accepted = false;
                if (auto *audio = qobject_cast<AudioClip *>(clip);
                    audio && audio->hasRealTimeAnchor()) {
                    auto drag = AudioClipDragState::begin(
                        audio->trimStartMs(), audio->playLengthMs(), audio->materialLengthMs(),
                        previous.start + previous.clipStart, previous.start + previous.clipStart,
                        session.model()->timeline());
                    accepted =
                        drag.resizeLeftTo(start, right, 1, next, session.model()->timeline());
                    if (accepted)
                        drag.writeTruth(next);
                } else {
                    accepted = ClipResizeUtils::updateLeftEdge(next, start, 1);
                }
                if (!accepted) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("start"), QStringLiteral("Clip left edge is invalid")));
                }
                const bool changed = !clipPropertiesEqual(previous, toDto(next));
                const QList<ObjectRef> affected{
                    {ObjectKind::Clip, clipId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<ClipActions>();
                if (clip->clipType() == Clip::Audio) {
                    actions->editAudioClipProperties({previous}, {next},
                                                     {static_cast<AudioClip *>(clip)},
                                                     {resolved.get().track});
                } else {
                    actions->editSingingClipProperties({previous}, {next},
                                                       {static_cast<SingingClip *>(clip)},
                                                       {resolved.get().track});
                }
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::resizeClipRight(const CommandContext &context, const ClipId clipId,
                                                 const int end) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::clips::resize_right, context,
            withIntegerPrefix(clipId.value(), QByteArray::number(end)),
            [this, clipId, end](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.clip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = resolved.get().clip;
                const Clip::ClipCommonProperties previous(*clip);
                auto next = previous;
                const auto left = previous.start + previous.clipStart;
                bool accepted = false;
                if (auto *audio = qobject_cast<AudioClip *>(clip);
                    audio && audio->hasRealTimeAnchor()) {
                    auto drag = AudioClipDragState::begin(
                        audio->trimStartMs(), audio->playLengthMs(), audio->materialLengthMs(),
                        left, left, session.model()->timeline());
                    accepted = drag.resizeRightTo(end, left, 1, next, session.model()->timeline());
                    if (accepted)
                        drag.writeTruth(next);
                } else {
                    auto contentLength = previous.length;
                    const auto *singing = qobject_cast<SingingClip *>(clip);
                    if (singing) {
                        contentLength = ClipResizeUtils::furthestContentEnd(
                            singing->notes().begin(), singing->notes().end(),
                            AppGlobal::ticksPerWholeNote,
                            [](const Note *note) { return note->localStart() + note->length(); });
                    }
                    accepted = ClipResizeUtils::updateRightEdge(next, end - left, 1,
                                                                singing != nullptr, contentLength);
                }
                if (!accepted) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("end"), QStringLiteral("Clip right edge is invalid")));
                }
                const bool changed = !clipPropertiesEqual(previous, toDto(next));
                const QList<ObjectRef> affected{
                    {ObjectKind::Clip, clipId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<ClipActions>();
                if (clip->clipType() == Clip::Audio) {
                    actions->editAudioClipProperties({previous}, {next},
                                                     {static_cast<AudioClip *>(clip)},
                                                     {resolved.get().track});
                } else {
                    actions->editSingingClipProperties({previous}, {next},
                                                       {static_cast<SingingClip *>(clip)},
                                                       {resolved.get().track});
                }
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult> ProjectAutomationFacade::setClipProperties(
        const OperationId &operationId, const CommandContext &context,
        const ClipPropertiesDto &properties, const std::optional<TrackId> targetTrackId) {
        auto requestFingerprint = fingerprint(properties);
        if (targetTrackId)
            requestFingerprint = withIntegerPrefix(targetTrackId->value(), requestFingerprint);
        return m_dispatcher.dispatchDocumentCommand(
            operationId, context, requestFingerprint,
            [this, properties, targetTrackId](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.clip(session, properties.id);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                if (!validClipProperties(properties)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("properties"),
                        QStringLiteral("Clip geometry or gain is invalid")));
                }
                auto *targetTrack = resolved.get().track;
                if (targetTrackId) {
                    auto target = m_objects.track(session, *targetTrackId);
                    if (!target)
                        return AutomationResult<MutationResult>(target.getError());
                    targetTrack = target.get();
                }
                auto *clip = resolved.get().clip;
                const Clip::ClipCommonProperties oldProperties(*clip);
                const bool trackChanged = targetTrack != resolved.get().track;
                const bool propertiesChanged = !clipPropertiesEqual(oldProperties, properties);
                const bool changed = trackChanged || propertiesChanged;
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Clip, properties.id.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                auto actions = std::make_unique<ClipActions>();
                const auto newProperties = toModelProperties(properties);
                if (trackChanged) {
                    actions->moveClipToTrack(oldProperties, newProperties, clip,
                                             resolved.get().track, targetTrack);
                } else if (clip->clipType() == Clip::Audio) {
                    actions->editAudioClipProperties({oldProperties}, {newProperties},
                                                     {static_cast<AudioClip *>(clip)},
                                                     {resolved.get().track});
                } else {
                    actions->editSingingClipProperties({oldProperties}, {newProperties},
                                                       {static_cast<SingingClip *>(clip)},
                                                       {resolved.get().track});
                }
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::patchClipProperties(const CommandContext &context,
                                                     const ClipPropertiesPatchDto &patch) {
        return patchClipProperties(OperationIds::clips::set_properties, context, patch);
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::renameClip(const CommandContext &context, const ClipId clipId,
                                            const QString &name) {
        return patchClipProperties(OperationIds::clips::rename, context,
                                   {.id = clipId, .name = name});
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::setClipGain(const CommandContext &context, const ClipId clipId,
                                             const double gain) {
        return patchClipProperties(OperationIds::clips::set_gain, context,
                                   {.id = clipId, .gain = gain});
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::setClipMute(const CommandContext &context, const ClipId clipId,
                                             const bool mute) {
        return patchClipProperties(OperationIds::clips::set_mute, context,
                                   {.id = clipId, .mute = mute});
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::patchClipProperties(const OperationId &operationId,
                                                     const CommandContext &context,
                                                     const ClipPropertiesPatchDto &patch) {
        return m_dispatcher.dispatchDocumentCommand(
            operationId, context, clipPatchFingerprint(patch),
            [this, patch](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.clip(session, patch.id);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = resolved.get().clip;
                const Clip::ClipCommonProperties previous(*clip);
                auto next = previous;
                if (patch.name)
                    next.name = *patch.name;
                if (patch.start)
                    next.start = *patch.start;
                if (patch.length)
                    next.length = *patch.length;
                if (patch.clipStart)
                    next.clipStart = *patch.clipStart;
                if (patch.clipLen)
                    next.clipLen = *patch.clipLen;
                if (patch.gain)
                    next.gain = *patch.gain;
                if (patch.mute)
                    next.mute = *patch.mute;
                const ClipPropertiesDto validated{patch.id,
                                                  next.name,
                                                  next.start,
                                                  next.length,
                                                  next.clipStart,
                                                  next.clipLen,
                                                  next.gain,
                                                  next.mute,
                                                  next.trimStartMs,
                                                  next.playLengthMs,
                                                  next.materialLengthMs};
                if (!validClipProperties(validated)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("properties"),
                        QStringLiteral("Clip geometry or gain is invalid")));
                }
                auto *targetTrack = resolved.get().track;
                if (patch.targetTrackId) {
                    auto target = m_objects.track(session, *patch.targetTrackId);
                    if (!target)
                        return AutomationResult<MutationResult>(target.getError());
                    targetTrack = target.get();
                }
                const bool trackChanged = targetTrack != resolved.get().track;
                const bool changed = trackChanged || !clipPropertiesEqual(previous, validated);
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Clip, patch.id.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<ClipActions>();
                if (trackChanged) {
                    actions->moveClipToTrack(previous, next, clip, resolved.get().track,
                                             targetTrack);
                } else if (clip->clipType() == Clip::Audio) {
                    actions->editAudioClipProperties({previous}, {next},
                                                     {static_cast<AudioClip *>(clip)},
                                                     {resolved.get().track});
                } else {
                    actions->editSingingClipProperties({previous}, {next},
                                                       {static_cast<SingingClip *>(clip)},
                                                       {resolved.get().track});
                }
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult> ProjectAutomationFacade::relocateAudioClip(
        const CommandContext &context, const ClipId clipId, const QString &path,
        const AudioPathInfo &pathInfo, const QJsonObject &formatData) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::audio_clips::relocate, context,
            relocateFingerprint(clipId, path, pathInfo, formatData),
            [this, clipId, path, pathInfo, formatData](DocumentSession &session,
                                                       const bool validateOnly) {
                auto resolved = m_objects.audioClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                if (path.isEmpty()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("path"), QStringLiteral("Audio path is empty")));
                }
                auto *clip = static_cast<AudioClip *>(resolved.get().clip);
                auto effectivePathInfo = pathInfo;
                if (effectivePathInfo.relativeDir.isEmpty()) {
                    effectivePathInfo.relativeDir =
                        DiffscopeAudioWorkspace::relativeDirFor(path, session.path());
                }
                const bool changed =
                    clip->path() != path ||
                    clip->pathInfo().relativeDir != effectivePathInfo.relativeDir ||
                    clip->pathInfo().sha512 != effectivePathInfo.sha512 ||
                    clip->workspace().value(kAudioFormatDataKey) != formatData ||
                    clip->pathStatus() != AudioClip::PathStatus::Normal;
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Clip, clipId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<ClipActions>();
                actions->relocateAudioClip(clip, path, effectivePathInfo, formatData);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::confirmAudioClipPath(const CommandContext &context,
                                                      const ClipId clipId) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::audio_clips::confirm_path, context,
            withIntegerPrefix(clipId.value(), QByteArrayLiteral("confirm")),
            [this, clipId](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.audioClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<AudioClip *>(resolved.get().clip);
                const bool changed = clip->pathStatus() != AudioClip::PathStatus::Normal;
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Clip, clipId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                return AutomationResult<MutationResult>(m_committer.commitStateChange(
                    session, changed,
                    [clip] { clip->setPathStatus(AudioClip::PathStatus::Normal); }, affected));
            });
    }

    AutomationResult<MutationResult> ProjectAutomationFacade::confirmAudioClipPath(
        const CommandContext &context, const ClipId clipId, const QString &path,
        const AudioPathInfo &pathInfo, const QJsonObject &formatData) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::audio_clips::confirm_path, context,
            relocateFingerprint(clipId, path, pathInfo, formatData),
            [this, clipId, path, pathInfo, formatData](DocumentSession &session,
                                                       const bool validateOnly) {
                auto resolved = m_objects.audioClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                if (path.isEmpty()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("path"), QStringLiteral("Audio path is empty")));
                }
                auto *clip = static_cast<AudioClip *>(resolved.get().clip);
                auto effectivePathInfo = pathInfo;
                if (effectivePathInfo.relativeDir.isEmpty()) {
                    effectivePathInfo.relativeDir =
                        DiffscopeAudioWorkspace::relativeDirFor(path, session.path());
                }
                const bool changed =
                    clip->path() != path ||
                    clip->pathInfo().relativeDir != effectivePathInfo.relativeDir ||
                    clip->pathInfo().sha512 != effectivePathInfo.sha512 ||
                    clip->workspace().value(kAudioFormatDataKey) != formatData ||
                    clip->pathStatus() != AudioClip::PathStatus::Normal;
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Clip, clipId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<ClipActions>();
                actions->relocateAudioClip(clip, path, effectivePathInfo, formatData);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult> ProjectAutomationFacade::applyAudioDecodeCache(
        const CommandContext &context, const ClipId clipId,
        const AudioAssetSnapshotDto &expectedAsset, const AudioInfoModel &audioInfo) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::audio_clips::apply_decode_cache, context,
            audioCacheFingerprint(clipId, expectedAsset, audioInfo),
            [this, clipId, expectedAsset, audioInfo](DocumentSession &session,
                                                     const bool validateOnly) {
                auto resolved = m_objects.audioClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<AudioClip *>(resolved.get().clip);
                if (!decodeSourceMatches(*clip, expectedAsset))
                    return AutomationResult<MutationResult>(staleAudioAsset(clipId));
                const bool cacheChanged = !audioInfoEqual(clip->audioInfo(), audioInfo);
                const bool statusChanged = clip->pathStatus() == AudioClip::PathStatus::Missing;
                const bool changed = cacheChanged || statusChanged;
                if (!validateOnly && changed) {
                    if (cacheChanged) {
                        clip->setAudioInfo(audioInfo);
                        clip->notifyPropertyChanged();
                    }
                    if (statusChanged)
                        clip->setPathStatus(AudioClip::PathStatus::Normal);
                }
                return AutomationResult<MutationResult>(
                    cacheMutationResult(session, changed, validateOnly));
            });
    }

    AutomationResult<MutationResult> ProjectAutomationFacade::setAudioClipPathStatus(
        const CommandContext &context, const ClipId clipId,
        const AudioAssetSnapshotDto &expectedAsset, const AudioClip::PathStatus status) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::audio_clips::set_path_status, context,
            audioPathFingerprint(clipId, expectedAsset, {}, static_cast<int>(status)),
            [this, clipId, expectedAsset, status](DocumentSession &session,
                                                  const bool validateOnly) {
                auto resolved = m_objects.audioClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<AudioClip *>(resolved.get().clip);
                if (clip->sourceGeneration() != expectedAsset.sourceGeneration ||
                    clip->path() != expectedAsset.path)
                    return AutomationResult<MutationResult>(staleAudioAsset(clipId));
                const bool changed = clip->pathStatus() != status;
                if (!validateOnly && changed)
                    clip->setPathStatus(status);
                return AutomationResult<MutationResult>(
                    cacheMutationResult(session, changed, validateOnly));
            });
    }

    AutomationResult<MutationResult> ProjectAutomationFacade::applyResolvedAudioPath(
        const CommandContext &context, const ClipId clipId,
        const AudioAssetSnapshotDto &expectedAsset, const QString &resolvedPath,
        const AudioClip::PathStatus status) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::audio_clips::apply_resolved_path, context,
            audioResolveFingerprint(clipId, expectedAsset, resolvedPath, static_cast<int>(status)),
            [this, clipId, expectedAsset, resolvedPath, status](DocumentSession &session,
                                                                const bool validateOnly) {
                auto resolved = m_objects.audioClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<AudioClip *>(resolved.get().clip);
                if (!resolveSourceMatches(*clip, expectedAsset))
                    return AutomationResult<MutationResult>(staleAudioAsset(clipId));
                const QFileInfo resolvedFile(resolvedPath);
                if (!resolvedFile.isAbsolute()) {
                    return AutomationResult<MutationResult>(invalidResolvedAudioPath(
                        clipId, QStringLiteral("Resolved audio path must be absolute")));
                }
                if (!resolvedFile.exists()) {
                    AutomationError error;
                    error.code = AutomationErrorCode::FileNotFound;
                    error.message = QStringLiteral("Resolved audio file does not exist");
                    error.fieldPath = QStringLiteral("resolved_path");
                    error.object = ObjectRef{ObjectKind::Clip, clipId.value()};
                    return AutomationResult<MutationResult>(std::move(error));
                }
                if (!resolvedFile.isFile()) {
                    return AutomationResult<MutationResult>(invalidResolvedAudioPath(
                        clipId, QStringLiteral("Resolved audio path is not a file")));
                }
                const bool changed = clip->path() != resolvedPath || clip->pathStatus() != status;
                if (!validateOnly && changed) {
                    const bool pathChanged = clip->path() != resolvedPath;
                    clip->setPathStatus(status);
                    clip->setPath(resolvedPath);
                    if (!pathChanged)
                        clip->notifySourceChanged();
                }
                return AutomationResult<MutationResult>(
                    cacheMutationResult(session, changed, validateOnly));
            });
    }

    AutomationResult<MutationResult> ProjectAutomationFacade::setAudioClipHash(
        const CommandContext &context, const ClipId clipId,
        const AudioAssetSnapshotDto &expectedAsset, const QString &sha512) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::audio_clips::set_hash, context,
            audioPathFingerprint(clipId, expectedAsset, sha512),
            [this, clipId, expectedAsset, sha512](DocumentSession &session,
                                                  const bool validateOnly) {
                auto resolved = m_objects.audioClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<AudioClip *>(resolved.get().clip);
                if (clip->sourceGeneration() != expectedAsset.sourceGeneration ||
                    clip->path() != expectedAsset.path)
                    return AutomationResult<MutationResult>(staleAudioAsset(clipId));
                if (sha512.isEmpty()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("sha512"), QStringLiteral("Audio hash is empty")));
                }
                const bool changed = clip->pathInfo().sha512 != sha512;
                if (!validateOnly && changed) {
                    auto info = clip->pathInfo();
                    info.sha512 = sha512;
                    clip->setPathInfo(info);
                }
                return AutomationResult<MutationResult>(
                    cacheMutationResult(session, changed, validateOnly));
            });
    }

    AutomationResult<MutationResult> ProjectAutomationFacade::setSingingClipDefaultLanguage(
        const CommandContext &context, const ClipId clipId, const QString &language) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::clips::set_default_language, context,
            withIntegerPrefix(clipId.value(), language.toUtf8()),
            [this, clipId, language](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                if (language.trimmed().isEmpty()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("language"), QStringLiteral("Language is empty")));
                }
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                const bool changed = clip->defaultLanguage() != language;
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Clip, clipId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<DefaultLanguageActions>();
                actions->setDefaultLanguage(clip, language);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    void ProjectAutomationFacade::registerOperations() {
        const auto add = [this](OperationDescriptor descriptor) {
            const auto result = m_catalog.add(std::move(descriptor));
            Q_ASSERT(result);
        };
        add({
            .id = OperationIds::project::get,
            .category = QStringLiteral("project"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });

        const auto addMutation =
            [&add](const OperationId &id, const HistoryPolicy historyPolicy = HistoryPolicy::Record,
                   const FileAccessPolicy fileAccess = FileAccessPolicy::None,
                   const RevisionPolicy revisionPolicy = RevisionPolicy::Increment) {
                add({
                    .id = id,
                    .category = id.section('.', 0, 0),
                    .kind = OperationKind::Command,
                    .syncMode = SyncMode::Synchronous,
                    .documentPolicy = DocumentPolicy::Write,
                    .revisionPolicy = revisionPolicy,
                    .historyPolicy = historyPolicy,
                    .fileAccess = fileAccess,
                    .hostAvailability = HostAvailability::Core,
                    .safety = SafetyClass::Reversible,
                    .exposure = ExposurePolicy::InternalOnly,
                    .idempotency = IdempotencyPolicy::DocumentGeneration,
                });
            };
        addMutation(OperationIds::audio_clips::confirm_path, HistoryPolicy::None);
        addMutation(OperationIds::audio_clips::apply_decode_cache, HistoryPolicy::None,
                    FileAccessPolicy::None, RevisionPolicy::None);
        addMutation(OperationIds::audio_clips::apply_resolved_path, HistoryPolicy::None,
                    FileAccessPolicy::Read, RevisionPolicy::None);
        addMutation(OperationIds::audio_clips::relocate, HistoryPolicy::Record,
                    FileAccessPolicy::Read);
        addMutation(OperationIds::audio_clips::set_hash, HistoryPolicy::None,
                    FileAccessPolicy::Read, RevisionPolicy::None);
        addMutation(OperationIds::audio_clips::set_path_status, HistoryPolicy::None,
                    FileAccessPolicy::None, RevisionPolicy::None);
        addMutation(OperationIds::clips::insert);
        addMutation(OperationIds::clips::duplicate);
        addMutation(OperationIds::clips::move);
        addMutation(OperationIds::imports::commit_batch);
        addMutation(OperationIds::clips::remove);
        addMutation(OperationIds::clips::rename);
        addMutation(OperationIds::clips::resize_left);
        addMutation(OperationIds::clips::resize_right);
        addMutation(OperationIds::clips::set_default_language);
        addMutation(OperationIds::clips::set_gain);
        addMutation(OperationIds::clips::set_mute);
        addMutation(OperationIds::clips::set_properties);
        addMutation(OperationIds::tracks::insert);
        addMutation(OperationIds::tracks::move);
        addMutation(OperationIds::tracks::rename);
        addMutation(OperationIds::tracks::remove);
        addMutation(OperationIds::tracks::set_color);
        addMutation(OperationIds::tracks::set_default_language);
        addMutation(OperationIds::tracks::set_gain);
        addMutation(OperationIds::tracks::set_mute);
        addMutation(OperationIds::tracks::set_pan);
        addMutation(OperationIds::tracks::set_properties);
        addMutation(OperationIds::tracks::set_solo);
    }

} // namespace Automation
