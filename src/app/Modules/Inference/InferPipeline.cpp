//
// Created by FlutyDeer on 2026/1/3.
//

#include "InferPipeline.h"

#include "States/InferDurationState.h"
#include "States/UpdateDurationState.h"
#include "States/AwaitingEditSessionApplyState.h"
#include "States/InferPitchState.h"
#include "States/UpdatePitchState.h"
#include "States/InferVarianceState.h"
#include "States/UpdateVarianceState.h"
#include "States/AwaitingInferAcousticState.h"
#include "States/InferAcousticState.h"
#include "States/UpdateAcousticState.h"
#include "States/PlaybackReadyState.h"
#include "Model/AppOptions/AppOptions.h"
#include "Utils/ConditionalTransition.h"
#include "Utils/InferenceApplyGate.h"

#include <QDebug>
#include <QFinalState>

InferPipeline::InferPipeline(InferPiece &piece) : QObject(&piece), m_piece(piece) {
    qDebug() << "InferPipeline created: pieceId =" << m_piece.id();
    initStates();
    initTransitions();

    connect(appOptions, &AppOptions::optionsChanged, this, &InferPipeline::onAppOptionsChanged);
}

InferPipeline::~InferPipeline() {
    qDebug() << "InferPipeline destroyed: pieceId =" << m_piece.id();
}

int InferPipeline::pieceId() const {
    return m_piece.id();
}

int InferPipeline::clipId() const {
    return m_piece.clipId();
}

void InferPipeline::run() {
    stateMachine.start();
}

InferPiece &InferPipeline::piece() const {
    return m_piece;
}

const InferenceTaskContext &InferPipeline::applyContext() const {
    return m_applyContext;
}

void InferPipeline::setApplyContext(const InferenceTaskContext &context) {
    m_applyContext = context;
}

void InferPipeline::clearApplyContext() {
    m_applyContext = {};
}

InferPipeline::ApplyGateResult
    InferPipeline::resolveApplyContext(const qsizetype expectedNoteCount,
                                       const bool checkEditSession) const {
    InferenceApplyGate::Options options;
    options.phase = checkEditSession ? "pipeline-update" : "pipeline-task-finished";
    options.expectedNoteCount = expectedNoteCount;
    options.checkEditSession = checkEditSession;
    options.allowUnchangedPieceRevisionMismatch = true;

    ApplyGateResult result;
    result.decision = InferenceApplyGate::resolve(m_applyContext, result.resolution, options);
    result.reason = result.resolution.dropReason;
    return result;
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

[[nodiscard]] const QString &InferPipeline::acousticResult() const {
    return m_acousticResult;
}

void InferPipeline::setAcousticResult(const QString &result) {
    m_acousticResult = result;
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

void InferPipeline::notifyPlaybackStarted() {
    emit playbackStarted();
}

void InferPipeline::initStates() {
    finalState = new QFinalState();
    inferDurationState = new InferDurationState(*this);
    updateDurationState = new UpdateDurationState(*this);
    awaitingDurationApplyState =
        new AwaitingEditSessionApplyState(*this, AwaitingEditSessionApplyState::Stage::Duration);
    inferPitchState = new InferPitchState(*this);
    updatePitchState = new UpdatePitchState(*this);
    awaitingPitchApplyState =
        new AwaitingEditSessionApplyState(*this, AwaitingEditSessionApplyState::Stage::Pitch);
    inferVarianceState = new InferVarianceState(*this);
    updateVarianceState = new UpdateVarianceState(*this);
    awaitingVarianceApplyState =
        new AwaitingEditSessionApplyState(*this, AwaitingEditSessionApplyState::Stage::Variance);
    awaitingInferAcousticState = new AwaitingInferAcousticState(*this);
    inferAcousticState = new InferAcousticState(*this);
    updateAcousticState = new UpdateAcousticState(*this);
    awaitingAcousticApplyState =
        new AwaitingEditSessionApplyState(*this, AwaitingEditSessionApplyState::Stage::Acoustic);
    playbackReadyState = new PlaybackReadyState(*this);

    stateMachine.addState(finalState);
    stateMachine.addState(inferDurationState);
    stateMachine.addState(updateDurationState);
    stateMachine.addState(awaitingDurationApplyState);
    stateMachine.addState(inferPitchState);
    stateMachine.addState(updatePitchState);
    stateMachine.addState(awaitingPitchApplyState);
    stateMachine.addState(inferVarianceState);
    stateMachine.addState(updateVarianceState);
    stateMachine.addState(awaitingVarianceApplyState);
    stateMachine.addState(awaitingInferAcousticState);
    stateMachine.addState(inferAcousticState);
    stateMachine.addState(updateAcousticState);
    stateMachine.addState(awaitingAcousticApplyState);
    stateMachine.addState(playbackReadyState);

    stateMachine.setInitialState(inferDurationState);
}

void InferPipeline::initTransitions() {
    initDurationTransitions();
    initPitchTransitions();
    initVarianceTransitions();
    initAwaitingInferAcousticTransitions();
    initAcousticTransitions();
    initPlaybackReadyTransitions();
}

void InferPipeline::initDurationTransitions() {
    inferDurationState->addTransition(inferDurationState, &InferDurationState::finished,
                                      updateDurationState);
    inferDurationState->addTransition(inferDurationState, &InferDurationState::dropped, finalState);
    inferDurationState->addTransition(this, &InferPipeline::pieceRemoved, finalState);

    updateDurationState->addTransition(updateDurationState, &UpdateDurationState::updateSuccess,
                                       inferPitchState);
    updateDurationState->addTransition(updateDurationState, &UpdateDurationState::pieceNotFound,
                                       finalState);
    updateDurationState->addTransition(updateDurationState, &UpdateDurationState::deferred,
                                       awaitingDurationApplyState);

    awaitingDurationApplyState->addTransition(awaitingDurationApplyState,
                                              &AwaitingEditSessionApplyState::resumeRequested,
                                              updateDurationState);
    awaitingDurationApplyState->addTransition(this, &InferPipeline::pieceRemoved, finalState);
}

void InferPipeline::initPitchTransitions() {
    inferPitchState->addTransition(inferPitchState, &InferPitchState::finished, updatePitchState);
    inferPitchState->addTransition(inferPitchState, &InferPitchState::dropped, finalState);
    inferPitchState->addTransition(this, &InferPipeline::pieceRemoved, finalState);
    inferPitchState->addTransition(this, &InferPipeline::expressivenessChanged, inferPitchState);

    updatePitchState->addTransition(updatePitchState, &UpdatePitchState::updateSuccess,
                                    inferVarianceState);
    updatePitchState->addTransition(updatePitchState, &UpdatePitchState::pieceNotFound, finalState);
    updatePitchState->addTransition(updatePitchState, &UpdatePitchState::deferred,
                                    awaitingPitchApplyState);
    awaitingPitchApplyState->addTransition(
        awaitingPitchApplyState, &AwaitingEditSessionApplyState::resumeRequested, updatePitchState);
    awaitingPitchApplyState->addTransition(this, &InferPipeline::pieceRemoved, finalState);
    awaitingPitchApplyState->addTransition(this, &InferPipeline::expressivenessChanged,
                                           inferPitchState);
    // TODO 音高步数更改
}

void InferPipeline::initVarianceTransitions() {
    inferVarianceState->addTransition(inferVarianceState, &InferVarianceState::finished,
                                      updateVarianceState);
    inferVarianceState->addTransition(inferVarianceState, &InferVarianceState::dropped, finalState);
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
    updateVarianceState->addTransition(updateVarianceState, &UpdateVarianceState::deferred,
                                       awaitingVarianceApplyState);
    awaitingVarianceApplyState->addTransition(awaitingVarianceApplyState,
                                              &AwaitingEditSessionApplyState::resumeRequested,
                                              updateVarianceState);
    awaitingVarianceApplyState->addTransition(this, &InferPipeline::pieceRemoved, finalState);
    awaitingVarianceApplyState->addTransition(this, &InferPipeline::expressivenessChanged,
                                              inferPitchState);
    awaitingVarianceApplyState->addTransition(this, &InferPipeline::pitchChanged,
                                              inferVarianceState);
}

void InferPipeline::initAwaitingInferAcousticTransitions() {
    awaitingInferAcousticState->addTransition(this, &InferPipeline::playbackStarted,
                                              inferAcousticState);
    awaitingInferAcousticState->addTransition(this, &InferPipeline::lazyInferAcousticTurnedOff,
                                              inferAcousticState);
    awaitingInferAcousticState->addTransition(this, &InferPipeline::pieceRemoved, finalState);
    awaitingInferAcousticState->addTransition(this, &InferPipeline::expressivenessChanged,
                                              inferPitchState);
    awaitingInferAcousticState->addTransition(this, &InferPipeline::pitchChanged,
                                              inferVarianceState);

}

void InferPipeline::initAcousticTransitions() {
    inferAcousticState->addTransition(inferAcousticState, &InferAcousticState::finished,
                                      updateAcousticState);
    inferAcousticState->addTransition(inferAcousticState, &InferAcousticState::dropped, finalState);
    inferAcousticState->addTransition(this, &InferPipeline::pieceRemoved, finalState);
    inferAcousticState->addTransition(this, &InferPipeline::expressivenessChanged, inferPitchState);
    inferAcousticState->addTransition(this, &InferPipeline::pitchChanged, inferVarianceState);
    inferAcousticState->addTransition(this, &InferPipeline::varianceChanged, inferAcousticState);

    updateAcousticState->addTransition(updateAcousticState, &UpdateAcousticState::updateSuccess,
                                       playbackReadyState);
    updateAcousticState->addTransition(updateAcousticState, &UpdateAcousticState::pieceNotFound,
                                       finalState);
    updateAcousticState->addTransition(updateAcousticState, &UpdateAcousticState::deferred,
                                       awaitingAcousticApplyState);
    awaitingAcousticApplyState->addTransition(awaitingAcousticApplyState,
                                              &AwaitingEditSessionApplyState::resumeRequested,
                                              updateAcousticState);
    awaitingAcousticApplyState->addTransition(this, &InferPipeline::pieceRemoved, finalState);
    awaitingAcousticApplyState->addTransition(this, &InferPipeline::expressivenessChanged,
                                              inferPitchState);
    awaitingAcousticApplyState->addTransition(this, &InferPipeline::pitchChanged,
                                              inferVarianceState);
    awaitingAcousticApplyState->addTransition(this, &InferPipeline::varianceChanged,
                                              inferAcousticState);
}

void InferPipeline::initPlaybackReadyTransitions() {
    playbackReadyState->addTransition(this, &InferPipeline::pieceRemoved, finalState);
    playbackReadyState->addTransition(this, &InferPipeline::expressivenessChanged, inferPitchState);
    playbackReadyState->addTransition(this, &InferPipeline::pitchChanged, inferVarianceState);

    auto immediateTransition = new ConditionalTransition(this, SIGNAL(varianceChanged()));
    immediateTransition->setTargetState(inferAcousticState);
    immediateTransition->setGuardCondition(
        []() { return appOptions->inference()->autoStartInfer; });
    playbackReadyState->addTransition(immediateTransition);

    auto lazyTransition = new ConditionalTransition(this, SIGNAL(varianceChanged()));
    lazyTransition->setTargetState(awaitingInferAcousticState);
    lazyTransition->setGuardCondition([]() { return !appOptions->inference()->autoStartInfer; });
    playbackReadyState->addTransition(lazyTransition);
}
