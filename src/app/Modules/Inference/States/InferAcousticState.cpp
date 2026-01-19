//
// Created by FlutyDeer on 2026/1/3.
//

#include "InferAcousticState.h"

#include "Modules/Inference/InferPipeline.h"
#include "Modules/Inference/InferControllerHelper.h"

#include <QTimer>
#include <QDebug>
#include <QFinalState>

InferAcousticState::InferAcousticState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
    m_runningInferenceState = new QState(this);
    m_awaitingModelReleaseState = new QState(this);
    m_errorState = new QState(this);
    m_finalState = new QFinalState(this);

    setInitialState(m_runningInferenceState);

    connect(m_runningInferenceState, &QState::entered, this,
            &InferAcousticState::onRunningInferenceStateEntered);

    connect(m_awaitingModelReleaseState, &QState::entered, this,
            &InferAcousticState::onAwaitingModelReleaseStateEntered);

    connect(m_errorState, &QState::entered, this, &InferAcousticState::onErrorStateEntered);

    m_runningInferenceState->addTransition(this, &InferAcousticState::taskSuccessWithModelLocked,
                                           m_awaitingModelReleaseState);
    m_runningInferenceState->addTransition(this, &InferAcousticState::taskFailed, m_errorState);
    m_runningInferenceState->addTransition(this, &InferAcousticState::moveToReadyState,
                                           m_finalState);

    m_awaitingModelReleaseState->addTransition(this, &InferAcousticState::moveToReadyState,
                                               m_finalState);
}

void InferAcousticState::onEntry(QEvent *event) {
    qDebug() << "InferAcousticState::onEntry";
    QState::onEntry(event);
}

void InferAcousticState::onExit(QEvent *event) {
    qDebug() << "InferAcousticState::onExit";
    QState::onExit(event);
}

void InferAcousticState::onRunningInferenceStateEntered() {
    qDebug() << "InferAcousticState::onRunningInferenceStateEntered";

    auto &piece = m_pipeline.piece();
    piece.acousticInferStatus = Running;
    piece.state = QString("Acoustic.Running");
    QTimer::singleShot(1000, this, &InferAcousticState::taskFailed);
}

void InferAcousticState::onAwaitingModelReleaseStateEntered() {
    qDebug() << "InferAcousticState::onAwaitingModelReleaseStateEntered";
    QTimer::singleShot(1000, this, &InferAcousticState::moveToReadyState);
}

void InferAcousticState::onErrorStateEntered() {
    qDebug() << "InferAcousticState::onErrorStateEntered";
    
    auto &piece = m_pipeline.piece();
    piece.acousticInferStatus = Failed;
    piece.state = QString("Acoustic.Error");
}