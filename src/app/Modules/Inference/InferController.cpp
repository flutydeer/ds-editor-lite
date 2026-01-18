//
// Created by OrangeCat on 24-9-3.
//

#include "InferController.h"
#include "InferController_p.h"

#include "InferControllerHelper.h"
#include "InferEngine.h"
#include "Model/AppModel/InferPiece.h"
#include "Model/AppOptions/AppOptions.h"
#include "Models/PhonemeNameInput.h"
#include "Tasks/GetPhonemeNameTask.h"
#include "Tasks/GetPronunciationTask.h"
#include "Utils/Linq.h"
#include "Utils/ValidationUtils.h"
#include "Controller/PlaybackController.h"
#include "InferPipeline.h"

namespace Helper = InferControllerHelper;

InferController::InferController(QObject *parent)
    : QObject(parent), d_ptr(new InferControllerPrivate(this)) {
    Q_D(InferController);
    d->m_autoStartAcousticInfer = appOptions->inference()->autoStartInfer;

    connect(appStatus, &AppStatus::moduleStatusChanged, d,
            &InferControllerPrivate::onModuleStatusChanged);
    connect(appOptions, &AppOptions::optionsChanged, d,
            &InferControllerPrivate::onInferOptionChanged);
    connect(playbackController, &PlaybackController::playbackStatusChanged, d,
            &InferControllerPrivate::onPlaybackStatusChanged);
}

InferController::~InferController() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(InferController)

void InferController::addInferDurationTask(InferDurationTask &task) {
    Q_D(InferController);
    d->m_inferDurTasks.add(&task);
    connect(&task, &InferDurationTask::finished, this,
            [d] { d->m_inferDurTasks.onCurrentFinished(); });
}

void InferController::cancelInferDurationTask(int taskId) {
    Q_D(InferController);
    d->m_inferDurTasks.cancelIf(L_PRED(t, t->id() == taskId));
}

void InferController::addInferPitchTask(InferPitchTask &task) {
    Q_D(InferController);
    d->m_inferPitchTasks.add(&task);
    connect(&task, &InferPitchTask::finished, this,
            [d] { d->m_inferPitchTasks.onCurrentFinished(); });
}

void InferController::cancelInferPitchTask(int taskId) {
    Q_D(InferController);
    d->m_inferPitchTasks.cancelIf(L_PRED(t, t->id() == taskId));
}

void InferControllerPrivate::onModuleStatusChanged(const AppStatus::ModuleType module,
                                                   const AppStatus::ModuleStatus status) {
    if (module == AppStatus::ModuleType::Language)
        handleLanguageModuleStatusChanged(status);
}

void InferControllerPrivate::onEditingChanged(const AppStatus::EditObjectType type) {
    // TODO：需要处理编辑被取消的情况
    // 方案：为文档模型相关对象增加版本号。即使用户正在编辑，也不取消相关任务。
    // 任务在执行完成后，如果相关对象（如音符）仍未完成编辑，则进入挂起状态，不直接更新相关对象，等待用户完成操作。
    // 当用户完成编辑，则从挂起状态恢复，分情况处理：
    // 1.
    // 用户提交更改：版本号更新，推理任务的版本号与当前版本号不一致，丢弃任务结果。此时，文档模型的更改会触发重新推理。
    // 2. 用户丢弃更改：版本号不变，应用推理结果
    if (type == AppStatus::EditObjectType::Note) {
        qWarning() << "Editing project. Cancelling related tasks.";
        const auto clip = appModel->findClipById(appStatus->activeClipId);
        if (clip->clipType() == IClip::Singing)
            cancelClipRelatedTasks(reinterpret_cast<SingingClip *>(clip));
    } else if (type == AppStatus::EditObjectType::None) {
    }
    m_lastEditObjectType = type;
}

void InferControllerPrivate::onInferOptionChanged(const AppOptionsGlobal::Option option) {
    if (option != AppOptionsGlobal::All && option != AppOptionsGlobal::Inference)
        return;

    m_autoStartAcousticInfer = appOptions->inference()->autoStartInfer;
    // runInferAcousticIfNeeded();
}

void InferControllerPrivate::onPlaybackStatusChanged(const PlaybackGlobal::PlaybackStatus status) {
    if (status == PlaybackGlobal::Playing) {
        // if (!m_inferAcousticTasks.current)
        // runNextInferAcousticTask();
    }
}

void InferControllerPrivate::handleTempoChanged(double tempo) {
    reset();
    recreateAllInferTasks();
}

void InferControllerPrivate::handleSingingClipInserted(SingingClip *clip) {
    ModelChangeHandler::handleSingingClipInserted(clip);
    connect(clip, &SingingClip::singerChanged, this, [=] { clip->reSegment(); });
}

void InferControllerPrivate::handleSingingClipRemoved(SingingClip *clip) {
    ModelChangeHandler::handleSingingClipRemoved(clip);
    cancelClipRelatedTasks(clip);
}

void InferControllerPrivate::handlePiecesChanged(const PieceList &newPieces,
                                                 const PieceList &discardedPieces,
                                                 SingingClip *clip) {
    m_getPronTasks.cancelIf(L_PRED(t, t->clipId() == clip->id()));
    m_getPhoneTasks.cancelIf(L_PRED(t, t->clipId() == clip->id()));
    for (const auto &piece : discardedPieces) {
        cancelPieceRelatedTasks(piece->id());
        auto pipelines = Linq::where(m_inferPipelines, [piece](const InferPipeline *p) {
            return p->pieceId() == piece->id();
        });
        for (const auto &pipeline : pipelines) {
            m_inferPipelines.removeOne(pipeline);
            delete pipeline;
        }
    }
    Helper::updateAllOriginalParam(*clip);
    if (appStatus->languageModuleStatus == AppStatus::ModuleStatus::Ready)
        createAndRunGetPronTask(*clip);
}

void InferControllerPrivate::handleNoteChanged(const SingingClip::NoteChangeType type,
                                               const QList<Note *> &notes, SingingClip *clip) {
    switch (type) {
        case SingingClip::Insert:
        case SingingClip::Remove:
        case SingingClip::EditedWordPropertyChange:
        case SingingClip::EditedPhonemeOffsetChange:
        case SingingClip::TimeKeyPropertyChange:
            for (const auto &piece : clip->findPiecesByNotes(notes)) {
                piece->dirty = true;
            }
            if (!clip->singerInfo().isEmpty())
                clip->reSegment();
            break;
        default:
            break;
    } // Ignore original word property change
}

void InferControllerPrivate::handleParamChanged(const ParamInfo::Name name, const Param::Type type,
                                                SingingClip *clip) {
    if (type != Param::Edited)
        return;
    auto dirtyPieces = Helper::getParamDirtyPiecesAndUpdateInput(name, *clip);
    switch (name) {
        case ParamInfo::Expressiveness:
            for (const auto &piece : dirtyPieces) {
                auto pred = L_PRED(t, t->pieceId() == piece->id());
                m_inferPitchTasks.cancelIf(pred);
                m_inferVarianceTasks.cancelIf(pred);
                m_inferAcousticTasks.cancelIf(pred);
                auto pipelines = Linq::where(m_inferPipelines, [piece](const InferPipeline *p) {
                    return p->pieceId() == piece->id();
                });
                Q_ASSERT(pipelines.size() == 1);
                pipelines.first()->onExpressivenessChanged();
            }
            break;
        case ParamInfo::Pitch:
            for (const auto &piece : dirtyPieces) {
                auto pred = L_PRED(t, t->pieceId() == piece->id());
                m_inferVarianceTasks.cancelIf(pred);
                m_inferAcousticTasks.cancelIf(pred);
                createAndRunInferVarianceTask(*piece);
            }
            break;
        case ParamInfo::Energy:
        case ParamInfo::Breathiness:
        case ParamInfo::Voicing:
        case ParamInfo::Tension:
        case ParamInfo::MouthOpening:
        case ParamInfo::Gender:
        case ParamInfo::Velocity:
        case ParamInfo::ToneShift:
            for (const auto &piece : dirtyPieces) {
                auto pred = L_PRED(t, t->pieceId() == piece->id());
                m_inferAcousticTasks.cancelIf(pred);
                createAndRunInferAcousticTask(*piece);
            }
            break;
        case ParamInfo::Unknown:
            qFatal() << "Unknown param";
            break;
    }
}

void InferControllerPrivate::handleLanguageModuleStatusChanged(
    const AppStatus::ModuleStatus status) {
    if (status == AppStatus::ModuleStatus::Ready) {
        qDebug() << "Language module is ready. Tasks will be started.";

        for (const auto track : appModel->tracks()) {
            for (const auto clip : track->clips()) {
                if (clip->clipType() == IClip::Singing)
                    createAndRunGetPronTask(*dynamic_cast<SingingClip *>(clip));
            }
        }
    } else if (status == AppStatus::ModuleStatus::Error) {
        m_getPronTasks.disposePendingTasks();
        appOptions->language()->langOrder.clear();
        appOptions->language()->g2pConfigs = QJsonObject();
        appOptions->saveAndNotify(AppOptionsGlobal::Language);
        qCritical() << "Failed to start the language module; tasks have been canceled. "
                       "Restart app to restore default language settings.";
    }
}

void InferControllerPrivate::handleGetPronTaskFinished(GetPronunciationTask &task) {
    m_getPronTasks.onCurrentFinished();
    const auto clip = appModel->findClipById(task.clipId());
    if (task.terminated() || !clip) {
        delete &task;
        return;
    }

    const auto singingClip = dynamic_cast<SingingClip *>(clip);
    // TODO 应该分析出哪些片段受影响，再创建推理管线
    Helper::updatePronunciation(task.notesRef, task.result, *singingClip);
    createAndRunGetPhoneTask(*singingClip);
    delete &task;
}

void InferControllerPrivate::handleGetPhoneTaskFinished(GetPhonemeNameTask &task) {
    m_getPhoneTasks.onCurrentFinished();
    const auto clip = appModel->findClipById(task.clipId());
    if (task.terminated() || !clip) {
        delete &task;
        return;
    }

    const auto singingClip = dynamic_cast<SingingClip *>(clip);
    if (task.success()) {
        Helper::updatePhoneName(task.notesRef, task.result, *singingClip);
        if (ValidationUtils::canInferDuration(*singingClip)) {
            for (const auto piece : singingClip->pieces()) {
                // 只对新的片段创建推理管线
                auto findPipelineById = [](const QList<InferPipeline *> &container,
                                           int id) -> InferPipeline * {
                    for (const auto pipeline : container)
                        if (pipeline->pieceId() == id)
                            return pipeline;
                    return nullptr;
                };
                if (!findPipelineById(m_inferPipelines, piece->id()))
                    createPipeline(*piece);
            }
        } else
            qWarning()
                << "Phoneme sequence has errors, unable to create duration inference task. clipId:"
                << clip->id();
    } else
        for (const auto piece : singingClip->pieces())
            piece->acousticInferStatus = Failed;
    delete &task;
}

void InferControllerPrivate::handleInferVarianceTaskFinished(InferVarianceTask &task) {
    // runNextInferVarianceTask();
    const auto clip = appModel->findClipById(task.clipId());
    if (task.terminated() || !clip) {
        delete &task;
        return;
    }

    const auto singingClip = dynamic_cast<SingingClip *>(appModel->findClipById(task.clipId()));
    const auto piece = singingClip->findPieceById(task.pieceId());
    if (!piece || !task.success()) {
        delete &task;
        return;
    }
    if (task.success()) {
        Helper::updateVariance(task.result(), *piece);
        createAndRunInferAcousticTask(*piece);
    } else {
        piece->acousticInferStatus = Failed;
    }
    delete &task;
}

void InferControllerPrivate::handleInferAcousticTaskFinished(InferAcousticTask &task) {
    // runNextInferAcousticTask();
    const auto clip = appModel->findClipById(task.clipId());
    if (task.terminated() || !clip) {
        delete &task;
        return;
    }

    const auto singingClip = dynamic_cast<SingingClip *>(appModel->findClipById(task.clipId()));
    const auto piece = singingClip->findPieceById(task.pieceId());
    if (!piece) {
        delete &task;
        return;
    }
    if (task.success()) {
        Helper::updateAcoustic(task.result(), *piece);
    } else
        piece->acousticInferStatus = Failed;
    delete &task;
}

void InferControllerPrivate::recreateAllInferTasks() {
    for (const auto &track : appModel->tracks())
        for (const auto &clip : track->clips()) {
            if (clip->clipType() != IClip::Singing)
                continue;
            const auto singingClip = reinterpret_cast<SingingClip *>(clip);
            for (const auto &piece : singingClip->pieces()) {
                Helper::resetPhoneOffset(piece->notes, *piece);
                piece->dirty = true;
            }
        }
}

void InferControllerPrivate::createAndRunGetPronTask(const SingingClip &clip) {
    if (clip.notes().count() <= 0) {
        qDebug() << "createAndRunGetPhoneTask:"
                 << "Note list is empty";
        return;
    }
    auto task = new GetPronunciationTask(clip.id(), clip.notes().toList());
    connect(task, &Task::finished, this, [task, this] { handleGetPronTaskFinished(*task); });
    m_getPronTasks.add(task);
}

void InferControllerPrivate::createAndRunGetPhoneTask(const SingingClip &clip) {
    QList<PhonemeNameInput> inputs;
    for (const auto note : clip.notes())
        inputs.append({note->lyric(), note->language(), note->pronunciation().result()});
    auto task = new GetPhonemeNameTask(clip, inputs);
    task->notesRef = clip.notes().toList();
    connect(task, &Task::finished, this, [task, this] { handleGetPhoneTaskFinished(*task); });
    m_getPhoneTasks.add(task);
}

void InferControllerPrivate::createPipeline(InferPiece &piece) {
    auto pipeline = new InferPipeline(piece);
    m_inferPipelines.append(pipeline);
    pipeline->run();
}

void InferControllerPrivate::createAndRunInferVarianceTask(InferPiece &piece) {
    const auto input = Helper::buildInferVarianceInput(piece, piece.clip->singerIdentifier());
    Helper::resetVariance(piece);
    auto task = new InferVarianceTask(input);
    connect(task, &Task::finished, this, [task, this] { handleInferVarianceTaskFinished(*task); });
    m_inferVarianceTasks.add(task);
    piece.acousticInferStatus = Running;
}

void InferControllerPrivate::createAndRunInferAcousticTask(InferPiece &piece) {
    const auto input = Helper::buildInderAcousticInput(piece, piece.clip->singerIdentifier());
    Helper::resetAcoustic(piece);
    auto task = new InferAcousticTask(input);
    connect(task, &Task::finished, this, [task, this] { handleInferAcousticTaskFinished(*task); });
    m_inferAcousticTasks.add(task);
    piece.acousticInferStatus = Running;
}

void InferControllerPrivate::reset() {
    m_getPronTasks.cancelAll();
    m_getPhoneTasks.cancelAll();
    m_inferDurTasks.cancelAll();
    m_inferPitchTasks.cancelAll();
    m_inferVarianceTasks.cancelAll();
    m_inferAcousticTasks.cancelAll();
}

void InferControllerPrivate::cancelAllInferTasks() {
    for (const auto &track : appModel->tracks())
        for (const auto &clip : track->clips()) {
            if (clip->clipType() != IClip::Singing)
                continue;
            const auto singingClip = reinterpret_cast<SingingClip *>(clip);
            this->cancelClipRelatedTasks(singingClip);
        }
}

void InferControllerPrivate::cancelClipRelatedTasks(const SingingClip *clip) {
    qInfo() << "Cancel singing-clip related tasks" << "clipId:" << clip->id();
    auto pred = L_PRED(t, t->clipId() == clip->id());
    m_getPronTasks.cancelIf(pred);
    m_getPhoneTasks.cancelIf(pred);
    for (const auto piece : clip->pieces())
        cancelPieceRelatedTasks(piece->id());
}

void InferControllerPrivate::cancelPieceRelatedTasks(int pieceId) {
    qInfo() << "Cancel infer-piece related tasks" << "pieceId:" << pieceId;
    auto pred = L_PRED(t, t->pieceId() == pieceId);
    m_inferDurTasks.cancelIf(pred);
    m_inferPitchTasks.cancelIf(pred);
    m_inferVarianceTasks.cancelIf(pred);
    m_inferAcousticTasks.cancelIf(pred);
}