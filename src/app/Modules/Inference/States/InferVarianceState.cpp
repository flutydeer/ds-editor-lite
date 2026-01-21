//
// Created by FlutyDeer on 2026/1/3.
//

#include "InferVarianceState.h"

#include "Modules/Inference/InferController.h"
#include "Modules/Inference/InferPipeline.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/Tasks/InferVarianceTask.h"
#include "Model/AppModel/AppModel.h"

#include <QTimer>
#include <QDebug>
#include <QFinalState>

namespace Helper = InferControllerHelper;

InferVarianceState::InferVarianceState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
    m_runningInferenceState = new QState(this);
    m_awaitingModelReleaseState = new QState(this);
    m_errorState = new QState(this);
    m_finalState = new QFinalState(this);

    setInitialState(m_runningInferenceState);

    connect(m_runningInferenceState, &QState::entered, this,
            &InferVarianceState::onRunningInferenceStateEntered);

    connect(m_awaitingModelReleaseState, &QState::entered, this,
            &InferVarianceState::onAwaitingModelReleaseStateEntered);

    connect(m_errorState, &QState::entered, this, &InferVarianceState::onErrorStateEntered);

    m_runningInferenceState->addTransition(this, &InferVarianceState::taskSuccessWithModelLocked,
                                           m_awaitingModelReleaseState);
    m_runningInferenceState->addTransition(this, &InferVarianceState::failed, m_errorState);
    m_runningInferenceState->addTransition(this, &InferVarianceState::ready, m_finalState);

    m_awaitingModelReleaseState->addTransition(this, &InferVarianceState::ready, m_finalState);
}

void InferVarianceState::onEntry(QEvent *event) {
    qDebug() << "InferVarianceState::onEntry";
    QState::onEntry(event);
}

void InferVarianceState::onExit(QEvent *event) {
    qDebug() << "InferVarianceState::onExit";
    // TODO: clean current task
    QState::onExit(event);
}

void InferVarianceState::onRunningInferenceStateEntered() {
    qDebug() << "InferVarianceState::onRunningInferenceStateEntered";

    if (taskId != -1) {
        inferController->cancelInferVarianceTask(taskId);
    }

    auto &piece = m_pipeline.piece();
    piece.acousticInferStatus = Running;
    piece.state = QString("Variance.Running");
    const auto input = Helper::buildInferVarianceInput(piece, piece.clip->singerIdentifier());
    Helper::resetVariance(piece);
    auto task = new InferVarianceTask(input);
    connect(task, &Task::finished, this, [task, this] { handleTaskFinished(*task); });
    inferController->addInferVarianceTask(*task);
    taskId = task->id();
}

void InferVarianceState::onAwaitingModelReleaseStateEntered() {
    qDebug() << "InferVarianceState::onAwaitingModelReleaseStateEntered";
    // QTimer::singleShot(1000, this, &InferVarianceState::ready);
}

void InferVarianceState::onErrorStateEntered() {
    qDebug() << "InferVarianceState::onErrorStateEntered";
    auto &piece = m_pipeline.piece();
    piece.acousticInferStatus = Failed;
    piece.state = QString("Variance.Error");
}

void InferVarianceState::handleTaskFinished(InferVarianceTask &task) {
    qDebug() << "InferVarianceState::handleTaskFinished";

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
        // TODO: 等待 AppModel 释放
        m_pipeline.setVarianceResult(task.result());
        emit ready();
    } else {
        emit failed();
    }
}