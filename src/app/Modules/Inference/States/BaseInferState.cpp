//
// Created by FlutyDeer on 2026/4/25.
//

#include "BaseInferState.h"
#include "Modules/Inference/InferPipeline.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Model/AppModel/SingingClip.h"
#include "Modules/Inference/Tasks/IInferTask.h"

#include <QTimer>
#include <QDebug>
#include <QFinalState>

BaseInferState::BaseInferState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
    m_runningInferenceState = new QState(this);
    m_awaitingModelReleaseState = new QState(this);
    m_errorState = new QState(this);
    m_finalState = new QFinalState(this);

    setInitialState(m_runningInferenceState);

    connect(m_runningInferenceState, &QState::entered, this,
            &BaseInferState::onRunningInferenceStateEntered);
    connect(m_runningInferenceState, &QState::exited, this,
            &BaseInferState::onRunningInferenceStateExited);

    connect(m_awaitingModelReleaseState, &QState::entered, this,
            &BaseInferState::onAwaitingModelReleaseStateEntered);

    connect(m_errorState, &QState::entered, this, &BaseInferState::onErrorStateEntered);

    m_runningInferenceState->addTransition(this, &BaseInferState::taskSuccessWithModelLocked,
                                           m_awaitingModelReleaseState);
    m_runningInferenceState->addTransition(this, &BaseInferState::failed, m_errorState);
    m_runningInferenceState->addTransition(this, &BaseInferState::ready, m_finalState);

    m_awaitingModelReleaseState->addTransition(this, &BaseInferState::ready, m_finalState);
}

void BaseInferState::onEntry(QEvent *event) {
    qDebug() << "BaseInferState::onEntry";
    QState::onEntry(event);
}

void BaseInferState::onExit(QEvent *event) {
    qDebug() << "BaseInferState::onExit";
    QState::onExit(event);
}

void BaseInferState::onRunningInferenceStateEntered() {
    qDebug() << "BaseInferState::onRunningInferenceStateEntered";
    if (currentTask) {
        currentTask->disconnect(this);
        cancelTaskInController(currentTask->id());
        currentTask = nullptr;
    }

    auto &piece = m_pipeline.piece();
    piece.acousticInferStatus = Running;
    piece.state = QString("%1.Running").arg(getStateNamePrefix());

    resetState();

    ++m_preparationEpoch;
    buildTaskInput();
    auto *task = createTask();
    connect(task, &IInferTask::finished, this, [this, task] { handleTaskFinished(*task); });
    addTaskToController(task);
    currentTask = task;
}

void BaseInferState::onRunningInferenceStateExited() {
    qDebug() << "BaseInferState::onRunningInferenceStateExited";
    m_preparationEpoch++;
    if (!currentTask)
        return;

    currentTask->disconnect(this);
    cancelTaskInController(currentTask->id());
    currentTask = nullptr;
}

void BaseInferState::onAwaitingModelReleaseStateEntered() {
    qDebug() << "BaseInferState::onAwaitingModelReleaseStateEntered";
}

void BaseInferState::onErrorStateEntered() {
    qDebug() << "BaseInferState::onErrorStateEntered";
    auto &piece = m_pipeline.piece();
    piece.acousticInferStatus = Failed;
    piece.state = QString("%1.Error").arg(getStateNamePrefix());
}

void BaseInferState::handleTaskFinished(IInferTask &task) {
    if (!currentTask || currentTask != &task) {
        qDebug() << "Ignoring finished task that is no longer current";
        return;
    }

    const auto finishCurrentTask = [this, &task] {
        if (!finishTaskInController(&task))
            qWarning() << "Failed to finish task because it is not controller queue current"
                       << "taskId:" << task.id();
        currentTask = nullptr;
    };

    if (task.terminated()) {
        qDebug() << "Task terminated, cleaning up";
        finishCurrentTask();
        return;
    }

    if (!task.success()) {
        finishCurrentTask();
        QTimer::singleShot(0, this, [this] { emit failed(); });
        return;
    }

    m_pipeline.setApplyContext(task.inferenceContext());
    const auto gate = m_pipeline.resolveApplyContext(-1, false);
    if (gate.decision != InferenceApplyGate::Decision::Apply) {
        m_pipeline.notifyDropped(gate.reason);
        finishCurrentTask();
        QTimer::singleShot(0, this, [this] { emit dropped(); });
        return;
    }

    if (!validateTaskResult(&task, gate.resolution.clip)) {
        m_pipeline.notifyDropped("task-result-invalid");
        finishCurrentTask();
        QTimer::singleShot(0, this, [this] { emit dropped(); });
        return;
    }

    setTaskResultToPipeline(&task);
    finishCurrentTask();
    QTimer::singleShot(0, this, [this] { emit ready(); });
}
