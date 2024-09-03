//
// Created by OrangeCat on 24-9-3.
//

#include "ParamController.h"

#include "Actions/AppModel/Note/NoteActions.h"
#include "Modules/Inference/InferDurationTask.h"
#include "Modules/Task/TaskManager.h"
#include "Tasks/GetPronTask.h"

#include <bits/fs_fwd.h>

ParamController::ParamController() {
    connect(appModel, &AppModel::modelChanged, this, &ParamController::onModelChanged);
    connect(appStatus, &AppStatus::moduleStatusChanged, this,
            &ParamController::onModuleStatusChanged);
}

void ParamController::onModelChanged() {
    for (const auto track : m_tracks)
        onTrackChanged(AppModel::Remove, -1, track);

    for (const auto track : appModel->tracks())
        onTrackChanged(AppModel::Insert, -1, track);
}

void ParamController::onTrackChanged(AppModel::TrackChangeType type, qsizetype index,
                                     Track *track) {
    if (type == AppModel::Insert) {
        // qDebug() << "onTrackChanged" << "Insert";
        m_tracks.append(track);
        for (const auto clip : track->clips())
            handleClipInserted(clip);
        connect(track, &Track::clipChanged, this, &ParamController::onClipChanged);
    } else if (type == AppModel::Remove) {
        // qDebug() << "onTrackChanged" << "Remove";
        m_tracks.removeOne(track);
        for (const auto clip : track->clips())
            handleClipRemoved(clip);
        disconnect(track, &Track::clipChanged, this, &ParamController::onClipChanged);
    }
}

void ParamController::onClipChanged(Track::ClipChangeType type, Clip *clip) {
    if (type == Track::Inserted) {
        // qDebug() << "onClipChanged" << "Inserted";
        handleClipInserted(clip);
    } else if (type == Track::Removed) {
        // qDebug() << "onClipChanged" << "Removed";
        handleClipRemoved(clip);
    }
}

void ParamController::onModuleStatusChanged(AppStatus::ModuleType module,
                                            AppStatus::ModuleStatus status) {
    if (module == AppStatus::ModuleType::Language) {
        if (status == AppStatus::ModuleStatus::Ready) {
            qDebug() << "语言模块就绪，开始运行任务";
            for (const auto task : m_pendingGetPronTasks)
                taskManager->startTask(task);
        } else if (status == AppStatus::ModuleStatus::Error) {
            for (const auto task : m_pendingGetPronTasks)
                taskManager->removeTask(task);
            m_pendingGetPronTasks.clear();
            qCritical() << "未能启动语言模块，已取消任务";
        }
    }
}

void ParamController::handleClipInserted(Clip *clip) {
    // m_clips.append(clip);
    if (clip->clipType() == Clip::Singing) {
        auto singingClip = reinterpret_cast<SingingClip *>(clip);
        QList<Note *> notes;
        for (const auto note : singingClip->notes()) {
            notes.append(note);
        }
        auto task = new GetPronTask(clip->id(), notes);
        connect(task, &Task::finished, this, [=] { handleGetPronTaskFinished(task); });
        taskManager->addTask(task);
        if (appStatus->languageModuleStatus == AppStatus::ModuleStatus::Ready) {
            qDebug() << "创建任务并执行";
            taskManager->startTask(task);
        } else {
            qDebug() << "语言模块未就绪，等待就绪后执行";
            m_pendingGetPronTasks.append(task);
        }
    }
}

void ParamController::handleClipRemoved(Clip *clip) {
    // m_clips.removeOne(clip);
    disconnect(clip, nullptr, this, nullptr);
}

void ParamController::handleGetPronTaskFinished(GetPronTask *task) {
    disconnect(task, nullptr, this, nullptr);
    auto clipId = task->id();
    qDebug() << "get pron and phone task finished";
    taskManager->removeTask(task);

    auto clip = appModel->findClipById(task->id());
    if (!clip) {
        delete task;
        return;
    }

    auto a = new NoteActions;
    a->editNotesWordProperties(task->notesRef, task->result);
    a->execute();
    delete task;

    auto singingClip = dynamic_cast<SingingClip *>(appModel->findClipById(task->id()));
    if (validatePronAndPhoneme(task->id())) {
        QList<Note *> notes;
        for (const auto note : singingClip->notes()) {
            notes.append(note);
        }
        auto durTask = new InferDurationTask(clipId, notes);
        connect(durTask, &Task::finished, this, [=] { handleInferDurTaskFinished(durTask); });
        taskManager->addAndStartTask(durTask);
    }
}

void ParamController::handleInferDurTaskFinished(InferDurationTask *task) {
    qDebug() << "infer dur task finished";
    disconnect(task, nullptr, this, nullptr);
    taskManager->removeTask(task);

    auto clip = appModel->findClipById(task->id());
    if (!clip) {
        delete task;
        return;
    }

    auto result = task->result();
    int phoneIndex = 0;
    QList<Note *> notes;
    auto singingClip = dynamic_cast<SingingClip *>(appModel->findClipById(task->id()));
    QList<Note::NoteWordProperties> args;
    for (const auto note : singingClip->notes()) {
        auto arg = Note::NoteWordProperties::fromNote(*note);
        for (auto &phoneme : arg.phonemes.original) {
            phoneme.start = result.at(phoneIndex).start;
            // qDebug() << phoneme.name << phoneme.start;
            phoneIndex++;
        }
        args.append(arg);
        notes.append(note);
    }

    auto a = new NoteActions;
    a->editNotesWordProperties(notes, args);
    a->execute();

    delete task;
}

bool ParamController::validatePronAndPhoneme(int clipId) {
    auto clip = dynamic_cast<SingingClip *>(appModel->findClipById(clipId));
    if (!clip) {
        qCritical() << "Invalid clip type";
        return false;
    }
    for (const auto note : clip->notes()) {
        if (note->pronunciation().original.isEmpty())
            return false;
        for (const auto &phoneme : note->phonemeInfo().original)
            if (phoneme.name.isEmpty())
                return false;
    }
    return true;
}