//
// Created by OrangeCat on 24-9-3.
//

#include "ParamController.h"

#include "Actions/AppModel/Note/NoteActions.h"
#include "Modules/Inference/InferDurationTask.h"
#include "Modules/Task/TaskManager.h"
#include "Tasks/GetPronTask.h"
#include "Utils/Linq.h"
#include "Utils/OriginalParamUtils.h"
#include "Utils/Queue.h"

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
        handleLanguageModuleStatusChanged(status);
    }
}

void ParamController::handleClipInserted(Clip *clip) {
    // m_clips.append(clip);
    if (clip->clipType() == Clip::Singing) {
        auto singingClip = reinterpret_cast<SingingClip *>(clip);
        connect(singingClip, &SingingClip::noteChanged, this,
                [=](SingingClip::NoteChangeType type, const QList<Note *> &notes) {
                    handleNoteChanged(type, notes, singingClip);
                });
        createAndRunGetPronTask(singingClip);
    }
}

void ParamController::handleClipRemoved(Clip *clip) {
    qDebug() << "handleClipRemoved" << clip->id();
    // m_clips.removeOne(clip);
    cancelClipRelatedTasks(clip);
    disconnect(clip, nullptr, this, nullptr);
}

void ParamController::handleNoteChanged(SingingClip::NoteChangeType type,
                                        const QList<Note *> &notes, SingingClip *clip) {
    switch (type) {
        case SingingClip::Insert:
        case SingingClip::Remove:
        case SingingClip::EditedWordPropertyChange:
        case SingingClip::TimeKeyPropertyChange:
            cancelClipRelatedTasks(clip);
            createAndRunGetPronTask(clip);
            break;
        default:
            break;
    } // Ignore original word property change
}

void ParamController::handleLanguageModuleStatusChanged(AppStatus::ModuleStatus status) {
    if (status == AppStatus::ModuleStatus::Ready) {
        qDebug() << "语言模块就绪，开始运行任务";
        runNextGetPronTask();
    } else if (status == AppStatus::ModuleStatus::Error) {
        for (const auto task : m_getPronTaskQueue) {
            taskManager->removeTask(task);
            disconnect(task, nullptr, this, nullptr);
            delete task;
        }
        qCritical() << "未能启动语言模块，已取消任务";
    }
}

void ParamController::handleGetPronTaskFinished(GetPronTask *task) {
    auto terminate = task->terminated();
    qInfo() << "获取发音和音素任务完成 clipId:" << task->clipId << "taskId:" << task->id()
            << "terminate:" << terminate;
    disconnect(task, nullptr, this, nullptr);
    taskManager->removeTask(task);
    m_runningGetPronTask = nullptr;
    runNextGetPronTask();

    auto clip = appModel->findClipById(task->clipId);
    if (terminate || !clip) {
        delete task;
        return;
    }

    auto singingClip = dynamic_cast<SingingClip *>(clip);
    OriginalParamUtils::updateNotePronPhoneme(task->notesRef, task->result, singingClip);
    createAndRunInferDurTask(singingClip);
    delete task;
}

void ParamController::handleInferDurTaskFinished(InferDurationTask *task) {
    auto terminate = task->terminated();
    qInfo() << "时长推理任务完成 clipId:" << task->clipId << "taskId:" << task->id()
            << "terminate:" << terminate;
    disconnect(task, nullptr, this, nullptr);
    taskManager->removeTask(task);
    m_runningInferDurTask = nullptr;
    runNextInferDurTask();

    auto clip = appModel->findClipById(task->clipId);
    if (terminate || !clip) {
        delete task;
        return;
    }

    auto result = task->result();
    int phoneIndex = 0;
    QList<Note *> notes;
    auto singingClip = dynamic_cast<SingingClip *>(appModel->findClipById(task->clipId));
    auto modelNoteCount = singingClip->notes().count();
    auto taskNoteCount = task->notes().count();
    qDebug() << "模型音符数：" << modelNoteCount << "任务音符数：" << taskNoteCount;
    if (modelNoteCount != taskNoteCount)
        qFatal() << "模型音符数不等于任务音符数";
    QList<Note::WordProperties> args;
    for (const auto note : singingClip->notes()) {
        auto arg = Note::WordProperties::fromNote(*note);
        for (auto &phoneme : arg.phonemes.original) {
            phoneme.start = result.at(phoneIndex).start;
            // qDebug() << phoneme.name << phoneme.start;
            phoneIndex++;
        }
        args.append(arg);
        notes.append(note);
    }

    OriginalParamUtils::updateNotePronPhoneme(notes, args, singingClip);
    delete task;
}

bool ParamController::validateForInferDuration(int clipId) {
    auto clip = dynamic_cast<SingingClip *>(appModel->findClipById(clipId));
    if (!clip) {
        qCritical() << "Invalid clip type";
        return false;
    }
    if (clip->notes().hasOverlappedItem())
        return false;
    for (const auto note : clip->notes()) {
        if (note->pronunciation().original.isEmpty())
            return false;
        for (const auto &phoneme : note->phonemeInfo().original)
            if (phoneme.name.isEmpty())
                return false;
    }
    return true;
}

void ParamController::createAndRunGetPronTask(SingingClip *clip) {
    QList<Note *> notes;
    for (const auto note : clip->notes()) {
        notes.append(note);
    }
    auto task = new GetPronTask(clip->id(), notes);
    qInfo() << "创建获取发音和音素任务 clipId:" << clip->id() << "taskId:" << task->id()
            << "noteCount:" << notes.size();
    connect(task, &Task::finished, this,
            [=] { handleGetPronTaskFinished(task); });
    taskManager->addTask(task);
    m_getPronTaskQueue.enqueue(task);
    if (!m_runningGetPronTask)
        runNextGetPronTask();
}

void ParamController::createAndRunInferDurTask(SingingClip *clip) {
    if (validateForInferDuration(clip->id())) {
        QList<Note *> notes;
        for (const auto note : clip->notes()) {
            notes.append(note);
        }
        auto durTask = new InferDurationTask(clip->id(), notes);
        qDebug() << "音素序列校验通过，创建时长推理任务 clipId:" << clip->id()
                 << "taskId:" << durTask->id();
        connect(durTask, &Task::finished, this,
                [=] { handleInferDurTaskFinished(durTask); });
        taskManager->addTask(durTask);
        m_inferDurTaskQueue.enqueue(durTask);
        if (!m_runningInferDurTask)
            runNextInferDurTask();
    } else
        qWarning() << "音素序列有错误，无法创建时长推理任务 clipId:" << clip->id();
}

void ParamController::cancelClipRelatedTasks(Clip *clip) {
    qInfo() << "--------------------------------";
    qInfo() << "模型发生改动，取消歌声剪辑相关任务";
    auto getPronTaskPred = [=](GetPronTask *task) { return task->clipId == clip->id(); };
    auto inferDurTaskPred = [=](InferDurationTask *task) { return task->clipId == clip->id(); };

    // Cancel get pron tasks
    for (const auto task : Linq::where(m_getPronTaskQueue, getPronTaskPred)) {
        disconnect(task, nullptr, this, nullptr);
        m_getPronTaskQueue.remove(task);
        taskManager->removeTask(task);
        qDebug() << "移除待运行的获取发音和音素任务 clipId:" << task->clipId
                 << "taskId:" << task->id();
        delete task;
    }
    if (m_runningGetPronTask && m_runningGetPronTask->clipId == clip->id()) {
        taskManager->terminateTask(m_runningGetPronTask);
        qDebug() << "终止正在运行的获取发音和音素任务 clipId:" << m_runningGetPronTask->clipId
                 << "taskId:" << m_runningGetPronTask->id();
    }

    // Cancel infer dur tasks
    for (const auto task : Linq::where(m_inferDurTaskQueue, inferDurTaskPred)) {
        disconnect(task, nullptr, this, nullptr);
        m_inferDurTaskQueue.remove(task);
        taskManager->removeTask(task);
        qDebug() << "移除待运行的时长推理任务 clipId:" << task->clipId << "taskId:" << task->id();
        delete task;
    }
    if (m_runningInferDurTask && m_runningInferDurTask->clipId == clip->id()) {
        taskManager->terminateTask(m_runningInferDurTask);
        qDebug() << "终止正在运行的时长推理任务 clipId:" << m_runningInferDurTask->clipId
                 << "taskId:" << m_runningInferDurTask->id();
    }
}

void ParamController::runNextGetPronTask() {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready)
        return;
    if (m_getPronTaskQueue.count() <= 0)
        return;

    const auto task = m_getPronTaskQueue.dequeue();
    qDebug() << "运行获取发音和音素任务 clipId:" << task->clipId << "taskId:" << task->id();
    taskManager->startTask(task);
    m_runningGetPronTask = task;
}

void ParamController::runNextInferDurTask() {
    if (m_inferDurTaskQueue.count() <= 0)
        return;

    const auto task = m_inferDurTaskQueue.dequeue();
    qDebug() << "运行时长推理任务 clipId:" << task->clipId << "taskId:" << task->id();
    taskManager->startTask(task);
    m_runningInferDurTask = task;
}