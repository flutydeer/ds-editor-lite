#include "UpdateDurationState.h"

#include <QTimer>

#include "Modules/Inference/InferenceAutomationBridge.h"
#include "Modules/Inference/InferPipeline.h"

UpdateDurationState::UpdateDurationState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
}

void UpdateDurationState::onEntry(QEvent *event) {
    qDebug() << "UpdateDurationState::onEntry";
    QState::onEntry(event);

    const auto gate = m_pipeline.resolveApplyContext(m_pipeline.durationResult().count());
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

    Automation::InferenceMutationRequest request;
    request.kind = Automation::InferenceMutationKind::ApplyDuration;
    request.clipId = Automation::ClipId(m_pipeline.applyContext().clipId);
    request.pieceId = Automation::PieceId(m_pipeline.applyContext().pieceId);
    for (const auto id : m_pipeline.applyContext().noteIds)
        request.noteIds.append(Automation::NoteId(id));
    request.durationResult = m_pipeline.durationResult();
    const auto result = InferenceAutomationBridge::executeAfterGate(
        m_pipeline.applyContext().documentVersion, request);
    if (!result) {
        m_pipeline.notifyDropped(InferenceAutomationBridge::dropReason(result.getError()));
        QTimer::singleShot(0, this, [this] { emit pieceNotFound(); });
        return;
    }
    QTimer::singleShot(0, this, [this] { emit updateSuccess(); });
}

void UpdateDurationState::onExit(QEvent *event) {
    qDebug() << "UpdateDurationState::onExit";
    QState::onExit(event);
}
