//
// Created by FlutyDeer on 2026/1/3.
//

#include "InferAcousticState.h"

#include "Modules/Inference/InferPipeline.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferController.h"
#include "Model/AppModel/AppModel.h"

#include <QTimer>
#include <QDebug>
#include <QFinalState>

namespace Helper = InferControllerHelper;

InferAcousticState::InferAcousticState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
    m_runningInferenceState = new QState(this);
    m_awaitingModelReleaseState = new QState(this);
    m_errorState = new QState(this);
    m_finalState = new QFinalState(this);

    setInitialState(m_runningInferenceState);

    connect(m_runningInferenceState, &QState::entered, this,
            &InferAcousticState::onRunningInferenceStateEntered);
    connect(m_runningInferenceState, &QState::exited, this,
            &InferAcousticState::onRunningInferenceStateExited);

    connect(m_awaitingModelReleaseState, &QState::entered, this,
            &InferAcousticState::onAwaitingModelReleaseStateEntered);

    connect(m_errorState, &QState::entered, this, &InferAcousticState::onErrorStateEntered);

    m_runningInferenceState->addTransition(this, &InferAcousticState::taskSuccessWithModelLocked,
                                           m_awaitingModelReleaseState);
    m_runningInferenceState->addTransition(this, &InferAcousticState::failed, m_errorState);
    m_runningInferenceState->addTransition(this, &InferAcousticState::ready, m_finalState);

    m_awaitingModelReleaseState->addTransition(this, &InferAcousticState::ready, m_finalState);
}

void InferAcousticState::onEntry(QEvent *event) {
    qDebug() << "InferAcousticState::onEntry";
    QState::onEntry(event);
}

void InferAcousticState::onExit(QEvent *event) {
    qDebug() << "InferAcousticState::onExit";
    QState::onExit(event);
}

void InferAcousticState::onRunningInferenceStateExited() {
    qDebug() << "InferAcousticState::onRunningInferenceStateExited";
    if (!currentTask)
        return;

    inferController->cancelInferAcousticTask(currentTask->id());
    // TODO: BUG 任务未完成时直接删除会引起崩溃
    // delete currentTask;
    currentTask = nullptr;
}

void InferAcousticState::onRunningInferenceStateEntered() {
    qDebug() << "InferAcousticState::onRunningInferenceStateEntered";
    // Reset task
    if (currentTask) {
        inferController->cancelInferAcousticTask(currentTask->id());
        // TODO: BUG 任务未完成时直接删除会引起崩溃
        // delete currentTask;
        currentTask = nullptr;
    }

    auto &piece = m_pipeline.piece();
    piece.acousticInferStatus = Running;
    piece.state = QString("Acoustic.Running");
    const auto input = Helper::buildInferAcousticInput(piece, piece.clip->singerIdentifier());
    Helper::resetAcoustic(piece);
    auto task = new InferAcousticTask(input);
    connect(task, &Task::finished, this, [this, task] { handleTaskFinished(*task); });
    inferController->addInferAcousticTask(*task);
    currentTask = task;
}

void InferAcousticState::onAwaitingModelReleaseStateEntered() {
    qDebug() << "InferAcousticState::onAwaitingModelReleaseStateEntered";
}

void InferAcousticState::onErrorStateEntered() {
    qDebug() << "InferAcousticState::onErrorStateEntered";

    auto &piece = m_pipeline.piece();
    piece.acousticInferStatus = Failed;
    piece.state = QString("Acoustic.Error");
}

void InferAcousticState::handleTaskFinished(InferAcousticTask &task) {
    inferController->finishCurrentInferAcousticTask();

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
        m_pipeline.setAcousticResult(task.result());
        emit ready();
    } else {
        emit failed();
    }
}
