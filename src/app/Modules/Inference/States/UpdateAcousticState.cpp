//
// Created by FlutyDeer on 2026/1/3.
//

#include "UpdateAcousticState.h"

UpdateAcousticState::UpdateAcousticState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
}

void UpdateAcousticState::onEntry(QEvent *event) {
    qDebug() << "UpdateAcousticState::onEntry";
    QState::onEntry(event);
    emit updateSuccess();
}

void UpdateAcousticState::onExit(QEvent *event) {
    qDebug() << "UpdateAcousticState::onExit";
    QState::onExit(event);
}