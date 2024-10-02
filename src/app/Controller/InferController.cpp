//
// Created by OrangeCat on 24-9-3.
//

#include "InferController.h"
#include "InferController_p.h"

#include "Model/Inference/InferDurPitNote.h"
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
    connect(appStatus, &AppStatus::moduleStatusChanged, d,
            &InferControllerPrivate::onModuleStatusChanged);
    connect(appStatus, &AppStatus::editingChanged, d, &InferControllerPrivate::onEditingChanged);
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
        //     qInfo() << "编辑完成，重新创建任务";
        //     auto clip = appModel->findClipById(appStatus->activeClipId);
        //     if (clip->clipType() == IClip::Singing)
        //         createAndRunGetPronTask(dynamic_cast<SingingClip *>(clip));
        // }
    }
    m_lastEditObjectType = type;
}

void InferControllerPrivate::handleSingingClipInserted(SingingClip *clip) {
    ModelChangeHandler::handleSingingClipInserted(clip);
    clip->reSegment();
    m_clipPieceDict[clip->id()] = Linq::selectMany(clip->pieces(), [](auto p) { return p->id(); });
    createAndRunGetPronTask(clip);
}

void InferControllerPrivate::handleSingingClipRemoved(SingingClip *clip) {
    ModelChangeHandler::handleSingingClipRemoved(clip);
    // TODO: 移除剪辑内的分段推理输入缓存
    m_clipPieceDict.remove(clip->id());
    cancelClipRelatedTasks(clip);
}

void InferControllerPrivate::handleNoteChanged(SingingClip::NoteChangeType type,
                                               const QList<Note *> &notes, SingingClip *clip) {
    switch (type) {
        case SingingClip::Insert:
        case SingingClip::Remove:
        case SingingClip::EditedWordPropertyChange:
        case SingingClip::TimeKeyPropertyChange:
            // 音符发生改动，其所属分段必定需要重新推理。将该分段标记为脏，以便在分段前丢弃
            for (const auto &piece : clip->findPiecesByNotes(notes)) {
                piece->dirty = true;
                cancelPieceRelatedTasks(piece);
            }
            clip->reSegment();
            // cancelClipRelatedTasks(clip);
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
    OriginalParamUtils::updatePronunciation(task->notesRef, task->result, singingClip);
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
    OriginalParamUtils::updatePhoneName(task->notesRef, task->result, singingClip);
    createAndRunInferDurTask(singingClip);
    delete task;
}

void InferControllerPrivate::handleInferDurTaskFinished(InferDurationTask *task) {
    m_inferDurTasks.onCurrentFinished();
    runNextInferDurTask();
    auto clip = appModel->findClipById(task->clipId());
    if (task->terminated() || !task->success() || !clip) {
        delete task;
        return;
    }

    auto singingClip = dynamic_cast<SingingClip *>(appModel->findClipById(task->clipId()));
    auto piece = singingClip->findPieceById(task->pieceId());
    auto modelNoteCount = piece->notes.count();
    auto taskNoteCount = task->result().count();
    if (modelNoteCount != taskNoteCount) {
        // piece->acousticInferStatus = Failed;
        qFatal() << "模型音符数不等于任务音符数"
                 << "模型音符数：" << modelNoteCount << "任务音符数：" << taskNoteCount;
        return;
    }
    // 推理成功，保存本次推理的输入以便之后比较
    m_lastInferDurInputs[task->pieceId()] = task->input();
    OriginalParamUtils::updatePhoneOffset(piece->notes, task->result(), singingClip);
    createAndRunInferPitchTask(*piece);
    // piece->acousticInferStatus = Success;
    delete task;
}

void InferControllerPrivate::handleInferPitchTaskFinished(InferPitchTask *task) {
    m_inferPitchTasks.onCurrentFinished();
    runNextInferPitchTask();
    auto clip = appModel->findClipById(task->clipId());
    if (task->terminated() || !clip) {
        delete task;
        return;
    }

    auto singingClip = dynamic_cast<SingingClip *>(appModel->findClipById(task->clipId()));
    auto piece = singingClip->findPieceById(task->pieceId());
    if (!piece || !task->success()) {
        delete task;
        return;
    }
    if (task->success()) {
        // 推理成功，保存本次推理的输入以便之后比较
        m_lastInferPitchInputs[task->pieceId()] = task->input();
        piece->acousticInferStatus = Success;
    } else {
        piece->acousticInferStatus = Failed;
    }
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
    auto inferDur = [=](InferPiece &piece) {
        QList<InferDurPitNote> inputNotes;
        for (const auto note : piece.notes)
            inputNotes.append(InferDurPitNote(*note));
        const InferDurationTask::InferDurInput input = {clip->id(), piece.id(), inputNotes,
                                                        m_singerConfigPath, appModel->tempo()};
        // 创建分段的推理任务前，首先检查输入是否和上次的相同。如果相同，则直接忽略，避免不必要的推理
        if (m_lastInferDurInputs.contains(piece.id())) {
            if (const auto lastInput = m_lastInferDurInputs[piece.id()]; lastInput == input) {
                // qDebug() << "createAndRunInferDurTask: 输入与上次相同，已取消创建推理任务"
                //          << "pieceId:" << piece.id();
                return;
            }
        }
        // 清空原有的自动参数
        OriginalParamUtils::resetPhoneOffset(piece.notes, clip);
        auto task = new InferDurationTask(input);
        connect(task, &Task::finished, this, [=] { handleInferDurTaskFinished(task); });
        m_inferDurTasks.add(task);
        piece.acousticInferStatus = Running;
        if (!m_inferDurTasks.current)
            runNextInferDurTask();
    };

    if (ValidationUtils::canInferDuration(*clip)) {
        for (const auto piece : clip->pieces())
            inferDur(*piece);
    } else
        qWarning() << "音素序列有错误，无法创建时长推理任务 clipId:" << clip->id();
}

void InferControllerPrivate::createAndRunInferPitchTask(InferPiece &piece) {
    QList<InferDurPitNote> inputNotes;
    for (const auto note : piece.notes)
        inputNotes.append(InferDurPitNote(*note));
    const InferPitchTask::InferPitchInput input = {piece.clipId(), piece.id(), inputNotes,
                                                   m_singerConfigPath, appModel->tempo()};
    if (m_lastInferPitchInputs.contains(piece.id())) {
        if (const auto lastInput = m_lastInferPitchInputs[piece.id()]; lastInput == input) {
            return;
        }
    }
    auto task = new InferPitchTask(input);
    connect(task, &Task::finished, this, [=] { handleInferPitchTaskFinished(task); });
    m_inferPitchTasks.add(task);
    if (!m_inferPitchTasks.current)
        runNextInferPitchTask();
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
void InferControllerPrivate::runNextInferPitchTask(){
    m_inferPitchTasks.runNext();
}