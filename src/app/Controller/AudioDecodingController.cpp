#include "AudioDecodingController.h"

#include <QFileInfo>
#include <QTimer>

#include <optional>

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
#include <lite/GUI/Controls/Toast.h>

namespace {
    Automation::CoreRuntime *automationRuntime() {
        return AppContext::instance<Automation::CoreRuntime>();
    }

    Automation::CommandContext commandContext(const Automation::DocumentVersion &document,
                                              const bool validateOnly = false) {
        return {
            .expected = document,
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::TrustedGui,
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

void AudioDecodingController::onModelChanged() {
    // Terminate all decoding tasks
    for (const auto task : m_tasks) {
        taskManager->terminateTask(task);
    }
    for (const auto task : m_resolveTasks) {
        taskManager->terminateTask(task);
    }

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

    // Defer resolving and decoding until the current event loop iteration ends:
    // in commitReplace, replaceProject (which emits modelChanged) runs before
    // updateProjectIdentity, so documentWorkflowController->projectPath() is not yet updated at
    // that point; projectPath only becomes available after commitReplace has fully completed
    const auto modelChangeEpoch = ++m_modelChangeEpoch;
    QTimer::singleShot(0, this, [this, modelChangeEpoch] {
        if (modelChangeEpoch == m_modelChangeEpoch)
            startDecodingAndResolving();
    });
}

void AudioDecodingController::startDecodingAndResolving() {
    for (const auto track : appModel->tracks()) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() != Clip::Audio)
                continue;
            startDecodingOrResolving(static_cast<AudioClip *>(clip), true);
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
            startDecodingOrResolving(audioClip, false);
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
            startDecodingOrResolving(audioClip, false);
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
        startDecodingOrResolving(clip, true);
    });
}

void AudioDecodingController::startDecodingOrResolving(AudioClip *clip, const bool forceDecode) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    if (isLoadableAudioPath(clip->path())) {
        if (forceDecode || clip->audioInfo().peakCache.isEmpty() ||
            clip->pathStatus() != AudioClip::PathStatus::Normal)
            createAndStartTask(clip);
        return;
    }
    m_pendingResolveCount++;
    createAndStartResolveTask(clip);
}

void AudioDecodingController::createAndStartTask(AudioClip *clip) {
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
    connect(decodeTask, &Task::finished, this,
            [decodeTask, this] { handleTaskFinished(decodeTask); });
    taskManager->addTask(decodeTask);
    taskManager->startTask(decodeTask);
}

void AudioDecodingController::createAndStartResolveTask(AudioClip *clip) {
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
    const auto projectPath = documentWorkflowController->projectPath();
    resolveTask->projectDir =
        projectPath.isEmpty() ? QString{} : QFileInfo(projectPath).absolutePath();
    const auto automationTask = runtime->automationTasks().createTask(
        Automation::OperationIds::audio_clips::apply_resolved_path, resolveTask->documentVersion,
        Automation::ObjectRef{Automation::ObjectKind::Clip, resolveTask->clipId},
        [resolveTask] { resolveTask->terminate(); });
    resolveTask->automationTaskId = automationTask.taskId;
    runtime->automationTasks().markRunning(automationTask.taskId);

    m_resolveTasks.append(resolveTask);
    connect(resolveTask, &Task::finished, this,
            [resolveTask, this] { handleResolveTaskFinished(resolveTask); });
    taskManager->addTask(resolveTask);
    taskManager->startTask(resolveTask);
}

void AudioDecodingController::handleResolveTaskFinished(ResolveAudioPathTask *task) {
    if (DocumentTaskCompletion::deferCompletionWhileDocumentBusy(
            automationRuntime(), documentWorkflowController, task, this,
            [this](ResolveAudioPathTask *deferredTask) {
                handleResolveTaskFinished(deferredTask);
            }))
        return;

    const auto terminated = task->terminated();
    taskManager->removeTask(task);
    m_resolveTasks.removeOne(task);
    auto *runtime = automationRuntime();
    const bool currentGeneration =
        runtime && runtime->documentVersion().documentId == task->documentVersion.documentId;
    if (currentGeneration && m_pendingResolveCount > 0)
        m_pendingResolveCount--;

    if (terminated) {
        if (runtime)
            runtime->automationTasks().cancel(task->automationTaskId);
        delete task;
        if (currentGeneration)
            finishResolveIfSessionDone();
        return;
    }
    if (!runtime) {
        delete task;
        return;
    }

    const auto currentProjectPath = documentWorkflowController->projectPath();
    const auto currentProjectDir =
        currentProjectPath.isEmpty() ? QString{} : QFileInfo(currentProjectPath).absolutePath();
    auto *currentClip = currentGeneration ? appModel->findClipById(task->clipId) : nullptr;
    auto *currentAudioClip = currentClip && currentClip->clipType() == Clip::Audio
                                 ? static_cast<AudioClip *>(currentClip)
                                 : nullptr;
    const auto currentAsset =
        currentAudioClip ? std::optional(Automation::audioAssetSnapshotDto(*currentAudioClip))
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
            startDecodingOrResolving(currentAudioClip, true);
        delete task;
        finishResolveIfSessionDone();
        return;
    }

    auto contextResult = runtime->derivedWritebackContext(task->documentVersion, true);
    if (!contextResult) {
        runtime->automationTasks().fail(task->automationTaskId, contextResult.getError());
        delete task;
        if (currentGeneration)
            finishResolveIfSessionDone();
        return;
    }
    auto context = contextResult.get();
    Automation::AutomationResult<Automation::MutationResult> validation(Automation::AutomationError{
        .code = Automation::AutomationErrorCode::InternalError,
        .message = QStringLiteral("Audio path resolution did not produce a result"),
    });
    switch (task->result) {
        case ResolveAudioPathTask::Result::HitRelative:
        case ResolveAudioPathTask::Result::HitSibling:
            validation = runtime->project().applyResolvedAudioPath(
                context, Automation::ClipId(task->clipId), task->assetSnapshot, task->resolvedPath,
                AudioClip::PathStatus::Normal);
            break;
        case ResolveAudioPathTask::Result::HitUnconfirmed:
            validation = runtime->project().applyResolvedAudioPath(
                context, Automation::ClipId(task->clipId), task->assetSnapshot, task->resolvedPath,
                AudioClip::PathStatus::Unconfirmed);
            break;
        case ResolveAudioPathTask::Result::Miss:
            validation = runtime->project().setAudioClipPathStatus(
                context, Automation::ClipId(task->clipId), task->assetSnapshot,
                AudioClip::PathStatus::Missing);
            break;
    }
    if (!validation) {
        runtime->automationTasks().fail(task->automationTaskId, validation.getError());
        delete task;
        if (currentGeneration)
            finishResolveIfSessionDone();
        return;
    }
    const auto committing = runtime->automationTasks().beginCommitting(task->automationTaskId);
    if (!committing || !committing.get()) {
        if (committing)
            runtime->automationTasks().cancel(task->automationTaskId);
        delete task;
        if (currentGeneration)
            finishResolveIfSessionDone();
        return;
    }
    context.validateOnly = false;
    Automation::AutomationResult<Automation::MutationResult> result(Automation::AutomationError{
        .code = Automation::AutomationErrorCode::InternalError,
        .message = QStringLiteral("Audio path resolution did not produce a result"),
    });
    switch (task->result) {
        case ResolveAudioPathTask::Result::HitRelative:
        case ResolveAudioPathTask::Result::HitSibling:
            result = runtime->project().applyResolvedAudioPath(
                context, Automation::ClipId(task->clipId), task->assetSnapshot, task->resolvedPath,
                AudioClip::PathStatus::Normal);
            break;
        case ResolveAudioPathTask::Result::HitUnconfirmed:
            result = runtime->project().applyResolvedAudioPath(
                context, Automation::ClipId(task->clipId), task->assetSnapshot, task->resolvedPath,
                AudioClip::PathStatus::Unconfirmed);
            break;
        case ResolveAudioPathTask::Result::Miss:
            result = runtime->project().setAudioClipPathStatus(
                context, Automation::ClipId(task->clipId), task->assetSnapshot,
                AudioClip::PathStatus::Missing);
            break;
    }
    if (result) {
        if (task->result == ResolveAudioPathTask::Result::Miss)
            runtime->automationTasks().fail(
                task->automationTaskId,
                audioPathNotFound(Automation::OperationIds::audio_clips::apply_resolved_path));
        else
            runtime->automationTasks().succeed(task->automationTaskId, result.get());
        if (currentGeneration) {
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
    if (currentGeneration)
        finishResolveIfSessionDone();
}

void AudioDecodingController::finishResolveIfSessionDone() {
    if (m_pendingResolveCount > 0)
        return;
    if (m_missingClipIds.isEmpty() && m_unconfirmedClipIds.isEmpty() && m_autoRelocatedCount == 0)
        return;

    if (m_missingClipIds.isEmpty() && m_unconfirmedClipIds.isEmpty())
        Toast::show(tr("%L1 audio file(s) relocated automatically").arg(m_autoRelocatedCount));

    emit resolveSessionFinished(m_missingClipIds, m_unconfirmedClipIds, m_autoRelocatedCount);
    m_missingClipIds.clear();
    m_unconfirmedClipIds.clear();
    m_autoRelocatedCount = 0;
}

void AudioDecodingController::resolveMissingClipsNear(const QString &filePath) {
    const auto candidateDir = QFileInfo(filePath).absolutePath();
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
            connect(resolveTask, &Task::finished, this,
                    [resolveTask, this] { handleCascadeResolveTaskFinished(resolveTask); });
            taskManager->addTask(resolveTask);
            taskManager->startTask(resolveTask);
        }
    }
}

void AudioDecodingController::handleCascadeResolveTaskFinished(ResolveAudioPathTask *task) {
    if (DocumentTaskCompletion::deferCompletionWhileDocumentBusy(
            automationRuntime(), documentWorkflowController, task, this,
            [this](ResolveAudioPathTask *deferredTask) {
                handleCascadeResolveTaskFinished(deferredTask);
            }))
        return;

    taskManager->removeTask(task);
    m_resolveTasks.removeOne(task);
    auto *runtime = automationRuntime();
    if (!runtime) {
        delete task;
        return;
    }
    if (task->terminated()) {
        runtime->automationTasks().cancel(task->automationTaskId);
        delete task;
        return;
    }
    if (task->result != ResolveAudioPathTask::Result::HitSibling) {
        runtime->automationTasks().fail(
            task->automationTaskId,
            audioPathNotFound(Automation::OperationIds::audio_clips::apply_resolved_path));
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
    const auto validation = runtime->project().applyResolvedAudioPath(
        context, Automation::ClipId(task->clipId), task->assetSnapshot, task->resolvedPath,
        AudioClip::PathStatus::Normal);
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
    const auto result = runtime->project().applyResolvedAudioPath(
        context, Automation::ClipId(task->clipId), task->assetSnapshot, task->resolvedPath,
        AudioClip::PathStatus::Normal);
    if (result) {
        runtime->automationTasks().succeed(task->automationTaskId, result.get());
        emit clipRelocated(task->clipId, task->resolvedPath);
    } else {
        runtime->automationTasks().fail(task->automationTaskId, result.getError());
    }
    delete task;
}

void AudioDecodingController::handleTaskFinished(DecodeAudioTask *task) {
    if (DocumentTaskCompletion::deferCompletionWhileDocumentBusy(
            automationRuntime(), documentWorkflowController, task, this,
            [this](DecodeAudioTask *deferredTask) { handleTaskFinished(deferredTask); }))
        return;

    const auto terminate = task->terminated();
    taskManager->removeTask(task);
    m_tasks.removeOne(task);
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
        Toast::show(tr("Failed to open audio file: %1").arg(task->path));

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
