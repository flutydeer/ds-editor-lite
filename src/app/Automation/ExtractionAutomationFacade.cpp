#include "ExtractionAutomationFacade.h"
#include "OperationIds.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QDataStream>
#include <QFileInfo>

#include <cmath>
#include <limits>

namespace Automation {
    namespace {
        AutomationError unavailable(const QString &message) {
            AutomationError error;
            error.code = AutomationErrorCode::ModuleNotReady;
            error.message = message;
            return error;
        }

        AutomationError taskError(AutomationError error, const TaskId &taskId,
                                  const OperationId &operationId) {
            if (error.operationId.isEmpty())
                error.operationId = operationId;
            if (!error.taskId)
                error.taskId = taskId;
            return error;
        }

        AutomationError backendError(const TaskId &taskId, const OperationId &operationId,
                                     const AutomationErrorCode code, QString message) {
            AutomationError error;
            error.code = code;
            error.operationId = operationId;
            error.taskId = taskId;
            error.message =
                message.isEmpty() ? QStringLiteral("Audio extraction failed") : std::move(message);
            return error;
        }

        QByteArray pitchFingerprint(const ClipId audioClipId, const ClipId singingClipId,
                                    const PitchExtractionOptionsDto &options) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << audioClipId.value() << singingClipId.value() << options.modelId
                   << options.minimumFrequency.has_value() << options.minimumFrequency.value_or(0.0)
                   << options.maximumFrequency.has_value()
                   << options.maximumFrequency.value_or(0.0);
            return result;
        }

        QByteArray midiFingerprint(const ClipId audioClipId,
                                   const MidiExtractionOptionsDto &options) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << audioClipId.value() << options.modelId << options.defaultLanguage
                   << options.defaultLyric << options.clientRef
                   << options.minimumNoteLength.has_value() << options.minimumNoteLength.value_or(0)
                   << options.destinationMode << options.targetTrackId.has_value()
                   << options.targetTrackId.value_or(TrackId{}).value()
                   << options.targetClipId.has_value()
                   << options.targetClipId.value_or(ClipId{}).value() << options.targetStart;
            return result;
        }

        AutomationResult<QList<CurveDraftDto>>
            pitchCurves(const PitchExtractionInput &input,
                        const QList<PitchExtractionSegmentDto> &segments) {
            QList<CurveDraftDto> curves;
            curves.reserve(segments.size());
            for (qsizetype segmentIndex = 0; segmentIndex < segments.size(); ++segmentIndex) {
                const auto &segment = segments.at(segmentIndex);
                CurveDraftDto curve;
                curve.type = CurveDraftDto::Type::Draw;
                curve.localStart = segment.globalStartTick - input.singingClipStartTick;
                curve.values.reserve(segment.values.size());
                for (const auto value : segment.values) {
                    const auto scaled = value * 100.0;
                    if (!std::isfinite(scaled) ||
                        scaled < static_cast<double>(std::numeric_limits<int>::min()) ||
                        scaled > static_cast<double>(std::numeric_limits<int>::max())) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("result.segments[%1].values").arg(segmentIndex),
                            QStringLiteral("Pitch extraction returned an invalid value"));
                    }
                    curve.values.append(static_cast<int>(scaled));
                }
                curves.append(std::move(curve));
            }
            return curves;
        }
    }

    ExtractionAutomationFacade::ExtractionAutomationFacade(AutomationDispatcher &dispatcher,
                                                           AutomationTaskManager &tasks,
                                                           DocumentObjectResolver &objects,
                                                           ParameterAutomationFacade &parameters,
                                                           ProjectAutomationFacade &project,
                                                           NoteAutomationFacade &notes,
                                                           ExtractionRuntimeServices services)
        : m_dispatcher(dispatcher), m_tasks(tasks), m_objects(objects), m_parameters(parameters),
          m_project(project), m_notes(notes), m_services(std::move(services)) {
    }

    AutomationResult<TaskAcceptedResult>
        ExtractionAutomationFacade::startPitch(const CommandContext &context,
                                               const ClipId audioClipId, const ClipId singingClipId,
                                               ExtractionObserver observer) {
        return startPitch(context, audioClipId, singingClipId, {}, std::move(observer));
    }

    AutomationResult<TaskAcceptedResult> ExtractionAutomationFacade::startPitch(
        const CommandContext &context, const ClipId audioClipId, const ClipId singingClipId,
        PitchExtractionOptionsDto options, ExtractionObserver observer) {
        const auto requestFingerprint = pitchFingerprint(audioClipId, singingClipId, options);
        return m_dispatcher.dispatchIdempotentDocumentCommandResult<TaskAcceptedResult>(
            OperationIds::extract::pitch::start, context, requestFingerprint,
            [this, context, requestFingerprint, audioClipId, singingClipId, source = context.source,
             options = std::move(options), observer = std::move(observer)](
                DocumentSession &session, const bool validateOnly) mutable {
                auto resolvedAudio = m_objects.audioClip(session, audioClipId);
                if (!resolvedAudio)
                    return AutomationResult<TaskAcceptedResult>(resolvedAudio.getError());
                auto resolvedSinging = m_objects.singingClip(session, singingClipId);
                if (!resolvedSinging)
                    return AutomationResult<TaskAcceptedResult>(resolvedSinging.getError());
                if (!m_services.preparePitch) {
                    return AutomationResult<TaskAcceptedResult>(
                        unavailable(QStringLiteral("Pitch extraction services are unavailable")));
                }

                const auto *audio = static_cast<const AudioClip *>(resolvedAudio.get().clip);
                const auto *singing = static_cast<const SingingClip *>(resolvedSinging.get().clip);
                const auto &timeline = session.model()->timeline();
                const int visibleStartTick = audio->start() + audio->clipStart();
                const double visibleStartMs = timeline.tickToMs(visibleStartTick);
                const double trimStartMs = audio->hasRealTimeAnchor()
                                               ? audio->trimStartMs()
                                               : visibleStartMs - timeline.tickToMs(audio->start());
                const double visibleLengthMs =
                    audio->hasRealTimeAnchor()
                        ? audio->playLengthMs()
                        : timeline.tickToMs(visibleStartTick + audio->clipLen()) - visibleStartMs;
                PitchExtractionInput input;
                input.audioClipId = audioClipId;
                input.singingClipId = singingClipId;
                input.audioPath = audio->path();
                input.timeline = timeline;
                input.singingClipStartTick = singing->start();
                input.audioMaterialOriginMs = visibleStartMs - trimStartMs;
                input.audioVisibleStartMs = visibleStartMs;
                input.audioVisibleEndMs = visibleStartMs + visibleLengthMs;
                input.showProgressDialog = source == InvocationSource::TrustedGui;
                input.modelId = options.modelId;
                input.authorizeSource = options.authorizeSource;
                if (options.minimumFrequency || options.maximumFrequency) {
                    return AutomationResult<TaskAcceptedResult>(AutomationError::invalidArgument(
                        QStringLiteral("options"),
                        QStringLiteral("The active pitch extraction backend does not support a "
                                       "custom frequency range")));
                }
                if (input.authorizeSource) {
                    auto authorized = input.authorizeSource(input.audioPath);
                    if (!authorized)
                        return AutomationResult<TaskAcceptedResult>(authorized.getError());
                }

                auto prepared = m_services.preparePitch(std::move(input));
                if (!prepared)
                    return AutomationResult<TaskAcceptedResult>(prepared.getError());
                if (!prepared.get().job) {
                    return AutomationResult<TaskAcceptedResult>(
                        unavailable(QStringLiteral("Pitch extraction job is unavailable")));
                }
                if (validateOnly) {
                    return AutomationResult<TaskAcceptedResult>({
                        .document = session.version(),
                        .validatedOnly = true,
                    });
                }

                auto job = prepared.get().job;
                const auto task = m_tasks.createTask(
                    OperationIds::extract::pitch::start, session.version(),
                    ObjectRef{ObjectKind::Clip, singingClipId.value()},
                    [weak = std::weak_ptr<IExtractionJob>(job)] {
                        if (const auto locked = weak.lock())
                            locked->cancel();
                    },
                    context.clientId);
                m_jobs.insert(task.taskId, {session.version(), job});
                const TaskAcceptedResult accepted{
                    .taskId = task.taskId,
                    .document = session.version(),
                };
                if (!context.idempotencyKey.isEmpty()) {
                    const auto bound = m_tasks.setUnsuccessfulCallback(
                        task.taskId, [this, context, requestFingerprint,
                                      accepted](const AutomationTaskSnapshot &) {
                            m_dispatcher.releaseDocumentIdempotency(
                                OperationIds::extract::pitch::start, context, requestFingerprint,
                                accepted);
                        });
                    Q_ASSERT(bound);
                }
                auto execute = [this, taskId = task.taskId, base = session.version(),
                                input = std::move(prepared.get().input), job = std::move(job),
                                observer = std::move(observer)]() mutable {
                    executePitchTask(taskId, base, std::move(input), std::move(job),
                                     std::move(observer));
                };
                if (m_services.schedule)
                    m_services.schedule(std::move(execute));
                else
                    execute();
                return AutomationResult<TaskAcceptedResult>(accepted);
            });
    }

    AutomationResult<TaskAcceptedResult> ExtractionAutomationFacade::startMidi(
        const CommandContext &context, const ClipId audioClipId, ExtractionObserver observer) {
        return startMidi(context, audioClipId, {}, std::move(observer));
    }

    AutomationResult<TaskAcceptedResult> ExtractionAutomationFacade::startMidi(
        const CommandContext &context, const ClipId audioClipId, MidiExtractionOptionsDto options,
        ExtractionObserver observer) {
        const auto requestFingerprint = midiFingerprint(audioClipId, options);
        return m_dispatcher.dispatchIdempotentDocumentCommandResult<TaskAcceptedResult>(
            OperationIds::extract::midi::start, context, requestFingerprint,
            [this, context, requestFingerprint, audioClipId, source = context.source,
             options = std::move(options), observer = std::move(observer)](
                DocumentSession &session, const bool validateOnly) mutable {
                auto resolvedAudio = m_objects.audioClip(session, audioClipId);
                if (!resolvedAudio)
                    return AutomationResult<TaskAcceptedResult>(resolvedAudio.getError());
                if (!m_services.prepareMidi) {
                    return AutomationResult<TaskAcceptedResult>(
                        unavailable(QStringLiteral("MIDI extraction services are unavailable")));
                }

                const auto *audio = static_cast<const AudioClip *>(resolvedAudio.get().clip);
                MidiExtractionInput input;
                input.audioClipId = audioClipId;
                input.audioPath = audio->path();
                input.timeline = session.model()->timeline();
                input.audioClipStartTick = audio->start();
                input.audioClipLengthTick = audio->length();
                input.modelId = options.modelId;
                input.defaultLanguage = options.defaultLanguage;
                input.defaultLyric = options.defaultLyric;
                input.clientRef = options.clientRef;
                input.minimumNoteLength = options.minimumNoteLength;
                input.destinationMode = options.destinationMode;
                input.targetTrackId = options.targetTrackId;
                input.targetClipId = options.targetClipId;
                input.targetStart = options.targetStart;
                if (input.destinationMode == QStringLiteral("create_clip")) {
                    if (!input.targetTrackId) {
                        return AutomationResult<TaskAcceptedResult>(
                            AutomationError::invalidArgument(
                                QStringLiteral("destination.target_track_id"),
                                QStringLiteral("A target track is required when creating a clip")));
                    }
                    auto target = m_objects.track(session, *input.targetTrackId);
                    if (!target)
                        return AutomationResult<TaskAcceptedResult>(target.getError());
                } else if (input.destinationMode == QStringLiteral("merge_into_clip")) {
                    if (!input.targetTrackId || !input.targetClipId) {
                        return AutomationResult<TaskAcceptedResult>(
                            AutomationError::invalidArgument(
                                QStringLiteral("destination.target_clip_id"),
                                QStringLiteral(
                                    "A target track and clip are required when merging notes")));
                    }
                    auto target = m_objects.singingClip(session, *input.targetClipId);
                    if (!target)
                        return AutomationResult<TaskAcceptedResult>(target.getError());
                    auto targetTrack = m_objects.track(session, *input.targetTrackId);
                    if (!targetTrack)
                        return AutomationResult<TaskAcceptedResult>(targetTrack.getError());
                    if (target.get().track != targetTrack.get()) {
                        return AutomationResult<TaskAcceptedResult>(
                            AutomationError::invalidArgument(
                                QStringLiteral("destination.target_track_id"),
                                QStringLiteral(
                                    "The target clip does not belong to the target track")));
                    }
                }
                input.showProgressDialog = source == InvocationSource::TrustedGui;
                input.authorizeSource = options.authorizeSource;
                if (input.authorizeSource) {
                    auto authorized = input.authorizeSource(input.audioPath);
                    if (!authorized)
                        return AutomationResult<TaskAcceptedResult>(authorized.getError());
                }

                auto prepared = m_services.prepareMidi(std::move(input));
                if (!prepared)
                    return AutomationResult<TaskAcceptedResult>(prepared.getError());
                if (!prepared.get().job) {
                    return AutomationResult<TaskAcceptedResult>(
                        unavailable(QStringLiteral("MIDI extraction job is unavailable")));
                }
                if (validateOnly) {
                    return AutomationResult<TaskAcceptedResult>({
                        .document = session.version(),
                        .validatedOnly = true,
                    });
                }

                auto job = prepared.get().job;
                const auto task = m_tasks.createTask(
                    OperationIds::extract::midi::start, session.version(),
                    ObjectRef{ObjectKind::Clip, audioClipId.value()},
                    [weak = std::weak_ptr<IExtractionJob>(job)] {
                        if (const auto locked = weak.lock())
                            locked->cancel();
                    },
                    context.clientId);
                m_jobs.insert(task.taskId, {session.version(), job});
                const TaskAcceptedResult accepted{
                    .taskId = task.taskId,
                    .document = session.version(),
                };
                if (!context.idempotencyKey.isEmpty()) {
                    const auto bound = m_tasks.setUnsuccessfulCallback(
                        task.taskId, [this, context, requestFingerprint,
                                      accepted](const AutomationTaskSnapshot &) {
                            m_dispatcher.releaseDocumentIdempotency(
                                OperationIds::extract::midi::start, context, requestFingerprint,
                                accepted);
                        });
                    Q_ASSERT(bound);
                }
                auto execute = [this, taskId = task.taskId, base = session.version(),
                                input = std::move(prepared.get().input), job = std::move(job),
                                observer = std::move(observer)]() mutable {
                    executeMidiTask(taskId, base, std::move(input), std::move(job),
                                    std::move(observer));
                };
                if (m_services.schedule)
                    m_services.schedule(std::move(execute));
                else
                    execute();
                return AutomationResult<TaskAcceptedResult>(accepted);
            });
    }

    void ExtractionAutomationFacade::executePitchTask(const TaskId &taskId,
                                                      DocumentVersion baseDocument,
                                                      PitchExtractionInput input,
                                                      std::shared_ptr<IPitchExtractionJob> job,
                                                      ExtractionObserver observer) {
        if (m_tasks.isCancellationRequested(taskId)) {
            m_tasks.cancel(taskId);
            notifyFinished(taskId, baseDocument.documentId, observer);
            return;
        }
        if (!m_tasks.markRunning(taskId))
            return;
        if (input.authorizeSource) {
            auto authorized = input.authorizeSource(input.audioPath);
            if (!authorized) {
                m_tasks.fail(taskId, taskError(authorized.getError(), taskId,
                                               OperationIds::extract::pitch::start));
                notifyFinished(taskId, baseDocument.documentId, observer);
                return;
            }
        }
        ExtractionJobCallbacks callbacks;
        callbacks.progress = [this, taskId](AutomationTaskProgress progress,
                                            const QString &message) {
            m_tasks.updateProgress(taskId, std::move(progress), message);
        };
        callbacks.cancelRequested = [this, documentId = baseDocument.documentId, taskId] {
            m_tasks.requestCancel(documentId, taskId);
        };
        job->start(std::move(callbacks),
                   [this, taskId, baseDocument, input = std::move(input),
                    observer = std::move(observer)](PitchExtractionBackendResult result) mutable {
                       completePitchTask(taskId, baseDocument, input, std::move(result), observer);
                   });
    }

    void ExtractionAutomationFacade::executeMidiTask(const TaskId &taskId,
                                                     DocumentVersion baseDocument,
                                                     MidiExtractionInput input,
                                                     std::shared_ptr<IMidiExtractionJob> job,
                                                     ExtractionObserver observer) {
        if (m_tasks.isCancellationRequested(taskId)) {
            m_tasks.cancel(taskId);
            notifyFinished(taskId, baseDocument.documentId, observer);
            return;
        }
        if (!m_tasks.markRunning(taskId))
            return;
        if (input.authorizeSource) {
            auto authorized = input.authorizeSource(input.audioPath);
            if (!authorized) {
                m_tasks.fail(taskId, taskError(authorized.getError(), taskId,
                                               OperationIds::extract::midi::start));
                notifyFinished(taskId, baseDocument.documentId, observer);
                return;
            }
        }
        ExtractionJobCallbacks callbacks;
        callbacks.progress = [this, taskId](AutomationTaskProgress progress,
                                            const QString &message) {
            m_tasks.updateProgress(taskId, std::move(progress), message);
        };
        callbacks.cancelRequested = [this, documentId = baseDocument.documentId, taskId] {
            m_tasks.requestCancel(documentId, taskId);
        };
        job->start(std::move(callbacks),
                   [this, taskId, baseDocument, input = std::move(input),
                    observer = std::move(observer)](MidiExtractionBackendResult result) mutable {
                       completeMidiTask(taskId, baseDocument, input, std::move(result), observer);
                   });
    }

    void ExtractionAutomationFacade::completePitchTask(const TaskId &taskId,
                                                       const DocumentVersion &baseDocument,
                                                       const PitchExtractionInput &input,
                                                       PitchExtractionBackendResult result,
                                                       const ExtractionObserver &observer) {
        if (result.state == ExtractionBackendState::Canceled ||
            m_tasks.isCancellationRequested(taskId)) {
            m_tasks.cancel(taskId);
            notifyFinished(taskId, baseDocument.documentId, observer);
            return;
        }
        if (result.state == ExtractionBackendState::Failed) {
            m_tasks.fail(taskId, backendError(taskId, OperationIds::extract::pitch::start,
                                              result.errorCode, std::move(result.errorMessage)));
            notifyFinished(taskId, baseDocument.documentId, observer);
            return;
        }

        auto curves = pitchCurves(input, result.segments);
        if (!curves) {
            m_tasks.fail(taskId,
                         taskError(curves.getError(), taskId, OperationIds::extract::pitch::start));
            notifyFinished(taskId, baseDocument.documentId, observer);
            return;
        }
        CommandContext context{
            .expected = baseDocument,
            .validateOnly = true,
            .source = InvocationSource::InternalAutomation,
        };
        const auto validation = m_parameters.replaceParameter(
            context, input.singingClipId, ParamInfo::Pitch, Param::Edited, curves.get());
        if (!validation) {
            m_tasks.fail(taskId, taskError(validation.getError(), taskId,
                                           OperationIds::extract::pitch::start));
            notifyFinished(taskId, baseDocument.documentId, observer);
            return;
        }
        if (m_tasks.isCancellationRequested(taskId)) {
            m_tasks.cancel(taskId);
            notifyFinished(taskId, baseDocument.documentId, observer);
            return;
        }
        const auto committing = m_tasks.beginCommitting(taskId);
        if (!committing || !committing.get()) {
            if (!committing) {
                m_tasks.fail(taskId, taskError(committing.getError(), taskId,
                                               OperationIds::extract::pitch::start));
            }
            notifyFinished(taskId, baseDocument.documentId, observer);
            return;
        }
        context.validateOnly = false;
        const auto committed = m_parameters.replaceParameter(
            context, input.singingClipId, ParamInfo::Pitch, Param::Edited, curves.get());
        if (committed)
            m_tasks.succeed(taskId, committed.get());
        else
            m_tasks.fail(taskId, taskError(committed.getError(), taskId,
                                           OperationIds::extract::pitch::start));
        notifyFinished(taskId, baseDocument.documentId, observer);
    }

    void ExtractionAutomationFacade::completeMidiTask(const TaskId &taskId,
                                                      const DocumentVersion &baseDocument,
                                                      const MidiExtractionInput &input,
                                                      MidiExtractionBackendResult result,
                                                      const ExtractionObserver &observer) {
        if (result.state == ExtractionBackendState::Canceled ||
            m_tasks.isCancellationRequested(taskId)) {
            m_tasks.cancel(taskId);
            notifyFinished(taskId, baseDocument.documentId, observer);
            return;
        }
        if (result.state == ExtractionBackendState::Failed) {
            m_tasks.fail(taskId, backendError(taskId, OperationIds::extract::midi::start,
                                              result.errorCode, std::move(result.errorMessage)));
            notifyFinished(taskId, baseDocument.documentId, observer);
            return;
        }

        QList<NoteDraftDto> extractedNotes;
        int createdNoteIndex = 0;
        for (const auto &note : result.notes) {
            if (note.localStart < 0 ||
                (input.minimumNoteLength && note.length < *input.minimumNoteLength))
                continue;
            extractedNotes.append({
                .clientRef =
                    input.clientRef.isEmpty()
                        ? QString()
                        : input.clientRef + QStringLiteral("/notes/%1").arg(createdNoteIndex++),
                .localStart = note.localStart,
                .length = note.length,
                .keyIndex = note.keyIndex,
                .lyric = input.defaultLyric,
                .language = input.defaultLanguage,
            });
        }

        const auto project = m_project.getProject(baseDocument.documentId);
        if (!project) {
            m_tasks.fail(taskId,
                         taskError(project.getError(), taskId, OperationIds::extract::midi::start));
            notifyFinished(taskId, baseDocument.documentId, observer);
            return;
        }
        TrackDraftDto track;
        ClipDraftDto clip;
        QList<ClipInsertDto> insertedClips;
        if (input.destinationMode != QStringLiteral("merge_into_clip")) {
            clip.clientRef = input.clientRef;
            clip.type = ClipDraftDto::Type::Singing;
            clip.properties.start =
                input.destinationMode.isEmpty() ? input.audioClipStartTick : input.targetStart;
            clip.properties.length = input.audioClipLengthTick;
            clip.properties.clipLen = input.audioClipLengthTick;
            clip.defaultLanguage = input.defaultLanguage;
            clip.notes = extractedNotes;
            if (input.targetTrackId) {
                insertedClips.append({*input.targetTrackId, clip});
            } else {
                if (!input.clientRef.isEmpty())
                    track.clientRef = input.clientRef + QStringLiteral("/track");
                track.name = QFileInfo(input.audioPath).baseName();
                track.defaultLanguage = input.defaultLanguage;
                track.clips.append(clip);
            }
        } else {
            int targetClipStart = -1;
            for (const auto &candidateTrack : project.get().tracks) {
                for (const auto &candidateClip : candidateTrack.clips) {
                    if (input.targetClipId && candidateClip.id == *input.targetClipId) {
                        targetClipStart = candidateClip.data.properties.start;
                        break;
                    }
                }
                if (targetClipStart >= 0)
                    break;
            }
            if (targetClipStart < 0) {
                m_tasks.fail(
                    taskId,
                    taskError(AutomationError::notFound(
                                  {ObjectKind::Clip, input.targetClipId.value_or(ClipId{}).value()},
                                  QStringLiteral("The target singing clip was not found")),
                              taskId, OperationIds::extract::midi::start));
                notifyFinished(taskId, baseDocument.documentId, observer);
                return;
            }
            const auto offset = input.targetStart - targetClipStart;
            for (auto &note : extractedNotes)
                note.localStart += offset;
        }

        const auto apply = [&](const CommandContext &context) -> AutomationResult<MutationResult> {
            if (input.destinationMode == QStringLiteral("merge_into_clip"))
                return m_notes.insertNotes(context, *input.targetClipId, extractedNotes);
            if (input.targetTrackId)
                return m_project.insertClips(context, insertedClips);
            return m_project.insertTrack(context, project.get().tracks.size(), track);
        };
        CommandContext context{
            .expected = baseDocument,
            .validateOnly = true,
            .source = InvocationSource::InternalAutomation,
        };
        const auto validation = apply(context);
        if (!validation) {
            m_tasks.fail(taskId, taskError(validation.getError(), taskId,
                                           OperationIds::extract::midi::start));
            notifyFinished(taskId, baseDocument.documentId, observer);
            return;
        }
        if (m_tasks.isCancellationRequested(taskId)) {
            m_tasks.cancel(taskId);
            notifyFinished(taskId, baseDocument.documentId, observer);
            return;
        }
        const auto committing = m_tasks.beginCommitting(taskId);
        if (!committing || !committing.get()) {
            if (!committing) {
                m_tasks.fail(taskId, taskError(committing.getError(), taskId,
                                               OperationIds::extract::midi::start));
            }
            notifyFinished(taskId, baseDocument.documentId, observer);
            return;
        }
        context.validateOnly = false;
        const auto committed = apply(context);
        if (committed)
            m_tasks.succeed(taskId, committed.get());
        else
            m_tasks.fail(taskId, taskError(committed.getError(), taskId,
                                           OperationIds::extract::midi::start));
        notifyFinished(taskId, baseDocument.documentId, observer);
    }

    void ExtractionAutomationFacade::notifyFinished(const TaskId &taskId,
                                                    const DocumentId &documentId,
                                                    const ExtractionObserver &observer) {
        if (!m_jobs.contains(taskId))
            return;
        if (observer.finished) {
            const auto task = m_tasks.get(documentId, taskId);
            if (task)
                observer.finished(task.get());
        }
        m_jobs.remove(taskId);
    }

    void ExtractionAutomationFacade::discardDocumentGeneration(const DocumentId &documentId) {
        for (auto it = m_jobs.begin(); it != m_jobs.end();) {
            if (it->baseDocument.documentId != documentId) {
                ++it;
                continue;
            }
            it->job->cancel();
            it = m_jobs.erase(it);
        }
    }

} // namespace Automation
