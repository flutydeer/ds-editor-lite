//
// Created by fluty on 2024/7/11.
//

#include "AudioDecodingController.h"

#include <QFileInfo>

#include <TalcsFormat/FormatManager.h>

#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Model/AppModel/AudioClip.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Task/TaskManager.h"
#include "Tasks/DecodeAudioTask.h"
#include "Tasks/ResolveAudioPathTask.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/Toast.h"
#include "UI/Dialogs/Base/Dialog.h"

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

    // Start new decoding tasks
    for (const auto track : appModel->tracks()) {
        connect(track, &Track::clipChanged, this, &AudioDecodingController::onClipChanged);
        for (const auto clip : track->clips()) {
            if (clip->clipType() != Clip::Audio)
                continue;
            const auto audioClip = static_cast<AudioClip *>(clip);
            connectClip(audioClip);
            if (QFileInfo::exists(audioClip->path())) {
                audioClip->setPathStatus(AudioClip::PathStatus::Normal);
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
        if (QFileInfo::exists(clip->path())) {
            clip->setPathStatus(AudioClip::PathStatus::Normal);
            createAndStartTask(clip);
        } else {
            clip->setPathStatus(AudioClip::PathStatus::Missing);
        }
    });
}

void AudioDecodingController::createAndStartTask(AudioClip *clip) {
    auto decodeTask = new DecodeAudioTask;
    decodeTask->clipId = clip->id();
    decodeTask->path = clip->path();
    decodeTask->workspace = clip->workspace().value("diffscope.audio.formatData");

    QVariant userData;
    QDataStream o(
        QByteArray::fromBase64(decodeTask->workspace.value("userData").toString().toLatin1()));
    o >> userData;
    const auto entryClassName = decodeTask->workspace.value("entryClassName").toString();
    decodeTask->io = AudioContext::instance()->formatManager()->getFormatLoad(
        decodeTask->path, userData, entryClassName);

    m_tasks.append(decodeTask);
    connect(decodeTask, &Task::finished, this,
            [decodeTask, this] {
                handleTaskFinished(decodeTask);
            });
    taskManager->addTask(decodeTask);
    taskManager->startTask(decodeTask);
}

void AudioDecodingController::createAndStartResolveTask(AudioClip *clip) {
    const auto resolveTask = new ResolveAudioPathTask;
    resolveTask->clipId = clip->id();
    resolveTask->originalPath = clip->path();
    resolveTask->relativeDir = clip->pathInfo().relativeDir;
    resolveTask->fileName = QFileInfo(clip->path()).fileName();
    resolveTask->expectedSha512 = clip->pathInfo().sha512;
    resolveTask->projectDir =
        QFileInfo(documentWorkflowController->projectPath()).absolutePath();

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
    m_pendingResolveCount--;

    if (terminated) {
        delete task;
        finishResolveIfSessionDone();
        return;
    }

    int trackIndex;
    const auto clip = appModel->findClipById(task->clipId, trackIndex);
    if (!clip || clip->clipType() != Clip::Audio) {
        delete task;
        finishResolveIfSessionDone();
        return;
    }

    const auto audioClip = static_cast<AudioClip *>(clip);
    switch (task->result) {
        case ResolveAudioPathTask::Result::HitRelative:
        case ResolveAudioPathTask::Result::HitSibling:
            // Hash verified; adopt the new path silently (no undo, no dirty flag, persisted on next save)
            audioClip->setPath(task->resolvedPath);
            audioClip->setPathStatus(AudioClip::PathStatus::Normal);
            m_autoRelocatedCount++;
            createAndStartTask(audioClip);
            break;
        case ResolveAudioPathTask::Result::HitUnconfirmed:
            // No sha512 to verify against; matched by file name, load it but mark as unconfirmed
            audioClip->setPath(task->resolvedPath);
            audioClip->setPathStatus(AudioClip::PathStatus::Unconfirmed);
            m_unconfirmedClipIds.append(audioClip->id());
            createAndStartTask(audioClip);
            break;
        case ResolveAudioPathTask::Result::Miss:
            audioClip->setPathStatus(AudioClip::PathStatus::Missing);
            m_missingClipIds.append(audioClip->id());
            break;
    }
    delete task;
    finishResolveIfSessionDone();
}

void AudioDecodingController::finishResolveIfSessionDone() {
    if (m_pendingResolveCount > 0)
        return;
    if (m_missingClipIds.isEmpty() && m_unconfirmedClipIds.isEmpty() &&
        m_autoRelocatedCount == 0)
        return;

    if (m_missingClipIds.isEmpty() && m_unconfirmedClipIds.isEmpty())
        Toast::show(tr("%1 audio file(s) relocated automatically").arg(m_autoRelocatedCount));

    emit resolveSessionFinished(m_missingClipIds, m_unconfirmedClipIds, m_autoRelocatedCount);
    m_missingClipIds.clear();
    m_unconfirmedClipIds.clear();
    m_autoRelocatedCount = 0;
}

void AudioDecodingController::handleTaskFinished(DecodeAudioTask *task) {
    const auto terminate = task->terminated();
    taskManager->removeTask(task);
    m_tasks.removeOne(task);

    if (terminate) {
        delete task;
        return;
    }
    if (!task->success) {
        int trackIdx;
        if (!QFileInfo::exists(task->path)) {
            // A missing file goes offline and is handled by the missing-media flow; no error dialog
            if (const auto clip = appModel->findClipById(task->clipId, trackIdx);
                clip && clip->clipType() == Clip::Audio)
                static_cast<AudioClip *>(clip)->setPathStatus(AudioClip::PathStatus::Missing);
            delete task;
            return;
        }
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

    int trackIndex;
    const auto clip = appModel->findClipById(task->clipId, trackIndex);
    if (!clip) {
        delete task;
        return;
    }

    const auto audioClip = static_cast<AudioClip *>(clip);
    audioClip->setAudioInfo(task->result());
    audioClip->notifyPropertyChanged();
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