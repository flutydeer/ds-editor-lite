//
// Created by FlutyDeer on 2026/1/3.
//

#include "UpdateVarianceState.h"

UpdateVarianceState::UpdateVarianceState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
}

void UpdateVarianceState::onEntry(QEvent *event) {
    qDebug() << "UpdateVarianceState::onEntry";
    QState::onEntry(event);
    emit updateSuccess();
}

void UpdateVarianceState::onExit(QEvent *event) {
    qDebug() << "UpdateVarianceState::onExit";
    QState::onExit(event);
}