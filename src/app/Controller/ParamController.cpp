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
        for (const auto note : singingClip->notes())
            handleNoteInserted(note, singingClip);
        connect(singingClip, &SingingClip::noteChanged, this,
                [=](SingingClip::NoteChangeType type, Note *note) {
                    handleNoteChanged(type, note, singingClip);
                });
        createAndStartGetPronTask(singingClip);
    }
}

void ParamController::handleClipRemoved(Clip *clip) {
    qDebug() << "handleClipRemoved" << clip->id();
    // m_clips.removeOne(clip);
    cancelClipRelatedTasks(clip);
    disconnect(clip, nullptr, this, nullptr);
}

void ParamController::handleNoteChanged(SingingClip::NoteChangeType type, Note *note,
                                        SingingClip *clip) {
    cancelClipRelatedTasks(clip);
    if (type == SingingClip::Inserted) {
        handleNoteInserted(note, clip);
    } else if (type == SingingClip::Removed) {
        handleNoteRemoved(note, clip);
    }
    createAndStartGetPronTask(clip);
}

void ParamController::handleNoteInserted(Note *note, SingingClip *clip) {
    connect(note, &Note::timeKeyPropertyChanged, this, [=] { handleNotePropertyChanged(note); });
    connect(note, &Note::wordPropertyChanged, this, [=](Note::WordPropertyType type) {
        if (type == Note::Edited)
            handleNotePropertyChanged(note);
    });
}

void ParamController::handleNoteRemoved(Note *note, SingingClip *clip) {
    disconnect(note, nullptr, this, nullptr);
}

void ParamController::handleNotePropertyChanged(Note *note) {
    cancelClipRelatedTasks(note->clip());
    createAndStartGetPronTask(note->clip());
}

void ParamController::handleLanguageModuleStatusChanged(AppStatus::ModuleStatus status) {
    if (status == AppStatus::ModuleStatus::Ready) {
        qDebug() << "语言模块就绪，开始运行任务";
        for (const auto task : m_pendingGetPronTasks) {
            taskManager->startTask(task);
            m_runningGetPronTasks.append(task);
        }
    } else if (status == AppStatus::ModuleStatus::Error) {
        for (const auto task : m_pendingGetPronTasks)
            taskManager->removeTask(task);
        qCritical() << "未能启动语言模块，已取消任务";
    }
    m_pendingGetPronTasks.clear();
}

void ParamController::handleGetPronTaskFinished(GetPronTask *task, bool terminate) {
    disconnect(task, nullptr, this, nullptr);
    auto clipId = task->id();
    qDebug() << "get pron and phone task finished";
    taskManager->removeTask(task);
    m_runningGetPronTasks.removeOne(task);

    auto clip = appModel->findClipById(task->id());
    if (terminate || !clip) {
        delete task;
        return;
    }

    OriginalParamUtils::updateNotePronPhoneme(task->notesRef, task->result);

    auto singingClip = dynamic_cast<SingingClip *>(appModel->findClipById(task->id()));
    if (validateForInferDuration(task->id())) {
        QList<Note *> notes;
        for (const auto note : singingClip->notes()) {
            notes.append(note);
        }
        auto durTask = new InferDurationTask(clipId, notes);
        connect(durTask, &Task::finished, this,
                [=](bool terminate) { handleInferDurTaskFinished(durTask, terminate); });
        taskManager->addAndStartTask(durTask);
    }
    delete task;
}

void ParamController::handleInferDurTaskFinished(InferDurationTask *task, bool terminate) {
    qDebug() << "infer dur task finished";
    disconnect(task, nullptr, this, nullptr);
    taskManager->removeTask(task);

    auto clip = appModel->findClipById(task->id());
    if (terminate || !clip) {
        delete task;
        return;
    }

    auto result = task->result();
    int phoneIndex = 0;
    QList<Note *> notes;
    auto singingClip = dynamic_cast<SingingClip *>(appModel->findClipById(task->id()));
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

    OriginalParamUtils::updateNotePronPhoneme(notes, args);
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

void ParamController::createAndStartGetPronTask(SingingClip *clip) {
    QList<Note *> notes;
    for (const auto note : clip->notes()) {
        notes.append(note);
    }
    auto task = new GetPronTask(clip->id(), notes);
    connect(task, &Task::finished, this,
            [=](bool terminate) { handleGetPronTaskFinished(task, terminate); });
    taskManager->addTask(task);
    if (appStatus->languageModuleStatus == AppStatus::ModuleStatus::Ready) {
        qDebug() << "创建任务并执行";
        taskManager->startTask(task);
        m_runningGetPronTasks.append(task);
    } else {
        qDebug() << "语言模块未就绪，等待就绪后执行";
        m_pendingGetPronTasks.append(task);
    }
}

void ParamController::cancelClipRelatedTasks(Clip *clip) {
    auto pred = [=](Task *task) { return task->id() == clip->id(); };

    // Cancel get pron tasks
    for (const auto task : Linq::where(m_pendingGetPronTasks, pred)) {
        disconnect(task, nullptr, this, nullptr);
        m_pendingGetPronTasks.removeOne(task);
        taskManager->removeTask(task);
        delete task;
    }
    for (const auto task : Linq::where(m_runningGetPronTasks, pred))
        taskManager->terminateTask(task);

    // Cancel infer dur tasks
    for (const auto task : Linq::where(m_pendingInferDurTasks, pred)) {
        disconnect(task, nullptr, this, nullptr);
        m_pendingInferDurTasks.removeOne(task);
        taskManager->removeTask(task);
        delete task;
    }
    for (const auto task : Linq::where(m_runningInferDurTasks, pred))
        taskManager->terminateTask(task);
}