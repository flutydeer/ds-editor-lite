//
// Created by FlutyDeer on 2026/1/3.
//

#include "UpdateVarianceState.h"

#include <QTimer>

#include "Model/AppOptions/AppOptions.h"
#include "Controller/PlaybackController.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferPipeline.h"

namespace Helper = InferControllerHelper;

UpdateVarianceState::UpdateVarianceState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
}

void UpdateVarianceState::onEntry(QEvent *event) {
    qDebug() << "UpdateVarianceState::onEntry";
    QState::onEntry(event);

    auto &piece = m_pipeline.piece();
    piece.state = QString("Variance.Update");
    Helper::updateVariance(m_pipeline.varianceResult(), piece);

    auto isLazy = !appOptions->inference()->autoStartInfer &&
                   playbackController->playbackStatus() != PlaybackStatus::Playing;
    if (isLazy)
        QTimer::singleShot(0, this, [this] { emit updateSuccessWithLazyInference(); });
    else
        QTimer::singleShot(0, this, [this] { emit updateSuccessWithImmediateInference(); });
}

void UpdateVarianceState::onExit(QEvent *event) {
    qDebug() << "UpdateVarianceState::onExit";
    QState::onExit(event);
}