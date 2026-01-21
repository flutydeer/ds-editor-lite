//
// Created by FlutyDeer on 2026/1/3.
//

#include "UpdateAcousticState.h"

#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferPipeline.h"

namespace Helper = InferControllerHelper;

UpdateAcousticState::UpdateAcousticState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
}

void UpdateAcousticState::onEntry(QEvent *event) {
    qDebug() << "UpdateAcousticState::onEntry";
    QState::onEntry(event);

    auto &piece = m_pipeline.piece();
    piece.state = QString("Acoustic.Update");
    Helper::updateAcoustic(m_pipeline.acousticResult(), piece);
    
    emit updateSuccess();
}

void UpdateAcousticState::onExit(QEvent *event) {
    qDebug() << "UpdateAcousticState::onExit";
    QState::onExit(event);
}