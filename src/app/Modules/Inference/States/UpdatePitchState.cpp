//
// Created by FlutyDeer on 2026/1/3.
//

#include "UpdatePitchState.h"

#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferPipeline.h"

namespace Helper = InferControllerHelper;

UpdatePitchState::UpdatePitchState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
}

void UpdatePitchState::onEntry(QEvent *event) {
    qDebug() << "UpdatePitchState::onEntry";
    QState::onEntry(event);
    
    auto &piece = m_pipeline.piece();
    piece.state = QString("Pitch.Update");
    Helper::updatePitch(m_pipeline.pitchResult(), piece);
    emit updateSuccess();
}

void UpdatePitchState::onExit(QEvent *event) {
    qDebug() << "UpdatePitchState::onExit";
    QState::onExit(event);
}