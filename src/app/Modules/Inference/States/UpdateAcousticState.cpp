//
// Created by FlutyDeer on 2026/1/3.
//

#include "UpdateAcousticState.h"

#include <QTimer>

#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferPipeline.h"

namespace Helper = InferControllerHelper;

UpdateAcousticState::UpdateAcousticState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
}

void UpdateAcousticState::onEntry(QEvent *event) {
    qDebug() << "UpdateAcousticState::onEntry";
    QState::onEntry(event);

    const auto gate = m_pipeline.resolveApplyContext();
    switch (gate.decision) {
        case InferenceApplyGate::Decision::Apply:
            break;
        case InferenceApplyGate::Decision::Defer:
            QTimer::singleShot(0, this, [this] { emit deferred(); });
            return;
        case InferenceApplyGate::Decision::Drop:
            QTimer::singleShot(0, this, [this] { emit pieceNotFound(); });
            return;
    }

    auto &piece = *gate.resolution.piece;
    piece.state = QString("Acoustic.Update");
    Helper::updateAcoustic(m_pipeline.acousticResult(), piece);

    QTimer::singleShot(0, this, [this] { emit updateSuccess(); });
}

void UpdateAcousticState::onExit(QEvent *event) {
    qDebug() << "UpdateAcousticState::onExit";
    QState::onExit(event);
}
