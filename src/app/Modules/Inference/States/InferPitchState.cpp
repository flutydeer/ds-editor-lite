//
// Created by FlutyDeer on 2026/1/3.
//

#include "InferPitchState.h"

#include "Modules/Inference/InferController.h"
#include "Modules/Inference/InferPipeline.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Model/AppModel/AppModel.h"

#include <QTimer>
#include <QDebug>
#include <QFinalState>

namespace Helper = InferControllerHelper;

InferPitchState::InferPitchState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
    m_runningInferenceState = new QState(this);
    m_awaitingModelReleaseState = new QState(this);
    m_errorState = new QState(this);
    m_finalState = new QFinalState(this);

    setInitialState(m_runningInferenceState);

    connect(m_runningInferenceState, &QState::entered, this,
            &InferPitchState::onRunningInferenceStateEntered);
    connect(m_runningInferenceState, &QState::exited, this,
            &InferPitchState::onRunningInferenceStateExited);

    connect(m_awaitingModelReleaseState, &QState::entered, this,
            &InferPitchState::onAwaitingModelReleaseStateEntered);

    connect(m_errorState, &QState::entered, this, &InferPitchState::onErrorStateEntered);

    m_runningInferenceState->addTransition(this, &InferPitchState::taskSuccessWithModelLocked,
                                           m_awaitingModelReleaseState);
    m_runningInferenceState->addTransition(this, &InferPitchState::failed, m_errorState);
    m_runningInferenceState->addTransition(this, &InferPitchState::ready, m_finalState);
    m_awaitingModelReleaseState->addTransition(this, &InferPitchState::ready, m_finalState);
}

void InferPitchState::onEntry(QEvent *event) {
    qDebug() << "InferPitchState::onEntry";
    QState::onEntry(event);
}

void InferPitchState::onExit(QEvent *event) {
    qDebug() << "InferPitchState::onExit";
    QState::onExit(event);
}

void InferPitchState::onRunningInferenceStateExited() {
    qDebug() << "InferPitchState::onRunningInferenceStateExited";
    if (!currentTask)
        return;

    currentTask->disconnect(this);  // Disconnect from state machine
    inferController->cancelInferPitchTask(currentTask->id());
    currentTask = nullptr;
}

void InferPitchState::onRunningInferenceStateEntered() {
    qDebug() << "InferPitchState::onRunningInferenceStateEntered";
    // Reset task
    if (currentTask) {
        currentTask->disconnect(this);  // Disconnect from state machine
        inferController->cancelInferPitchTask(currentTask->id());
        currentTask = nullptr;
    }

    auto &piece = m_pipeline.piece();
    piece.acousticInferStatus = Running;
    piece.state = QString("Pitch.Running");
    const auto input = Helper::buildInferPitchInput(piece, piece.clip->singerIdentifier());
    Helper::resetPitch(piece);
    auto task = new InferPitchTask(input);
    connect(task, &Task::finished, this, [this, task] { handleTaskFinished(*task); });
    inferController->addInferPitchTask(*task);
    currentTask = task;
}

void InferPitchState::onAwaitingModelReleaseStateEntered() {
    qDebug() << "InferPitchState::onAwaitingModelReleaseStateEntered";
}

void InferPitchState::onErrorStateEntered() {
    qDebug() << "InferPitchState::onErrorStateEntered";

    m_pipeline.piece().acousticInferStatus = Failed;
    m_pipeline.piece().state = QString("Pitch.Error");
}

void InferPitchState::handleTaskFinished(InferPitchTask &task) {
    // Only handle tasks that are still connected to this state machine
    if (!currentTask || currentTask != &task) {
        qDebug() << "Ignoring finished task that is no longer current";
        return;
    }

    inferController->finishCurrentInferPitchTask();

    const auto clip = appModel->findClipById(task.clipId());
    if (task.terminated() || !clip) {
        qDebug() << "Task terminated or clip not found, cleaning up";
        delete currentTask;
        currentTask = nullptr;
        return;
    }

    const auto singingClip = dynamic_cast<SingingClip *>(appModel->findClipById(task.clipId()));
    const auto piece = singingClip->findPieceById(task.pieceId());
    if (!piece) {
        qDebug() << "Piece not found, cleaning up";
        delete currentTask;
        currentTask = nullptr;
        return;
    }

    if (task.success()) {
        // TODO: 等待 AppModel 释放
        m_pipeline.setPitchResult(task.result());
        emit ready();
    } else {
        emit failed();
    }

    // Clean up the task after handling
    delete currentTask;
    currentTask = nullptr;
}