//
// Created by OrangeCat on 24-9-3.
//

#include "InferController.h"
#include "InferController_p.h"

#include "Model/Inference/InferDurNote.h"
#include "Model/Inference/InferPiece.h"
#include "Modules/Inference/InferDurationTask.h"
#include "Tasks/GetPhonemeNameTask.h"
#include "Tasks/GetPronunciationTask.h"
#include "Utils/Linq.h"
#include "Utils/NoteWordUtils.h"
#include "Utils/OriginalParamUtils.h"
#include "Utils/ValidationUtils.h"

InferController::InferController() : d_ptr(new InferControllerPrivate(this)) {
    Q_D(InferController);
    connect(appModel, &AppModel::modelChanged, d, &InferControllerPrivate::onModelChanged);
    connect(appStatus, &AppStatus::moduleStatusChanged, d,
            &InferControllerPrivate::onModuleStatusChanged);
    connect(appStatus, &AppStatus::editingChanged, d, &InferControllerPrivate::onEditingChanged);
}

void InferControllerPrivate::onModelChanged() {
    for (const auto track : m_tracks)
        onTrackChanged(AppModel::Remove, -1, track);

    for (const auto track : appModel->tracks())
        onTrackChanged(AppModel::Insert, -1, track);
}

void InferControllerPrivate::onTrackChanged(AppModel::TrackChangeType type, qsizetype index,
                                            Track *track) {
    if (type == AppModel::Insert) {
        m_tracks.append(track);
        for (const auto clip : track->clips())
            handleClipInserted(clip);
        connect(track, &Track::clipChanged, this, &InferControllerPrivate::onClipChanged);
    } else if (type == AppModel::Remove) {
        m_tracks.removeOne(track);
        for (const auto clip : track->clips())
            handleClipRemoved(clip);
        disconnect(track, &Track::clipChanged, this, &InferControllerPrivate::onClipChanged);
    }
}

void InferControllerPrivate::onClipChanged(Track::ClipChangeType type, Clip *clip) {
    if (type == Track::Inserted) {
        handleClipInserted(clip);
    } else if (type == Track::Removed) {
        handleClipRemoved(clip);
    }
}

void InferControllerPrivate::onModuleStatusChanged(AppStatus::ModuleType module,
                                                   AppStatus::ModuleStatus status) {
    if (module == AppStatus::ModuleType::Language) {
        handleLanguageModuleStatusChanged(status);
    }
}

void InferControllerPrivate::onEditingChanged(AppStatus::EditObjectType type) {
    // TODO：需要处理编辑被取消的情况
    if (type == AppStatus::EditObjectType::Note) {
        qWarning() << "正在编辑工程，取消相关任务";
        auto clip = appModel->findClipById(appStatus->activeClipId);
        cancelClipRelatedTasks(clip);
    } else if (type == AppStatus::EditObjectType::None) {
        // if (m_lastEditObjectType == AppStatus::EditObjectType::Note) {
        // qInfo() << "编辑完成，重新创建任务";
        // auto clip = appModel->findClipById(appStatus->activeClipId);
        // if (clip->clipType() == IClip::Singing)
        //     createAndRunGetPronTask(dynamic_cast<SingingClip *>(clip));
        // }
    }
    m_lastEditObjectType = type;
}

void InferControllerPrivate::handleClipInserted(Clip *clip) {
    if (clip->clipType() != Clip::Singing)
        return;
    auto singingClip = reinterpret_cast<SingingClip *>(clip);
    connect(singingClip, &SingingClip::noteChanged, this,
            [=](SingingClip::NoteChangeType type, const QList<Note *> &notes) {
                handleNoteChanged(type, notes, singingClip);
            });
    singingClip->reSegment();
    createAndRunGetPronTask(singingClip);
}

void InferControllerPrivate::handleClipRemoved(Clip *clip) {
    cancelClipRelatedTasks(clip);
    disconnect(clip, nullptr, this, nullptr);
}

void InferControllerPrivate::handleNoteChanged(SingingClip::NoteChangeType type,
                                               const QList<Note *> &notes, SingingClip *clip) {
    switch (type) {
        case SingingClip::Insert:
        case SingingClip::Remove:
        case SingingClip::EditedWordPropertyChange:
        case SingingClip::TimeKeyPropertyChange:
            clip->reSegment();
            cancelClipRelatedTasks(clip);
            createAndRunGetPronTask(clip);
            break;
        default:
            break;
    } // Ignore original word property change
}

void InferControllerPrivate::handleLanguageModuleStatusChanged(AppStatus::ModuleStatus status) {
    if (status == AppStatus::ModuleStatus::Ready) {
        qDebug() << "语言模块就绪，开始运行任务";
        runNextGetPronTask();
    } else if (status == AppStatus::ModuleStatus::Error) {
        m_getPronTasks.disposePendingTasks();
        qCritical() << "未能启动语言模块，已取消任务";
    }
}

void InferControllerPrivate::handleGetPronTaskFinished(GetPronunciationTask *task) {
    m_getPronTasks.onCurrentFinished();
    runNextGetPronTask();
    auto clip = appModel->findClipById(task->clipId());
    if (task->terminated() || !clip) {
        delete task;
        return;
    }

    auto singingClip = dynamic_cast<SingingClip *>(clip);
    OriginalParamUtils::updateNotesPronunciation(task->notesRef, task->result, singingClip);
    createAndRunGetPhoneTask(singingClip);
    delete task;
}

void InferControllerPrivate::handleGetPhoneTaskFinished(GetPhonemeNameTask *task) {
    m_getPhoneTasks.onCurrentFinished();
    runNextGetPhoneTask();
    auto clip = appModel->findClipById(task->clipId());
    if (task->terminated() || !clip) {
        delete task;
        return;
    }

    auto singingClip = dynamic_cast<SingingClip *>(clip);
    OriginalParamUtils::updateNotesPhonemeName(task->notesRef, task->result, singingClip);
    createAndRunInferDurTask(singingClip);
    delete task;
}

void InferControllerPrivate::handleInferDurTaskFinished(InferDurationTask *task) {
    m_inferDurTasks.onCurrentFinished();
    runNextInferDurTask();
    auto clip = appModel->findClipById(task->clipId());
    if (task->terminated() || !clip) {
        delete task;
        return;
    }

    auto singingClip = dynamic_cast<SingingClip *>(appModel->findClipById(task->clipId()));
    auto piece = singingClip->findPieceById(task->pieceId());
    auto modelNoteCount = piece->notes.count();
    auto taskNoteCount = task->result().count();
    if (modelNoteCount != taskNoteCount) {
        qFatal() << "模型音符数不等于任务音符数"
                 << "模型音符数：" << modelNoteCount << "任务音符数：" << taskNoteCount;
        return;
    }
    OriginalParamUtils::updateNotesPhonemeOffset(piece->notes, task->result(), singingClip);
    delete task;
}

void InferControllerPrivate::createAndRunGetPronTask(SingingClip *clip) {
    auto task = new GetPronunciationTask(clip->id(), clip->notes().toList());
    connect(task, &Task::finished, this, [=] { handleGetPronTaskFinished(task); });
    m_getPronTasks.add(task);
    if (!m_getPronTasks.current)
        runNextGetPronTask();
}

void InferControllerPrivate::createAndRunGetPhoneTask(SingingClip *clip) {
    QList<PhonemeNameInput> inputs;
    for (const auto note : clip->notes())
        inputs.append({note->lyric(), note->pronunciation().result()});
    auto task = new GetPhonemeNameTask(clip->id(), inputs);
    task->notesRef = clip->notes().toList();
    connect(task, &Task::finished, this, [=] { handleGetPhoneTaskFinished(task); });
    m_getPhoneTasks.add(task);
    if (!m_getPhoneTasks.current)
        runNextGetPhoneTask();
}

void InferControllerPrivate::createAndRunInferDurTask(SingingClip *clip) {
    auto inferDur = [=](const InferPiece &piece) {
        QList<InferDurNote> inputNotes;
        for (const auto note : piece.notes)
            inputNotes.append(InferDurNote(*note));
        auto task = new InferDurationTask(
            {clip->id(), piece.id(), inputNotes,
             R"(F:\Sound libraries\DiffSinger\OpenUtau\Singers\Junninghua_v1.4.0_DiffSinger_OpenUtau\dsconfig.yaml)",
             appModel->tempo()});
        connect(task, &Task::finished, this, [=] { handleInferDurTaskFinished(task); });
        m_inferDurTasks.add(task);
        if (!m_inferDurTasks.current)
            runNextInferDurTask();
    };

    if (ValidationUtils::canInferDuration(*clip)) {
        for (const auto piece : clip->pieces())
            inferDur(*piece);
    } else
        qWarning() << "音素序列有错误，无法创建时长推理任务 clipId:" << clip->id();
}

void InferControllerPrivate::cancelClipRelatedTasks(const Clip *clip) {
    qInfo() << "取消歌声剪辑相关任务"
            << "clipId:" << clip->id();
    auto pred = [=](auto t) { return t->clipId() == clip->id(); };
    m_getPronTasks.cancelIf(pred);
    m_getPhoneTasks.cancelIf(pred);
    m_inferDurTasks.cancelIf(pred);
}

void InferControllerPrivate::cancelPieceRelatedTasks(const InferPiece *piece) {
    qInfo() << "取消分段相关任务"
            << "pieceId:" << piece->id();
    auto pred = [=](auto t) { return t->pieceId() == piece->id(); };
    m_inferDurTasks.cancelIf(pred);
}

void InferControllerPrivate::runNextGetPronTask() {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready)
        return;
    m_getPronTasks.runNext();
}

void InferControllerPrivate::runNextGetPhoneTask() {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready)
        return;
    m_getPhoneTasks.runNext();
}

void InferControllerPrivate::runNextInferDurTask() {
    m_inferDurTasks.runNext();
}