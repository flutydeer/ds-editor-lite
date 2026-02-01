//
// Created by FlutyDeer on 2026/1/22.
//

#include "PlaybackReadyState.h"

#include "Modules/Inference/InferPipeline.h"

PlaybackReadyState::PlaybackReadyState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
}

void PlaybackReadyState::onEntry(QEvent *event) {
    qDebug() << "PlaybackReadyState::onEntry";
    QState::onEntry(event);

    auto &piece = m_pipeline.piece();
    piece.acousticInferStatus = Success;
    piece.state = QString("Ready");
}

void PlaybackReadyState::onExit(QEvent *event) {
    qDebug() << "PlaybackReadyState::onExit";
    QState::onExit(event);
}