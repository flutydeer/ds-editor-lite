#include "UpdatePitchState.h"

#include <QTimer>

#include "Modules/Inference/InferenceAutomationBridge.h"
#include "Modules/Inference/InferPipeline.h"

UpdatePitchState::UpdatePitchState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
}

void UpdatePitchState::onEntry(QEvent *event) {
    qDebug() << "UpdatePitchState::onEntry";
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

    gate.resolution.piece->state = QString("Pitch.Update");
    Automation::InferenceMutationRequest request;
    request.kind = Automation::InferenceMutationKind::ApplyPitch;
    request.clipId = Automation::ClipId(m_pipeline.applyContext().clipId);
    request.pieceId = Automation::PieceId(m_pipeline.applyContext().pieceId);
    request.pitchResult = m_pipeline.pitchResult();
    request.pitchSmoothKernelSize = m_pipeline.applyContext().pitchSmoothKernelSize;
    const auto result = InferenceAutomationBridge::executeAfterGate(
        m_pipeline.applyContext().documentVersion, request);
    if (!result) {
        m_pipeline.notifyDropped(InferenceAutomationBridge::dropReason(result.getError()));
        QTimer::singleShot(0, this, [this] { emit pieceNotFound(); });
        return;
    }
    QTimer::singleShot(0, this, [this] { emit updateSuccess(); });
}

void UpdatePitchState::onExit(QEvent *event) {
    qDebug() << "UpdatePitchState::onExit";
    QState::onExit(event);
}
