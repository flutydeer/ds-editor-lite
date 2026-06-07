//
// Created by FlutyDeer on 2026/1/3.
//

#include "UpdatePitchState.h"

#include <QTimer>

#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferPipeline.h"

namespace Helper = InferControllerHelper;

UpdatePitchState::UpdatePitchState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
}

void UpdatePitchState::onEntry(QEvent *event) {
    qDebug() << "UpdatePitchState::onEntry";
    QState::onEntry(event);

    InferenceTaskResolution resolution;
    if (!m_pipeline.resolveApplyContext(resolution)) {
        QTimer::singleShot(0, this, [this] { emit pieceNotFound(); });
        return;
    }

    auto &piece = *resolution.piece;
    piece.state = QString("Pitch.Update");
    Helper::updatePitch(m_pipeline.pitchResult(), piece);
    QTimer::singleShot(0, this, [this] { emit updateSuccess(); });
}

void UpdatePitchState::onExit(QEvent *event) {
    qDebug() << "UpdatePitchState::onExit";
    QState::onExit(event);
}
