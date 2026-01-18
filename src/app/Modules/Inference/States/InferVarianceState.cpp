//
// Created by FlutyDeer on 2026/1/3.
//

#include "InferVarianceState.h"

#include <QTimer>
#include <QDebug>
#include <QFinalState>

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
    m_runningInferenceState->addTransition(this, &InferVarianceState::taskFailed, m_errorState);
    m_runningInferenceState->addTransition(this, &InferVarianceState::moveToReadyState,
                                           m_finalState);

    m_awaitingModelReleaseState->addTransition(this, &InferVarianceState::moveToReadyState,
                                               m_finalState);
}

void InferVarianceState::onEntry(QEvent *event) {
    qDebug() << "InferVarianceState::onEntry";
    QState::onEntry(event);
}

void InferVarianceState::onExit(QEvent *event) {
    qDebug() << "InferVarianceState::onExit";
    QState::onExit(event);
}

void InferVarianceState::onRunningInferenceStateEntered() {
    qDebug() << "InferVarianceState::onRunningInferenceStateEntered";
    QTimer::singleShot(1000, this, &InferVarianceState::taskFailed);
}

void InferVarianceState::onAwaitingModelReleaseStateEntered() {
    qDebug() << "InferVarianceState::onAwaitingModelReleaseStateEntered";
    QTimer::singleShot(1000, this, &InferVarianceState::moveToReadyState);
}

void InferVarianceState::onErrorStateEntered() {
    qDebug() << "InferVarianceState::onErrorStateEntered";
}