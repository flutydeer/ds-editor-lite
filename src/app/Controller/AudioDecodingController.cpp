#include "AudioDecodingController.h"

#include <QFileInfo>
#include <QTimer>
#include <QScopeGuard>
#include <QScopedValueRollback>

#include <optional>
#include <utility>

#include <TalcsFormat/FormatManager.h>

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Automation/OperationIds.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowPathUtils.h"
#include "Controller/Tasks/DocumentTaskCompletion.h"
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include "Modules/Audio/AudioContext.h"
#include <lite/Tasking/TaskManager.h>
#include "Tasks/DecodeAudioTask.h"
#include "Tasks/ResolveAudioPathTask.h"

namespace {
    Automation::CoreRuntime *automationRuntime() {
        return AppContext::instance<Automation::CoreRuntime>();
    }

    bool interactiveAudioCheck() {
        auto *runtime = automationRuntime();
        return runtime && runtime->dispatcher().currentInvocationSource() ==
                              Automation::InvocationSource::TrustedGui;
    }

    Automation::CommandContext commandContext(const Automation::DocumentVersion &document,
                                              const bool validateOnly = false) {
        return {
            .expected = document,
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::InternalAutomation,
        };
    }

    bool isLoadableAudioPath(const QString &path) {
        const QFileInfo file(path);
        return file.isAbsolute() && file.isFile();
    }

    Automation::AutomationError audioPathNotFound(const Automation::OperationId &operationId) {
        return {
            .code = Automation::AutomationErrorCode::FileNotFound,
            .message = QStringLiteral("No matching audio file was found"),
            .operationId = operationId,
        };
    }

}

AudioDecodingController::AudioDecodingController(QObject *parent) : QObject(parent) {
}

AudioDecodingController::~AudioDecodingController() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(AudioDecodingController)

void AudioDecodingController::setGuiServices(DocumentWorkflowController *workflow,
                                             std::function<void(const QString &)> notifier) {
    m_documentWorkflow = workflow;
    m_notifier = std::move(notifier);
}

AppGlobal::AudioClipLoadState AudioDecodingController::loadState(const AudioClip *clip) const {
    if (!clip)
        return AppGlobal::AudioClipLoadState::Loaded;
    // A file the resolver already proved missing keeps its permanent hint while later cascade
    // retries run, otherwise every retry over an already-missing clip would flash the loading
    // placeholder back in.
    if (clip->pathStatus() == AudioClip::PathStatus::Missing)
        return AppGlobal::AudioClipLoadState::Missing;
    if (m_inFlightByClip.contains(clip->id()))
        return AppGlobal::AudioClipLoadState::Loading;
    return AppGlobal::AudioClipLoadState::Loaded;
}

void AudioDecodingController::beginDecode(const int clipId, DecodeAudioTask *task) {
    m_inFlightByClip[clipId].decode = task;
    refreshLoadState(clipId);
}

void AudioDecodingController::endDecode(const int clipId, DecodeAudioTask *task) {
    const auto it = m_inFlightByClip.find(clipId);
    // A superseded task (relink or undo while its predecessor was in flight) finishes later and
    // must not clear the state its successor just raised.
    if (it == m_inFlightByClip.end() || it->decode != task)
        return;
    it->decode = nullptr;
    if (!it->any())
        m_inFlightByClip.erase(it);
    refreshLoadState(clipId);
}

void AudioDecodingController::beginResolve(const int clipId, ResolveAudioPathTask *task) {
    m_inFlightByClip[clipId].resolve = task;
    refreshLoadState(clipId);
}

void AudioDecodingController::endResolve(const int clipId, ResolveAudioPathTask *task) {
    const auto it = m_inFlightByClip.find(clipId);
    if (it == m_inFlightByClip.end() || it->resolve != task)
        return;
    it->resolve = nullptr;
    if (!it->any())
        m_inFlightByClip.erase(it);
    refreshLoadState(clipId);
}

void AudioDecodingController::refreshLoadState(const int clipId) {
    const auto *clip = qobject_cast<const AudioClip *>(appModel->findClipById(clipId));
    if (!clip) {
        // The clip is gone (removed, or the document was replaced): forget its last state rather
        // than reporting one for an id that no longer exists.
        m_lastLoadState.remove(clipId);
        return;
    }
    const auto state = loadState(clip);
    const auto it = m_lastLoadState.constFind(clipId);
    if (it != m_lastLoadState.constEnd() && it.value() == state)
        return;
    m_lastLoadState.insert(clipId, state);
    emit loadStateChanged(clipId, state);
}

void AudioDecodingController::onModelChanged() {
    // Terminate all decoding tasks
    for (const auto task : m_tasks) {
        taskManager->terminateTask(task);
    }
    for (const auto task : m_resolveTasks) {
        taskManager->terminateTask(task);
    }
    // The clips those tasks described are gone with the replaced document. Their completion slots
    // still run later and must not report a state change for an unrelated new clip with the same
    // id, so drop both maps without emitting.
    m_inFlightByClip.clear();
    m_lastLoadState.clear();

    // Reset aggregated path resolution state
    m_pendingResolveCount = 0;
    m_missingClipIds.clear();
    m_unconfirmedClipIds.clear();
    m_autoRelocatedCount = 0;

    for (const auto track : appModel->tracks()) {
        connect(track, &Track::clipChanged, this, &AudioDecodingController::onClipChanged);
        for (const auto clip : track->clips()) {
            if (clip->clipType() == Clip::Audio)
                connectClip(static_cast<AudioClip *>(clip));
        }
    }

    // Document identity is committed after the model replacement signal, so defer until the
    // current event-loop iteration finishes before resolving project-relative audio paths.
    const auto modelChangeEpoch = ++m_modelChangeEpoch;
    const auto interactive = interactiveAudioCheck();
    QTimer::singleShot(0, this, [this, modelChangeEpoch, interactive] {
        if (modelChangeEpoch == m_modelChangeEpoch)
            startDecodingAndResolving(interactive);
    });
}

void AudioDecodingController::startDecodingAndResolving(const bool interactive) {
    for (const auto track : appModel->tracks()) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() != Clip::Audio)
                continue;
            startDecodingOrResolving(static_cast<AudioClip *>(clip), true, interactive);
        }
    }
}

void AudioDecodingController::onTrackChanged(const AppModel::TrackChangeType type,
                                             const qsizetype index, const Track *track) {
    Q_UNUSED(index);
    if (type == AppModel::Insert) {
        connect(track, &Track::clipChanged, this, &AudioDecodingController::onClipChanged);
        for (auto *clip : track->clips()) {
            if (clip->clipType() != Clip::Audio)
                continue;
            auto *audioClip = static_cast<AudioClip *>(clip);
            connectClip(audioClip);
            startDecodingOrResolving(audioClip, false, interactiveAudioCheck());
        }
    } else if (type == AppModel::Remove) {
        disconnect(track, nullptr, this, nullptr);
        for (auto *clip : track->clips()) {
            if (clip->clipType() == Clip::Audio)
                disconnect(static_cast<AudioClip *>(clip), nullptr, this, nullptr);
        }
        terminateTasksByTrackId(track->id());
    }
}

void AudioDecodingController::onClipChanged(const Track::ClipChangeType type, Clip *clip) {
    if (type == Track::Inserted) {
        if (clip->clipType() == Clip::Audio) {
            const auto audioClip = static_cast<AudioClip *>(clip);
            connectClip(audioClip);
            startDecodingOrResolving(audioClip, false, interactiveAudioCheck());
        }
    } else if (type == Track::Removed) {
        if (clip->clipType() == Clip::Audio) {
            disconnect(static_cast<AudioClip *>(clip), nullptr, this, nullptr);
            terminateTaskByClipId(clip->id());
        }
    }
}

void AudioDecodingController::connectClip(AudioClip *clip) {
    // Re-decode the waveform after relink/replace (including undo)
    connect(clip, &AudioClip::sourceChanged, this, [clip, this] {
        terminateTaskByClipId(clip->id());
        auto *runtime = automationRuntime();
        if (!runtime)
            return;
        const auto interactive = m_sourceChangeContext.clip == clip
                                     ? m_sourceChangeContext.interactive
                                     : interactiveAudioCheck();
        startDecodingOrResolving(clip, true, interactive);
    });
    // PathStatus is written from more than one place (resolution, the audio resource check, undo
    // of a path edit), so re-derive the load state on every change instead of making each writer
    // remember to refresh it.
    connect(clip, &AudioClip::pathStatusChanged, this,
            [this, clipId = clip->id()](const AudioClip::PathStatus) { refreshLoadState(clipId); });
}

void AudioDecodingController::startDecodingOrResolving(AudioClip *clip, const bool forceDecode,
                                                       const bool interactive) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    if (isLoadableAudioPath(clip->path())) {
        if (forceDecode || clip->audioInfo().peakCache.isEmpty() ||
            clip->pathStatus() != AudioClip::PathStatus::Normal)
            createAndStartTask(clip, interactive);
        return;
    }
    m_pendingResolveCount++;
    createAndStartResolveTask(clip, interactive);
}

void AudioDecodingController::createAndStartTask(AudioClip *clip, const bool interactive) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    auto decodeTask = new DecodeAudioTask;
    decodeTask->clipId = clip->id();
    Track *track = nullptr;
    appModel->findClipById(clip->id(), track);
    decodeTask->trackId = track ? track->id() : -1;
    decodeTask->documentVersion = runtime->documentVersion();
    decodeTask->assetSnapshot = Automation::audioAssetSnapshotDto(*clip);
    decodeTask->path = decodeTask->assetSnapshot.path;
    decodeTask->workspace = decodeTask->assetSnapshot.formatData;

    QVariant userData;
    QDataStream o(
        QByteArray::fromBase64(decodeTask->workspace.value("userData").toString().toLatin1()));
    o >> userData;
    const auto entryClassName = decodeTask->workspace.value("entryClassName").toString();
    decodeTask->io = AudioContext::instance()->formatManager()->getFormatLoad(
        decodeTask->path, userData, entryClassName);
    const auto automationTask = runtime->automationTasks().createTask(
        Automation::OperationIds::audio_clips::apply_decode_cache, decodeTask->documentVersion,
        Automation::ObjectRef{Automation::ObjectKind::Clip, decodeTask->clipId},
        [decodeTask] { decodeTask->terminate(); });
    decodeTask->automationTaskId = automationTask.taskId;
    runtime->automationTasks().markRunning(automationTask.taskId);

    m_tasks.append(decodeTask);
    beginDecode(clip->id(), decodeTask);
    connect(
        decodeTask, &Task::finished, this,
        [decodeTask, this, interactive] { handleTaskFinished(decodeTask, interactive); },
        Qt::QueuedConnection);
    taskManager->addTask(decodeTask);
    taskManager->startTask(decodeTask);
}

void AudioDecodingController::createAndStartResolveTask(AudioClip *clip, const bool interactive) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    const auto resolveTask = new ResolveAudioPathTask;
    resolveTask->clipId = clip->id();
    Track *track = nullptr;
    appModel->findClipById(clip->id(), track);
    resolveTask->trackId = track ? track->id() : -1;
    resolveTask->documentVersion = runtime->documentVersion();
    resolveTask->assetSnapshot = Automation::audioAssetSnapshotDto(*clip);
    resolveTask->originalPath = resolveTask->assetSnapshot.path;
    resolveTask->relativeDir = resolveTask->assetSnapshot.pathInfo.relativeDir;
    resolveTask->fileName = QFileInfo(resolveTask->assetSnapshot.path).fileName();
    resolveTask->expectedSha512 = resolveTask->assetSnapshot.pathInfo.sha512;
    resolveTask->referenceDir = clip->referenceDirectory();
    const auto projectPath = runtime->documentPath();
    resolveTask->projectDir =
        projectPath.isEmpty() ? QString{} : QFileInfo(projectPath).absolutePath();
    const auto automationTask = runtime->automationTasks().createTask(
        Automation::OperationIds::audio_clips::apply_resolved_path, resolveTask->documentVersion,
        Automation::ObjectRef{Automation::ObjectKind::Clip, resolveTask->clipId},
        [resolveTask] { resolveTask->terminate(); });
    resolveTask->automationTaskId = automationTask.taskId;
    runtime->automationTasks().markRunning(automationTask.taskId);

    m_resolveTasks.append(resolveTask);
    beginResolve(clip->id(), resolveTask);
    connect(
        resolveTask, &Task::finished, this,
        [resolveTask, this, interactive] { handleResolveTaskFinished(resolveTask, interactive); },
        Qt::QueuedConnection);
    taskManager->addTask(resolveTask);
    taskManager->startTask(resolveTask);
}

void AudioDecodingController::handleResolveTaskFinished(ResolveAudioPathTask *task,
                                                        const bool interactive) {
    if (DocumentTaskCompletion::deferCompletionWhileDocumentBusy(
            automationRuntime(), m_documentWorkflow.data(), task, this,
            [this, interactive](ResolveAudioPathTask *deferredTask) {
                handleResolveTaskFinished(deferredTask, interactive);
            }))
        return;

    const auto terminated = task->terminated();
    const bool cascade = task->mode == ResolveAudioPathTask::Mode::Cascade;
    taskManager->removeTask(task);
    m_resolveTasks.removeOne(task);
    // The resolution entry must outlive the writeback: a hit starts the decode synchronously inside
    // the commit, so clearing the entry first would report Loaded for an instant. The guard matches
    // on the pointer alone and never dereferences it, which matters because every path below
    // deletes the task before returning.
    const auto endResolution =
        qScopeGuard([this, clipId = task->clipId, task] { endResolve(clipId, task); });

    auto *runtime = automationRuntime();
    if (!runtime) {
        delete task;
        return;
    }

    const bool currentGeneration =
        runtime->documentVersion().documentId == task->documentVersion.documentId;
    // Only the project-load / relink pass feeds the aggregated session notification, the cascade
    // pass runs inside an already-finished session.
    if (!cascade && currentGeneration && m_pendingResolveCount > 0)
        m_pendingResolveCount--;

    if (terminated) {
        runtime->automationTasks().cancel(task->automationTaskId);
        delete task;
        if (!cascade && currentGeneration)
            finishResolveIfSessionDone();
        return;
    }

    AudioClip *targetClip = nullptr;
    if (currentGeneration) {
        if (auto *clip = appModel->findClipById(task->clipId);
            clip && clip->clipType() == Clip::Audio)
            targetClip = static_cast<AudioClip *>(clip);
    }

    if (cascade && task->result != ResolveAudioPathTask::Result::HitSibling) {
        // Cascade only adopts a hash-verified sibling hit, anything else leaves the clip missing
        runtime->automationTasks().fail(
            task->automationTaskId,
            audioPathNotFound(Automation::OperationIds::audio_clips::apply_resolved_path));
        delete task;
        return;
    }
    if (!cascade) {
        // The project moved, or the clip's identity inputs changed while the task ran: the result
        // no longer describes this clip, so restart from the current state instead of writing it.
        const auto currentProjectPath = runtime->documentPath();
        const auto currentProjectDir =
            currentProjectPath.isEmpty() ? QString{} : QFileInfo(currentProjectPath).absolutePath();
        const auto currentAsset =
            targetClip ? std::optional(Automation::audioAssetSnapshotDto(*targetClip))
                       : std::nullopt;
        const bool projectDirectoryChanged =
            currentGeneration &&
            !DocumentWorkflowPathUtils::projectPathsEqual(task->projectDir, currentProjectDir);
        const bool resolveInputsChanged =
            currentAsset &&
            (currentAsset->pathInfo.relativeDir != task->assetSnapshot.pathInfo.relativeDir ||
             currentAsset->pathInfo.sha512 != task->assetSnapshot.pathInfo.sha512);
        if (projectDirectoryChanged || resolveInputsChanged) {
            runtime->automationTasks().cancel(task->automationTaskId);
            if (currentAsset &&
                currentAsset->sourceGeneration == task->assetSnapshot.sourceGeneration &&
                currentAsset->path == task->assetSnapshot.path)
                startDecodingOrResolving(targetClip, true, interactive);
            delete task;
            finishResolveIfSessionDone();
            return;
        }
    }

    // Both passes share the writeback, they differ only in which results they accept (handled
    // above) and how they report. The context is taken by value because the validation pass and
    // the commit pass differ only in validateOnly.
    const auto applyResult = [runtime, task](Automation::CommandContext context)
        -> Automation::AutomationResult<Automation::MutationResult> {
        switch (task->result) {
            case ResolveAudioPathTask::Result::HitRelative:
            case ResolveAudioPathTask::Result::HitSibling:
                return runtime->project().applyResolvedAudioPath(
                    context, Automation::ClipId(task->clipId), task->assetSnapshot,
                    task->resolvedPath, AudioClip::PathStatus::Normal);
            case ResolveAudioPathTask::Result::HitUnconfirmed:
                return runtime->project().applyResolvedAudioPath(
                    context, Automation::ClipId(task->clipId), task->assetSnapshot,
                    task->resolvedPath, AudioClip::PathStatus::Unconfirmed);
            case ResolveAudioPathTask::Result::Miss:
                return runtime->project().setAudioClipPathStatus(
                    context, Automation::ClipId(task->clipId), task->assetSnapshot,
                    AudioClip::PathStatus::Missing);
        }
        return Automation::AutomationResult<Automation::MutationResult>(Automation::AutomationError{
            .code = Automation::AutomationErrorCode::InternalError,
            .message = QStringLiteral("Audio path resolution did not produce a result"),
        });
    };

    auto contextResult = runtime->derivedWritebackContext(task->documentVersion, true);
    if (!contextResult) {
        runtime->automationTasks().fail(task->automationTaskId, contextResult.getError());
        delete task;
        if (!cascade && currentGeneration)
            finishResolveIfSessionDone();
        return;
    }
    auto context = contextResult.get();
    const auto validation = applyResult(context);
    if (!validation) {
        runtime->automationTasks().fail(task->automationTaskId, validation.getError());
        delete task;
        if (!cascade && currentGeneration)
            finishResolveIfSessionDone();
        return;
    }
    const auto committing = runtime->automationTasks().beginCommitting(task->automationTaskId);
    if (!committing || !committing.get()) {
        if (committing)
            runtime->automationTasks().cancel(task->automationTaskId);
        delete task;
        if (!cascade && currentGeneration)
            finishResolveIfSessionDone();
        return;
    }
    context.validateOnly = false;
    const QScopedValueRollback sourceChangeScope(m_sourceChangeContext,
                                                 SourceChangeContext{targetClip, interactive});
    const auto result = applyResult(context);
    if (result) {
        if (task->result == ResolveAudioPathTask::Result::Miss)
            runtime->automationTasks().fail(
                task->automationTaskId,
                audioPathNotFound(Automation::OperationIds::audio_clips::apply_resolved_path));
        else
            runtime->automationTasks().succeed(task->automationTaskId, result.get());
        if (cascade) {
            emit clipRelocated(task->clipId, task->resolvedPath);
        } else if (currentGeneration && interactive) {
            if (task->result == ResolveAudioPathTask::Result::HitUnconfirmed)
                m_unconfirmedClipIds.append(task->clipId);
            else if (task->result == ResolveAudioPathTask::Result::Miss)
                m_missingClipIds.append(task->clipId);
            else
                m_autoRelocatedCount++;
        }
    } else {
        runtime->automationTasks().fail(task->automationTaskId, result.getError());
    }
    delete task;
    if (!cascade && currentGeneration)
        finishResolveIfSessionDone();
}

void AudioDecodingController::finishResolveIfSessionDone() {
    if (m_pendingResolveCount > 0)
        return;
    if (m_missingClipIds.isEmpty() && m_unconfirmedClipIds.isEmpty() && m_autoRelocatedCount == 0)
        return;

    if (m_missingClipIds.isEmpty() && m_unconfirmedClipIds.isEmpty() && m_notifier) {
        m_notifier(tr("%L1 audio file(s) relocated automatically").arg(m_autoRelocatedCount));
    }

    emit resolveSessionFinished(m_missingClipIds, m_unconfirmedClipIds, m_autoRelocatedCount);
    m_missingClipIds.clear();
    m_unconfirmedClipIds.clear();
    m_autoRelocatedCount = 0;
}

void AudioDecodingController::resolveMissingClipsNear(const QString &filePath) {
    const auto candidateDir = QFileInfo(filePath).absolutePath();
    const auto interactive = interactiveAudioCheck();
    auto *runtime = automationRuntime();
    if (candidateDir.isEmpty() || !runtime)
        return;

    for (const auto track : appModel->tracks()) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() != Clip::Audio)
                continue;
            const auto audioClip = static_cast<AudioClip *>(clip);
            if (audioClip->pathStatus() != AudioClip::PathStatus::Missing)
                continue;
            // Without sha512 the identity cannot be verified; skip cascading, user must relink
            // manually
            if (audioClip->pathInfo().sha512.isEmpty())
                continue;

            const auto resolveTask = new ResolveAudioPathTask;
            resolveTask->mode = ResolveAudioPathTask::Mode::Cascade;
            resolveTask->clipId = audioClip->id();
            resolveTask->trackId = track->id();
            resolveTask->documentVersion = runtime->documentVersion();
            resolveTask->assetSnapshot = Automation::audioAssetSnapshotDto(*audioClip);
            resolveTask->originalPath = resolveTask->assetSnapshot.path;
            resolveTask->relativeDir = {};
            resolveTask->fileName = QFileInfo(resolveTask->assetSnapshot.path).fileName();
            resolveTask->expectedSha512 = resolveTask->assetSnapshot.pathInfo.sha512;
            resolveTask->projectDir = candidateDir;
            const auto automationTask = runtime->automationTasks().createTask(
                Automation::OperationIds::audio_clips::apply_resolved_path,
                resolveTask->documentVersion,
                Automation::ObjectRef{Automation::ObjectKind::Clip, resolveTask->clipId},
                [resolveTask] { resolveTask->terminate(); });
            resolveTask->automationTaskId = automationTask.taskId;
            runtime->automationTasks().markRunning(automationTask.taskId);

            m_resolveTasks.append(resolveTask);
            beginResolve(audioClip->id(), resolveTask);
            connect(
                resolveTask, &Task::finished, this,
                [resolveTask, this, interactive] {
                    handleResolveTaskFinished(resolveTask, interactive);
                },
                Qt::QueuedConnection);
            taskManager->addTask(resolveTask);
            taskManager->startTask(resolveTask);
        }
    }
}

void AudioDecodingController::handleTaskFinished(DecodeAudioTask *task, const bool interactive) {
    if (DocumentTaskCompletion::deferCompletionWhileDocumentBusy(
            automationRuntime(), m_documentWorkflow.data(), task, this,
            [this, interactive](DecodeAudioTask *deferredTask) {
                handleTaskFinished(deferredTask, interactive);
            }))
        return;

    const auto terminate = task->terminated();
    taskManager->removeTask(task);
    m_tasks.removeOne(task);
    endDecode(task->clipId, task);
    auto *runtime = automationRuntime();

    if (terminate) {
        if (runtime)
            runtime->automationTasks().cancel(task->automationTaskId);
        delete task;
        return;
    }
    if (!runtime) {
        delete task;
        return;
    }
    if (!task->success) {
        if (!QFileInfo(task->path).isFile()) {
            auto contextResult = runtime->derivedWritebackContext(task->documentVersion, true);
            if (!contextResult) {
                runtime->automationTasks().fail(task->automationTaskId, contextResult.getError());
                delete task;
                return;
            }
            auto context = contextResult.get();
            const auto validation = runtime->project().setAudioClipPathStatus(
                context, Automation::ClipId(task->clipId), task->assetSnapshot,
                AudioClip::PathStatus::Missing);
            if (!validation) {
                runtime->automationTasks().fail(task->automationTaskId, validation.getError());
                delete task;
                return;
            }
            const auto committing =
                runtime->automationTasks().beginCommitting(task->automationTaskId);
            if (!committing || !committing.get()) {
                if (committing)
                    runtime->automationTasks().cancel(task->automationTaskId);
                delete task;
                return;
            }
            context.validateOnly = false;
            const auto status = runtime->project().setAudioClipPathStatus(
                context, Automation::ClipId(task->clipId), task->assetSnapshot,
                AudioClip::PathStatus::Missing);
            if (!status) {
                runtime->automationTasks().fail(task->automationTaskId, status.getError());
                delete task;
                return;
            }
            runtime->automationTasks().fail(
                task->automationTaskId,
                Automation::AutomationError{
                    .code = Automation::AutomationErrorCode::FileNotFound,
                    .message = QStringLiteral("Audio file is missing"),
                    .operationId = Automation::OperationIds::audio_clips::apply_decode_cache,
                });
            delete task;
            return;
        }
        runtime->automationTasks().fail(
            task->automationTaskId,
            Automation::AutomationError{
                .code = Automation::AutomationErrorCode::IoError,
                .message = task->errorMessage,
                .operationId = Automation::OperationIds::audio_clips::apply_decode_cache,
            });
        if (interactive && m_notifier)
            m_notifier(tr("Failed to open audio file: %1").arg(task->path));

        delete task;
        return;
    }

    auto contextResult = runtime->derivedWritebackContext(task->documentVersion, true);
    if (!contextResult) {
        runtime->automationTasks().fail(task->automationTaskId, contextResult.getError());
        delete task;
        return;
    }
    auto context = contextResult.get();
    const auto validation = runtime->project().applyAudioDecodeCache(
        context, Automation::ClipId(task->clipId), task->assetSnapshot, task->result());
    if (!validation) {
        runtime->automationTasks().fail(task->automationTaskId, validation.getError());
        delete task;
        return;
    }
    const auto committing = runtime->automationTasks().beginCommitting(task->automationTaskId);
    if (!committing || !committing.get()) {
        if (committing)
            runtime->automationTasks().cancel(task->automationTaskId);
        delete task;
        return;
    }
    context.validateOnly = false;
    const auto result = runtime->project().applyAudioDecodeCache(
        context, Automation::ClipId(task->clipId), task->assetSnapshot, task->result());
    if (result)
        runtime->automationTasks().succeed(task->automationTaskId, result.get());
    else
        runtime->automationTasks().fail(task->automationTaskId, result.getError());
    delete task;
}

void AudioDecodingController::terminateTaskByClipId(const int clipId) {
    for (const auto task : m_tasks)
        if (task->clipId == clipId)
            taskManager->terminateTask(task);
    for (const auto task : m_resolveTasks)
        if (task->clipId == clipId)
            taskManager->terminateTask(task);
}

void AudioDecodingController::terminateTasksByTrackId(const int trackId) {
    for (auto task : m_tasks) {
        if (task->trackId == trackId)
            taskManager->terminateTask(task);
    }
    for (auto task : m_resolveTasks) {
        if (task->trackId == trackId)
            taskManager->terminateTask(task);
    }
}
