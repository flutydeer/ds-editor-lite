//
// Created by fluty on 2024/7/11.
//

#include "AudioDecodingController.h"
#include "Model/Clip.h"
#include "Modules/Task/TaskManager.h"
#include "Tasks/DecodeAudioTask.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Dialogs/Base/Dialog.h"

void AudioDecodingController::onModelChanged() {
    qDebug() << "AudioDecodingController::onModelChanged";
    // Terminate all decoding tasks
    for (auto task : m_tasks) {
        TaskManager::instance()->terminateTask(task);
    }
    // Start new decoding tasks
    for (auto track : AppModel::instance()->tracks()) {
        connect(track, &Track::clipChanged, this, &AudioDecodingController::onClipChanged);
        for (auto clip : track->clips()) {
            if (clip->type() == Clip::Audio)
                createAndStartTask(reinterpret_cast<AudioClip *>(clip));
        }
    }
}
void AudioDecodingController::onTrackChanged(AppModel::TrackChangeType type, qsizetype index,
                                             Track *track) {
    qDebug() << "AudioDecodingController::onTrackChanged";
    if (type == AppModel::Insert)
        connect(track, &Track::clipChanged, this, &AudioDecodingController::onClipChanged);
    else if (type == AppModel::Remove) {
        disconnect(track, nullptr, this, nullptr);
        terminateTasksByTrackId(track->id());
    }
}
void AudioDecodingController::onClipChanged(Track::ClipChangeType type, int id, Clip *clip) {
    qDebug() << "AudioDecodingController::onClipChanged";
    if (type == Track::Inserted) {
        if (clip->type() == Clip::Audio) {
            auto audioClip = reinterpret_cast<AudioClip *>(clip);
            // TODO: 用其他方式判断是否需要重新解码
            if (audioClip->info.peakCache.count() <= 0)
                createAndStartTask(audioClip);
        }
    } else if (type == Track::Removed) {
        if (clip->type() == Clip::Audio)
            terminateTaskByClipId(clip->id());
    }
}
void AudioDecodingController::createAndStartTask(AudioClip *clip) {
    auto decodeTask = new DecodeAudioTask;
    decodeTask->clipId = clip->id();
    decodeTask->path = clip->path();
    m_tasks.append(decodeTask);
    connect(decodeTask, &Task::finished, this,
            [=](bool terminate) { handleTaskFinished(decodeTask, terminate); });
    TaskManager::instance()->addTask(decodeTask);
    TaskManager::instance()->startTask(decodeTask);
}
void AudioDecodingController::handleTaskFinished(DecodeAudioTask *task, bool terminate) {
    TaskManager::instance()->removeTask(task);
    m_tasks.removeOne(task);

    if (terminate) {
        delete task;
        return;
    }
    if (!task->success) {
        auto dlg = new Dialog;
        dlg->setWindowTitle(tr("Error"));
        dlg->setTitle(tr("Failed to open audio file:"));
        dlg->setMessage(task->path);
        dlg->setModal(true);

        auto btnClose = new AccentButton(tr("Close"));
        connect(btnClose, &Button::clicked, dlg, &Dialog::accept);
        dlg->setPositiveButton(btnClose);
        dlg->show();

        delete task;
        return;
    }

    int trackIndex;
    auto clip = AppModel::instance()->findClipById(task->clipId, trackIndex);
    if (!clip) {
        delete task;
        return;
    }

    auto audioClip = reinterpret_cast<AudioClip *>(clip);
    audioClip->info = task->result();
    auto track = AppModel::instance()->tracks().at(trackIndex);
    track->notifyClipPropertyChanged(audioClip);
    delete task;
}
void AudioDecodingController::terminateTaskByClipId(int clipId) {
    for (const auto task : m_tasks)
        if (task->clipId == clipId)
            TaskManager::instance()->terminateTask(task);
}
void AudioDecodingController::terminateTasksByTrackId(int trackId) {
    for (auto task : m_tasks) {
        if (task->trackId == trackId)
            TaskManager::instance()->terminateTask(task);
    }
}