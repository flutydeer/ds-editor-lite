//
// Created by FlutyDeer on 2026/1/3.
//

#include "InferPipeline.h"

#include "States/InferDurationState.h"
#include "States/UpdateDurationState.h"
#include "States/InferPitchState.h"
#include "States/UpdatePitchState.h"
#include "States/InferVarianceState.h"
#include "States/UpdateVarianceState.h"
#include "States/AwaitingInferAcousticState.h"
#include "States/InferAcousticState.h"
#include "States/UpdateAcousticState.h"
#include "Model/AppOptions/AppOptions.h"
#include "Controller/PlaybackController.h"

#include <QFinalState>

InferPipeline::InferPipeline(InferPiece &piece) : QObject(&piece), m_piece(piece) {
    qDebug() << "InferPipeline created: pieceId =" << m_piece.id();
    initStates();
    initTransitions();

    connect(appOptions, &AppOptions::optionsChanged, this, &InferPipeline::onAppOptionsChanged);
    connect(playbackController, &PlaybackController::playbackStatusChanged, this,
            &InferPipeline::onPlaybackStatusChanged);
}

InferPipeline::~InferPipeline() {
    qDebug() << "InferPipeline destroyed: pieceId =" << m_piece.id();
}

int InferPipeline::pieceId() const {
    return m_piece.id();
}

void InferPipeline::run() {
    stateMachine.start();
}

InferPiece &InferPipeline::piece() const {
    return m_piece;
}

const QList<InferInputNote> &InferPipeline::durationResult() const {
    return m_durationResult;
}

void InferPipeline::setDurationResult(const QList<InferInputNote> &result) {
    m_durationResult = result;
}

const InferParamCurve &InferPipeline::pitchResult() const {
    return m_pitchResult;
}

void InferPipeline::setPitchResult(const InferParamCurve &result) {
    m_pitchResult = result;
}

const InferVarianceTask::InferVarianceResult &InferPipeline::varianceResult() const {
    return m_varianceResult;
}

void InferPipeline::setVarianceResult(const InferVarianceTask::InferVarianceResult &result) {
    m_varianceResult = result;
}

void InferPipeline::onExpressivenessChanged() {
    emit expressivenessChanged();
}

void InferPipeline::onPitchChanged() {
    emit pitchChanged();
}

void InferPipeline::onVarianceChanged() {
    emit varianceChanged();
}

void InferPipeline::onAppOptionsChanged(const AppOptionsGlobal::Option option) {
    if (option != AppOptionsGlobal::All && option != AppOptionsGlobal::Inference)
        return;

    if (appOptions->inference()->autoStartInfer)
        emit lazyInferAcousticTurnedOff();
}

void InferPipeline::onPlaybackStatusChanged(PlaybackStatus status) {
    if (status == PlaybackStatus::Playing)
        emit playbackStarted();
}

void InferPipeline::initStates() {
    finalState = new QFinalState();
    inferDurationState = new InferDurationState(*this);
    updateDurationState = new UpdateDurationState(*this);
    inferPitchState = new InferPitchState(*this);
    updatePitchState = new UpdatePitchState(*this);
    inferVarianceState = new InferVarianceState(*this);
    updateVarianceState = new UpdateVarianceState(*this);
    awaitingInferAcousticState = new AwaitingInferAcousticState(*this);
    inferAcousticState = new InferAcousticState(*this);
    updateAcousticState = new UpdateAcousticState(*this);

    stateMachine.addState(finalState);
    stateMachine.addState(inferDurationState);
    stateMachine.addState(updateDurationState);
    stateMachine.addState(inferPitchState);
    stateMachine.addState(updatePitchState);
    stateMachine.addState(inferVarianceState);
    stateMachine.addState(updateVarianceState);
    stateMachine.addState(awaitingInferAcousticState);
    stateMachine.addState(inferAcousticState);
    stateMachine.addState(updateAcousticState);

    stateMachine.setInitialState(inferDurationState);
}

void InferPipeline::initTransitions() {
    initDurationTransitions();
    initPitchTransitions();
    initVarianceTransitions();
    initAcousticTransitions();
}

void InferPipeline::initDurationTransitions() {
    inferDurationState->addTransition(inferDurationState, &InferDurationState::finished,
                                      updateDurationState);
    inferDurationState->addTransition(this, &InferPipeline::pieceRemoved, finalState);

    updateDurationState->addTransition(updateDurationState, &UpdateDurationState::updateSuccess,
                                       inferPitchState);
    updateDurationState->addTransition(updateDurationState, &UpdateDurationState::pieceNotFound,
                                       finalState);
}

void InferPipeline::initPitchTransitions() {
    inferPitchState->addTransition(inferPitchState, &InferPitchState::finished, updatePitchState);
    inferPitchState->addTransition(this, &InferPipeline::pieceRemoved, finalState);
    inferPitchState->addTransition(this, &InferPipeline::expressivenessChanged, inferPitchState);

    updatePitchState->addTransition(updatePitchState, &UpdatePitchState::updateSuccess,
                                    inferVarianceState);
    updatePitchState->addTransition(updatePitchState, &UpdatePitchState::pieceNotFound, finalState);
    // TODO 音高步数更改
}

void InferPipeline::initVarianceTransitions() {
    inferVarianceState->addTransition(inferVarianceState, &InferVarianceState::finished,
                                      updateVarianceState);
    inferVarianceState->addTransition(this, &InferPipeline::pieceRemoved, finalState);
    inferVarianceState->addTransition(this, &InferPipeline::expressivenessChanged, inferPitchState);
    inferVarianceState->addTransition(this, &InferPipeline::pitchChanged, inferVarianceState);

    updateVarianceState->addTransition(updateVarianceState,
                                       &UpdateVarianceState::updateSuccessWithLazyInference,
                                       awaitingInferAcousticState);
    updateVarianceState->addTransition(updateVarianceState,
                                       &UpdateVarianceState::updateSuccessWithImmediateInference,
                                       inferAcousticState);
    updateVarianceState->addTransition(updateVarianceState, &UpdateVarianceState::pieceNotFound,
                                       finalState);
}

void InferPipeline::initAcousticTransitions() {
    awaitingInferAcousticState->addTransition(this, &InferPipeline::playbackStarted,
                                              inferAcousticState);
    awaitingInferAcousticState->addTransition(this, &InferPipeline::lazyInferAcousticTurnedOff,
                                              inferAcousticState);
    awaitingInferAcousticState->addTransition(this, &InferPipeline::pieceRemoved, finalState);
    awaitingInferAcousticState->addTransition(this, &InferPipeline::expressivenessChanged,
                                              inferPitchState);
    awaitingInferAcousticState->addTransition(this, &InferPipeline::pitchChanged,
                                              inferVarianceState);
    awaitingInferAcousticState->addTransition(this, &InferPipeline::varianceChanged,
                                              inferAcousticState);

    inferAcousticState->addTransition(inferAcousticState, &InferAcousticState::finished,
                                      updateAcousticState);
    inferAcousticState->addTransition(this, &InferPipeline::pieceRemoved, finalState);
    inferAcousticState->addTransition(this, &InferPipeline::expressivenessChanged, inferPitchState);
    inferAcousticState->addTransition(this, &InferPipeline::pitchChanged, inferVarianceState);
    inferAcousticState->addTransition(this, &InferPipeline::varianceChanged, inferAcousticState);

    updateAcousticState->addTransition(updateAcousticState, &UpdateAcousticState::updateSuccess,
                                       playbackReadyState);
    updateAcousticState->addTransition(updateAcousticState, &UpdateAcousticState::pieceNotFound,
                                       finalState);
}
