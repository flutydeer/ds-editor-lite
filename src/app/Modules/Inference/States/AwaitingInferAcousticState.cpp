//
// Created by FlutyDeer on 2026/1/21.
//

#include "AwaitingInferAcousticState.h"

#include "Modules/Inference/InferPipeline.h"

AwaitingInferAcousticState::AwaitingInferAcousticState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
}

void AwaitingInferAcousticState::onEntry(QEvent *event) {
    qDebug() << "AwaitingInferAcousticState::onEntry";
    QState::onEntry(event);

    auto &piece = m_pipeline.piece();
    piece.acousticInferStatus = Pending;
    piece.state = QString("Acoustic.Awaiting");
}

void AwaitingInferAcousticState::onExit(QEvent *event) {
    qDebug() << "AwaitingInferAcousticState::onExit";
    QState::onExit(event);
}