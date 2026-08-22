#include "UpdateAcousticState.h"

#include <QTimer>

#include "Modules/Inference/InferenceAutomationBridge.h"
#include "Modules/Inference/InferPipeline.h"

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
            m_pipeline.notifyDropped(gate.reason);
            QTimer::singleShot(0, this, [this] { emit pieceNotFound(); });
            return;
    }

    gate.resolution.piece->state = QString("Acoustic.Update");
    Automation::InferenceMutationRequest request;
    request.kind = Automation::InferenceMutationKind::ApplyAcoustic;
    request.clipId = Automation::ClipId(m_pipeline.applyContext().clipId);
    request.pieceId = Automation::PieceId(m_pipeline.applyContext().pieceId);
    request.acousticPath = m_pipeline.acousticResult();
    const auto result = InferenceAutomationBridge::execute(
        m_pipeline.applyContext().documentVersion, request);
    if (!result) {
        m_pipeline.notifyDropped(InferenceAutomationBridge::dropReason(result.getError()));
        QTimer::singleShot(0, this, [this] { emit pieceNotFound(); });
        return;
    }

    QTimer::singleShot(0, this, [this] { emit updateSuccess(); });
}

void UpdateAcousticState::onExit(QEvent *event) {
    qDebug() << "UpdateAcousticState::onExit";
    QState::onExit(event);
}
