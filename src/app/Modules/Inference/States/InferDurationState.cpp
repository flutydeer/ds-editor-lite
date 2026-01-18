//
// Created by FlutyDeer on 2026/1/3.
//

#include "InferDurationState.h"

#include "Modules/Inference/Tasks/InferDurationTask.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferPipeline.h"
#include "Model/AppModel/AppModel.h"
#include "Modules/Inference/InferController.h"

#include <QTimer>
#include <QDebug>
#include <QFinalState>
#include <memory>

namespace Helper = InferControllerHelper;

InferDurationState::InferDurationState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
    m_runningInferenceState = new QState(this);
    m_awaitingModelReleaseState = new QState(this);
    m_errorState = new QState(this);
    m_finalState = new QFinalState(this);

    setInitialState(m_runningInferenceState);

    connect(m_runningInferenceState, &QState::entered, this,
            &InferDurationState::onRunningInferenceStateEntered);

    connect(m_awaitingModelReleaseState, &QState::entered, this,
            &InferDurationState::onAwaitingModelReleaseStateEntered);

    connect(m_errorState, &QState::entered, this, &InferDurationState::onErrorStateEntered);

    m_runningInferenceState->addTransition(this, &InferDurationState::successWithModelLocked,
                                           m_awaitingModelReleaseState);
    m_runningInferenceState->addTransition(this, &InferDurationState::failed, m_errorState);
    m_runningInferenceState->addTransition(this, &InferDurationState::ready, m_finalState);

    m_awaitingModelReleaseState->addTransition(this, &InferDurationState::ready, m_finalState);
}

void InferDurationState::onEntry(QEvent *event) {
    qDebug() << "InferDurationState::onEntry";
    QState::onEntry(event);
}

void InferDurationState::onExit(QEvent *event) {
    qDebug() << "InferDurationState::onExit";
    QState::onExit(event);
}

void InferDurationState::onRunningInferenceStateEntered() {
    qDebug() << "InferDurationState::onRunningInferenceStateEntered";
    // Reset task
    if (taskId != -1)
        inferController->cancelInferDurationTask(taskId);

    auto &piece = m_pipeline.piece();
    piece.acousticInferStatus = Running;
    piece.state = QString("Duration.Running");
    const auto input = Helper::buildInferDurInput(piece, piece.clip->singerIdentifier());
    Helper::resetPhoneOffset(piece.notes, piece);
    auto task = new InferDurationTask(input);
    connect(task, &Task::finished, this, [this, task] { handleTaskFinished(*task); });
    inferController->addInferDurationTask(*task);
    taskId = task->id();
}

void InferDurationState::onAwaitingModelReleaseStateEntered() {
    qDebug() << "InferDurationState::onAwaitingModelReleaseStateEntered";
    // QTimer::singleShot(1000, this, &InferDurationState::ready);
}

void InferDurationState::onErrorStateEntered() {
    qDebug() << "InferDurationState::onErrorStateEntered";
    m_pipeline.piece().acousticInferStatus = Failed;
    m_pipeline.piece().state = QString("Duration.Error");
}

void InferDurationState::handleTaskFinished(InferDurationTask &task) {
    const auto clip = appModel->findClipById(task.clipId());
    if (task.terminated() || !clip) {
        return;
    }

    const auto singingClip = dynamic_cast<SingingClip *>(appModel->findClipById(task.clipId()));
    const auto piece = singingClip->findPieceById(task.pieceId());
    if (!piece) {
        return;
    }

    if (task.success()) {
        const auto modelNoteCount = piece->notes.count();
        const auto taskNoteCount = task.result().count();
        if (modelNoteCount != taskNoteCount) {
            qFatal() << "Model note count does not equal task note count"
                     << "Model note count:" << modelNoteCount
                     << "Task note count:" << taskNoteCount;
            emit failed();
            return;
        }
        // TODO: 等待 AppModel 释放
        m_pipeline.setDurationResult(task.result());
        emit ready();
    } else
        emit failed();
}