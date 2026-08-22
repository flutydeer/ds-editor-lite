#include "AudioDecodingController.h"

#include <QFileInfo>
#include <QTimer>

#include <TalcsFormat/FormatManager.h>

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Automation/OperationIds.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include "Modules/Audio/AudioContext.h"
#include <lite/Tasking/TaskManager.h>
#include "Tasks/DecodeAudioTask.h"
#include "Tasks/ResolveAudioPathTask.h"
#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/Toast.h>
#include "UI/Dialogs/Base/Dialog.h"

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
    // in commitReplace, replaceProject (which emits modelChanged) runs before updateProjectIdentity,
    // so documentWorkflowController->projectPath() is not yet updated at that point;
    // projectPath only becomes available after commitReplace has fully completed
    QTimer::singleShot(0, this, &AudioDecodingController::startDecodingAndResolving);
}

void AudioDecodingController::startDecodingAndResolving() {
    for (const auto track : appModel->tracks()) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() != Clip::Audio)
                continue;
            const auto audioClip = static_cast<AudioClip *>(clip);
            if (QFileInfo::exists(audioClip->path())) {
                if (auto *runtime = automationRuntime()) {
                    runtime->project().setAudioClipPathStatus(
                        commandContext(runtime->documentVersion()),
                        Automation::ClipId(audioClip->id()), audioClip->path(),
                        AudioClip::PathStatus::Normal);
                }
                createAndStartTask(audioClip);
            } else {
                // Absolute path is broken; relocate in background via relativeDir / project sibling
                m_pendingResolveCount++;
                createAndStartResolveTask(audioClip);
            }
        }
    }
}

void AudioDecodingController::onTrackChanged(const AppModel::TrackChangeType type, const qsizetype index,
                                             const Track *track) {
    Q_UNUSED(index);
    if (type == AppModel::Insert)
        connect(track, &Track::clipChanged, this, &AudioDecodingController::onClipChanged);
    else if (type == AppModel::Remove) {
        disconnect(track, nullptr, this, nullptr);
        terminateTasksByTrackId(track->id());
    }
}

void AudioDecodingController::onClipChanged(const Track::ClipChangeType type, Clip *clip) {
    if (type == Track::Inserted) {
        if (clip->clipType() == Clip::Audio) {
            const auto audioClip = static_cast<AudioClip *>(clip);
            connectClip(audioClip);
            // TODO: 用其他方式判断是否需要重新解码
            if (audioClip->audioInfo().peakCache.count() <= 0)
                createAndStartTask(audioClip);
        }
    } else if (type == Track::Removed) {
        if (clip->clipType() == Clip::Audio) {
            disconnect(static_cast<AudioClip *>(clip), &AudioClip::pathChanged, this, nullptr);
            terminateTaskByClipId(clip->id());
        }
    }
}

void AudioDecodingController::connectClip(AudioClip *clip) {
    // Re-decode the waveform after relink/replace (including undo)
    connect(clip, &AudioClip::pathChanged, this, [clip, this] {
        terminateTaskByClipId(clip->id());
        auto *runtime = automationRuntime();
        if (!runtime)
            return;
        if (QFileInfo::exists(clip->path())) {
            runtime->project().setAudioClipPathStatus(
                commandContext(runtime->documentVersion()), Automation::ClipId(clip->id()),
                clip->path(), AudioClip::PathStatus::Normal);
            createAndStartTask(clip);
        } else {
            runtime->project().setAudioClipPathStatus(
                commandContext(runtime->documentVersion()), Automation::ClipId(clip->id()),
                clip->path(), AudioClip::PathStatus::Missing);
        }
    });
}

void AudioDecodingController::createAndStartTask(AudioClip *clip) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    auto decodeTask = new DecodeAudioTask;
    decodeTask->clipId = clip->id();
    decodeTask->documentVersion = runtime->documentVersion();
    decodeTask->path = clip->path();
    decodeTask->workspace = clip->workspace().value("diffscope.audio.formatData");

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
            [decodeTask, this] {
                handleTaskFinished(decodeTask);
            });
    taskManager->addTask(decodeTask);
    taskManager->startTask(decodeTask);
}

void AudioDecodingController::createAndStartResolveTask(AudioClip *clip) {
    auto *runtime = automationRuntime();
    if (!runtime)
        return;
    const auto resolveTask = new ResolveAudioPathTask;
    resolveTask->clipId = clip->id();
    resolveTask->documentVersion = runtime->documentVersion();
    resolveTask->originalPath = clip->path();
    resolveTask->relativeDir = clip->pathInfo().relativeDir;
    resolveTask->fileName = QFileInfo(clip->path()).fileName();
    resolveTask->expectedSha512 = clip->pathInfo().sha512;
    resolveTask->projectDir =
        QFileInfo(documentWorkflowController->projectPath()).absolutePath();
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

    Automation::CommandContext context = commandContext(task->documentVersion, true);
    Automation::AutomationResult<Automation::MutationResult> validation(
        Automation::AutomationError{
            .code = Automation::AutomationErrorCode::InternalError,
            .message = QStringLiteral("Audio path resolution did not produce a result"),
        });
    switch (task->result) {
        case ResolveAudioPathTask::Result::HitRelative:
        case ResolveAudioPathTask::Result::HitSibling:
            validation = runtime->project().applyResolvedAudioPath(
                context, Automation::ClipId(task->clipId), task->originalPath,
                task->resolvedPath, AudioClip::PathStatus::Normal);
            break;
        case ResolveAudioPathTask::Result::HitUnconfirmed:
            validation = runtime->project().applyResolvedAudioPath(
                context, Automation::ClipId(task->clipId), task->originalPath,
                task->resolvedPath, AudioClip::PathStatus::Unconfirmed);
            break;
        case ResolveAudioPathTask::Result::Miss:
            validation = runtime->project().setAudioClipPathStatus(
                context, Automation::ClipId(task->clipId), task->originalPath,
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
    Automation::AutomationResult<Automation::MutationResult> result(
        Automation::AutomationError{
            .code = Automation::AutomationErrorCode::InternalError,
            .message = QStringLiteral("Audio path resolution did not produce a result"),
        });
    switch (task->result) {
        case ResolveAudioPathTask::Result::HitRelative:
        case ResolveAudioPathTask::Result::HitSibling:
            result = runtime->project().applyResolvedAudioPath(
                context, Automation::ClipId(task->clipId), task->originalPath,
                task->resolvedPath, AudioClip::PathStatus::Normal);
            break;
        case ResolveAudioPathTask::Result::HitUnconfirmed:
            result = runtime->project().applyResolvedAudioPath(
                context, Automation::ClipId(task->clipId), task->originalPath,
                task->resolvedPath, AudioClip::PathStatus::Unconfirmed);
            break;
        case ResolveAudioPathTask::Result::Miss:
            result = runtime->project().setAudioClipPathStatus(
                context, Automation::ClipId(task->clipId), task->originalPath,
                AudioClip::PathStatus::Missing);
            break;
    }
    if (result) {
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
    if (m_missingClipIds.isEmpty() && m_unconfirmedClipIds.isEmpty() &&
        m_autoRelocatedCount == 0)
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
            // Without sha512 the identity cannot be verified; skip cascading, user must relink manually
            if (audioClip->pathInfo().sha512.isEmpty())
                continue;

            const auto resolveTask = new ResolveAudioPathTask;
            resolveTask->clipId = audioClip->id();
            resolveTask->documentVersion = runtime->documentVersion();
            resolveTask->originalPath = audioClip->path();
            resolveTask->relativeDir = {};
            resolveTask->fileName = QFileInfo(audioClip->path()).fileName();
            resolveTask->expectedSha512 = audioClip->pathInfo().sha512;
            resolveTask->projectDir = candidateDir;
            const auto automationTask = runtime->automationTasks().createTask(
                Automation::OperationIds::audio_clips::apply_resolved_path,
                resolveTask->documentVersion,
                Automation::ObjectRef{Automation::ObjectKind::Clip, resolveTask->clipId},
                [resolveTask] { resolveTask->terminate(); });
            resolveTask->automationTaskId = automationTask.taskId;
            runtime->automationTasks().markRunning(automationTask.taskId);

            m_resolveTasks.append(resolveTask);
            connect(resolveTask, &Task::finished, this, [resolveTask, this] {
                taskManager->removeTask(resolveTask);
                m_resolveTasks.removeOne(resolveTask);
                auto *runtime = automationRuntime();
                if (!runtime) {
                    delete resolveTask;
                    return;
                }
                if (resolveTask->terminated()) {
                    runtime->automationTasks().cancel(resolveTask->automationTaskId);
                    delete resolveTask;
                    return;
                }
                if (resolveTask->result != ResolveAudioPathTask::Result::HitSibling) {
                    runtime->automationTasks().fail(
                        resolveTask->automationTaskId,
                        Automation::AutomationError{
                            .code = Automation::AutomationErrorCode::NotFound,
                            .message = QStringLiteral("No matching audio file was found"),
                        });
                    delete resolveTask;
                    return;
                }
                auto context = commandContext(resolveTask->documentVersion, true);
                const auto validation = runtime->project().applyResolvedAudioPath(
                    context, Automation::ClipId(resolveTask->clipId),
                    resolveTask->originalPath, resolveTask->resolvedPath,
                    AudioClip::PathStatus::Normal);
                if (!validation) {
                    runtime->automationTasks().fail(resolveTask->automationTaskId,
                                                    validation.getError());
                    delete resolveTask;
                    return;
                }
                const auto committing =
                    runtime->automationTasks().beginCommitting(resolveTask->automationTaskId);
                if (!committing || !committing.get()) {
                    if (committing)
                        runtime->automationTasks().cancel(resolveTask->automationTaskId);
                    delete resolveTask;
                    return;
                }
                context.validateOnly = false;
                const auto result = runtime->project().applyResolvedAudioPath(
                    context, Automation::ClipId(resolveTask->clipId),
                    resolveTask->originalPath, resolveTask->resolvedPath,
                    AudioClip::PathStatus::Normal);
                if (result) {
                    runtime->automationTasks().succeed(resolveTask->automationTaskId, result.get());
                    emit clipRelocated(resolveTask->clipId, resolveTask->resolvedPath);
                } else {
                    runtime->automationTasks().fail(resolveTask->automationTaskId,
                                                    result.getError());
                }
                delete resolveTask;
            });
            taskManager->addTask(resolveTask);
            taskManager->startTask(resolveTask);
        }
    }
}

void AudioDecodingController::handleTaskFinished(DecodeAudioTask *task) {
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
        if (!QFileInfo::exists(task->path)) {
            runtime->project().setAudioClipPathStatus(
                commandContext(task->documentVersion), Automation::ClipId(task->clipId),
                task->path, AudioClip::PathStatus::Missing);
            runtime->automationTasks().fail(
                task->automationTaskId,
                Automation::AutomationError{
                    .code = Automation::AutomationErrorCode::IoError,
                    .message = QStringLiteral("Audio file is missing"),
                });
            delete task;
            return;
        }
        runtime->automationTasks().fail(
            task->automationTaskId,
            Automation::AutomationError{
                .code = Automation::AutomationErrorCode::IoError,
                .message = task->errorMessage,
            });
        const auto dlg = new Dialog;
        dlg->setWindowTitle(tr("Error"));
        dlg->setTitle(tr("Failed to open audio file:"));
        dlg->setMessage(task->path);
        dlg->setModal(true);

        const auto btnClose = new AccentButton(tr("Close"));
        connect(btnClose, &Button::clicked, dlg, &Dialog::accept);
        dlg->setPositiveButton(btnClose);
        dlg->show();

        delete task;
        return;
    }

    auto context = commandContext(task->documentVersion, true);
    const auto validation = runtime->project().applyAudioDecodeCache(
        context, Automation::ClipId(task->clipId), task->path, task->result());
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
        context, Automation::ClipId(task->clipId), task->path, task->result());
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
}

void AudioDecodingController::terminateTasksByTrackId(const int trackId) {
    for (auto task : m_tasks) {
        if (task->trackId == trackId)
            taskManager->terminateTask(task);
    }
}
