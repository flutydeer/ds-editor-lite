#include "ProjectAutomationFacade.h"

#include "Controller/Actions/AppModel/Clip/ClipActions.h"
#include "Controller/Actions/AppModel/Import/BatchImportActions.h"
#include "Controller/Actions/AppModel/Track/TrackActions.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QDataStream>
#include <QIODevice>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace Automation {
    namespace {
        constexpr auto kAudioFormatDataKey = "diffscope.audio.formatData";

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

        QByteArray relocateFingerprint(const ClipId clipId, const QString &path,
                                       const AudioPathInfo &pathInfo,
                                       const QJsonObject &formatData) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << clipId.value() << path << pathInfo.relativeDir << pathInfo.sha512
                   << formatData;
            return result;
        }

        QByteArray audioCacheFingerprint(const ClipId clipId, const QString &path,
                                         const AudioInfoModel &audioInfo) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << clipId.value() << path << audioInfo.sampleRate << audioInfo.channels
                   << audioInfo.frames << audioInfo.chunkSize << audioInfo.mipmapScale
                   << audioInfo.peakCache.size();
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

        QByteArray audioStateFingerprint(const ClipId clipId, const QString &expectedPath,
                                         const QString &value, const int state = -1) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << clipId.value() << expectedPath << value << state;
            return result;
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

        AutomationError staleAudioPath(const ClipId clipId) {
            auto error = AutomationError::invalidArgument(
                QStringLiteral("expected_path"), QStringLiteral("Audio clip path changed"));
            error.object = ObjectRef{ObjectKind::Clip, clipId.value()};
            return error;
        }

        bool validClipProperties(const ClipPropertiesDto &properties) {
            return properties.start + properties.clipStart >= 0 && properties.length >= 0 &&
                   properties.clipStart >= 0 && properties.clipLen >= 0 &&
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
            QStringLiteral("project.get"), documentId, [](DocumentSession &session) {
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
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("tracks.insert"), context,
            withIntegerPrefix(index, fingerprint(trackDraft)),
            [this, index, trackDraft](DocumentSession &session, const bool validateOnly) {
                auto *model = session.model();
                if (!model)
                    return AutomationResult<MutationResult>(invalidModelError());
                if (index < 0 || index > model->tracks().size()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("index"), QStringLiteral("Track index is out of range")));
                }
                auto validation = validate(trackDraft);
                if (!validation)
                    return AutomationResult<MutationResult>(validation.getError());
                if (validateOnly)
                    return AutomationResult<MutationResult>(m_committer.preview(session, true));

                QList<CreatedObjectRef> createdObjects;
                auto track = buildTrack(trackDraft, model->timeline(), &createdObjects);
                const auto id = track->id();
                auto actions = std::make_unique<TrackActions>();
                actions->insertTrack(track.release(), index, model);
                return m_committer.commit(session, std::move(actions),
                                          {
                                              {ObjectKind::Track, id}
                },
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
            QStringLiteral("tracks.remove"), context, integerListFingerprint(rawIds),
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
            QStringLiteral("tracks.move"), context,
            withIntegerPrefix(trackId.value(), QByteArray::number(targetIndex)),
            [this, trackId, targetIndex](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.track(session, trackId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *model = session.model();
                const auto currentIndex = model->tracks().indexOf(resolved.get());
                if (targetIndex < 0 || targetIndex >= model->tracks().size()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("target_index"),
                        QStringLiteral("Target track index is out of range")));
                }
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Track, trackId.value()}
                };
                const bool changed = currentIndex != targetIndex;
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
        ProjectAutomationFacade::setTrackProperties(const CommandContext &context,
                                                    const TrackPropertiesDto &properties) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("tracks.set_properties"), context, fingerprint(properties),
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

                Track::TrackProperties newProperties;
                newProperties.id = properties.id.value();
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
        ProjectAutomationFacade::setTrackColor(const CommandContext &context, const TrackId trackId,
                                               const int colorIndex) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("tracks.set_color"), context,
            withIntegerPrefix(trackId.value(), QByteArray::number(colorIndex)),
            [this, trackId, colorIndex](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.track(session, trackId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                if (colorIndex < 0) {
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
                return AutomationResult<MutationResult>(m_committer.commitStateChange(
                    session, changed, [track, colorIndex] { track->setColorIndex(colorIndex); },
                    affected));
            });
    }

    AutomationResult<MutationResult> ProjectAutomationFacade::setTrackDefaultLanguage(
        const CommandContext &context, const TrackId trackId, const QString &language) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("tracks.set_default_language"), context,
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
                return AutomationResult<MutationResult>(m_committer.commitStateChange(
                    session, changed, [track, language] { track->setDefaultLanguage(language); },
                    affected));
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::insertClips(const CommandContext &context,
                                             const QList<ClipInsertDto> &clips) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("clips.insert"), context, fingerprint(clips),
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
            QStringLiteral("imports.commit_batch"), context, fingerprint(batch),
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
            QStringLiteral("clips.remove"), context, integerListFingerprint(rawIds),
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
        auto requestFingerprint = fingerprint(properties);
        if (targetTrackId)
            requestFingerprint = withIntegerPrefix(targetTrackId->value(), requestFingerprint);
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("clips.set_properties"), context, requestFingerprint,
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

    AutomationResult<MutationResult> ProjectAutomationFacade::relocateAudioClip(
        const CommandContext &context, const ClipId clipId, const QString &path,
        const AudioPathInfo &pathInfo, const QJsonObject &formatData) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("audio_clips.relocate"), context,
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
                const bool changed = clip->path() != path ||
                                     clip->pathInfo().relativeDir != pathInfo.relativeDir ||
                                     clip->pathInfo().sha512 != pathInfo.sha512 ||
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
                actions->relocateAudioClip(clip, path, pathInfo, formatData);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::confirmAudioClipPath(const CommandContext &context,
                                                      const ClipId clipId) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("audio_clips.confirm_path"), context,
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

    AutomationResult<MutationResult> ProjectAutomationFacade::applyAudioDecodeCache(
        const CommandContext &context, const ClipId clipId, const QString &expectedPath,
        const AudioInfoModel &audioInfo) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("audio_clips.apply_decode_cache"), context,
            audioCacheFingerprint(clipId, expectedPath, audioInfo),
            [this, clipId, expectedPath, audioInfo](DocumentSession &session,
                                                    const bool validateOnly) {
                auto resolved = m_objects.audioClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<AudioClip *>(resolved.get().clip);
                if (clip->path() != expectedPath)
                    return AutomationResult<MutationResult>(staleAudioPath(clipId));
                const bool changed = !audioInfoEqual(clip->audioInfo(), audioInfo);
                if (!validateOnly && changed) {
                    clip->setAudioInfo(audioInfo);
                    clip->notifyPropertyChanged();
                }
                return AutomationResult<MutationResult>(
                    cacheMutationResult(session, changed, validateOnly));
            });
    }

    AutomationResult<MutationResult> ProjectAutomationFacade::setAudioClipPathStatus(
        const CommandContext &context, const ClipId clipId, const QString &expectedPath,
        const AudioClip::PathStatus status) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("audio_clips.set_path_status"), context,
            audioStateFingerprint(clipId, expectedPath, {}, static_cast<int>(status)),
            [this, clipId, expectedPath, status](DocumentSession &session,
                                                 const bool validateOnly) {
                auto resolved = m_objects.audioClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<AudioClip *>(resolved.get().clip);
                if (clip->path() != expectedPath)
                    return AutomationResult<MutationResult>(staleAudioPath(clipId));
                const bool changed = clip->pathStatus() != status;
                if (!validateOnly && changed)
                    clip->setPathStatus(status);
                return AutomationResult<MutationResult>(
                    cacheMutationResult(session, changed, validateOnly));
            });
    }

    AutomationResult<MutationResult> ProjectAutomationFacade::applyResolvedAudioPath(
        const CommandContext &context, const ClipId clipId, const QString &expectedPath,
        const QString &resolvedPath, const AudioClip::PathStatus status) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("audio_clips.apply_resolved_path"), context,
            audioStateFingerprint(clipId, expectedPath, resolvedPath, static_cast<int>(status)),
            [this, clipId, expectedPath, resolvedPath, status](DocumentSession &session,
                                                               const bool validateOnly) {
                auto resolved = m_objects.audioClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<AudioClip *>(resolved.get().clip);
                if (clip->path() != expectedPath)
                    return AutomationResult<MutationResult>(staleAudioPath(clipId));
                if (resolvedPath.isEmpty()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("resolved_path"), QStringLiteral("Resolved path is empty")));
                }
                const bool changed = clip->path() != resolvedPath || clip->pathStatus() != status;
                if (!validateOnly && changed) {
                    clip->setPath(resolvedPath);
                    clip->setPathStatus(status);
                }
                return AutomationResult<MutationResult>(
                    cacheMutationResult(session, changed, validateOnly));
            });
    }

    AutomationResult<MutationResult>
        ProjectAutomationFacade::setAudioClipHash(const CommandContext &context,
                                                  const ClipId clipId, const QString &expectedPath,
                                                  const QString &sha512) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("audio_clips.set_hash"), context,
            audioStateFingerprint(clipId, expectedPath, sha512),
            [this, clipId, expectedPath, sha512](DocumentSession &session,
                                                 const bool validateOnly) {
                auto resolved = m_objects.audioClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<AudioClip *>(resolved.get().clip);
                if (clip->path() != expectedPath)
                    return AutomationResult<MutationResult>(staleAudioPath(clipId));
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
            QStringLiteral("clips.set_default_language"), context,
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
                return AutomationResult<MutationResult>(m_committer.commitStateChange(
                    session, changed, [clip, language] { clip->setDefaultLanguage(language); },
                    affected));
            });
    }

    void ProjectAutomationFacade::registerOperations() {
        const auto add = [this](OperationDescriptor descriptor) {
            const auto result = m_catalog.add(std::move(descriptor));
            Q_ASSERT(result);
        };
        add({
            .id = QStringLiteral("project.get"),
            .category = QStringLiteral("project"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.DocumentRef.v1"),
            .outputContract = QStringLiteral("automation.ProjectSnapshot.v1"),
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });

        const auto addMutation = [&add](const QString &id, const QString &inputContract,
                                        const HistoryPolicy historyPolicy = HistoryPolicy::Record,
                                        const FileAccessPolicy fileAccess = FileAccessPolicy::None,
                                        const RevisionPolicy revisionPolicy =
                                            RevisionPolicy::Increment) {
            add({
                .id = id,
                .category = id.section('.', 0, 0),
                .kind = OperationKind::Command,
                .syncMode = SyncMode::Synchronous,
                .inputContract = inputContract,
                .outputContract = QStringLiteral("automation.MutationResult.v1"),
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
        addMutation(QStringLiteral("audio_clips.confirm_path"),
                    QStringLiteral("automation.ConfirmAudioPathCommand.v1"), HistoryPolicy::None);
        addMutation(QStringLiteral("audio_clips.apply_decode_cache"),
                    QStringLiteral("automation.ApplyAudioDecodeCacheCommand.v1"),
                    HistoryPolicy::None, FileAccessPolicy::None, RevisionPolicy::None);
        addMutation(QStringLiteral("audio_clips.apply_resolved_path"),
                    QStringLiteral("automation.ApplyResolvedAudioPathCommand.v1"),
                    HistoryPolicy::None, FileAccessPolicy::Read, RevisionPolicy::None);
        addMutation(QStringLiteral("audio_clips.relocate"),
                    QStringLiteral("automation.RelocateAudioClipCommand.v1"), HistoryPolicy::Record,
                    FileAccessPolicy::Read);
        addMutation(QStringLiteral("audio_clips.set_hash"),
                    QStringLiteral("automation.SetAudioClipHashCommand.v1"), HistoryPolicy::None,
                    FileAccessPolicy::Read, RevisionPolicy::None);
        addMutation(QStringLiteral("audio_clips.set_path_status"),
                    QStringLiteral("automation.SetAudioClipPathStatusCommand.v1"),
                    HistoryPolicy::None, FileAccessPolicy::None, RevisionPolicy::None);
        addMutation(QStringLiteral("clips.insert"),
                    QStringLiteral("automation.InsertClipsCommand.v1"));
        addMutation(QStringLiteral("imports.commit_batch"),
                    QStringLiteral("automation.PreparedBatchImportCommand.v1"));
        addMutation(QStringLiteral("clips.remove"),
                    QStringLiteral("automation.RemoveClipsCommand.v1"));
        addMutation(QStringLiteral("clips.set_default_language"),
                    QStringLiteral("automation.SetClipDefaultLanguageCommand.v1"),
                    HistoryPolicy::None);
        addMutation(QStringLiteral("clips.set_properties"),
                    QStringLiteral("automation.SetClipPropertiesCommand.v1"));
        addMutation(QStringLiteral("tracks.insert"),
                    QStringLiteral("automation.InsertTrackCommand.v1"));
        addMutation(QStringLiteral("tracks.move"),
                    QStringLiteral("automation.MoveTrackCommand.v1"));
        addMutation(QStringLiteral("tracks.remove"),
                    QStringLiteral("automation.RemoveTracksCommand.v1"));
        addMutation(QStringLiteral("tracks.set_color"),
                    QStringLiteral("automation.SetTrackColorCommand.v1"), HistoryPolicy::None);
        addMutation(QStringLiteral("tracks.set_default_language"),
                    QStringLiteral("automation.SetTrackDefaultLanguageCommand.v1"),
                    HistoryPolicy::None);
        addMutation(QStringLiteral("tracks.set_properties"),
                    QStringLiteral("automation.SetTrackPropertiesCommand.v1"));
    }

} // namespace Automation
