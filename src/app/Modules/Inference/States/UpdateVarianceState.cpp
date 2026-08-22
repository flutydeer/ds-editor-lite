#include "UpdateVarianceState.h"

#include <QTimer>

#include "Model/AppOptions/AppOptions.h"
#include "Controller/PlaybackController.h"
#include "Modules/Inference/InferenceAutomationBridge.h"
#include "Modules/Inference/InferPipeline.h"

UpdateVarianceState::UpdateVarianceState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
}

void UpdateVarianceState::onEntry(QEvent *event) {
    qDebug() << "UpdateVarianceState::onEntry";
    QState::onEntry(event);

    const auto gate = m_pipeline.resolveApplyContext();
    switch (gate.decision) {
        case InferenceApplyGate::Decision::Apply:
            break;
        case InferenceApplyGate::Decision::Defer:
            QTimer::singleShot(0, this, [this] { emit deferred(); });
            return;
        case InferenceApplyGate::Decision::Drop:
            m_pipeline.notifyDropped(gate.reason);
            QTimer::singleShot(0, this, [this] { emit pieceNotFound(); });
            return;
    }

    gate.resolution.piece->state = QString("Variance.Update");
    Automation::InferenceMutationRequest request;
    request.kind = Automation::InferenceMutationKind::ApplyVariance;
    request.clipId = Automation::ClipId(m_pipeline.applyContext().clipId);
    request.pieceId = Automation::PieceId(m_pipeline.applyContext().pieceId);
    const auto &variance = m_pipeline.varianceResult();
    request.varianceResult.breathiness = variance.breathiness;
    request.varianceResult.tension = variance.tension;
    request.varianceResult.voicing = variance.voicing;
    request.varianceResult.energy = variance.energy;
    request.varianceResult.mouthOpening = variance.mouthOpening;
    const auto result = InferenceAutomationBridge::execute(
        m_pipeline.applyContext().documentVersion, request);
    if (!result) {
        m_pipeline.notifyDropped(InferenceAutomationBridge::dropReason(result.getError()));
        QTimer::singleShot(0, this, [this] { emit pieceNotFound(); });
        return;
    }

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
