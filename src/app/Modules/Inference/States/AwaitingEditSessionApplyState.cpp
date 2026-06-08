//
// Created by FlutyDeer on 2026/6/8.
//

#include "AwaitingEditSessionApplyState.h"

#include "Modules/Inference/EditSessionManager.h"
#include "Modules/Inference/InferPipeline.h"
#include "Modules/Inference/Utils/InferenceApplyGate.h"

#include <QDebug>
#include <QTimer>

AwaitingEditSessionApplyState::AwaitingEditSessionApplyState(InferPipeline &pipeline,
                                                             const Stage stage, QState *parent)
    : QState(parent), m_pipeline(pipeline), m_stage(stage) {
}

void AwaitingEditSessionApplyState::onEntry(QEvent *event) {
    qDebug() << "AwaitingEditSessionApplyState::onEntry" << "stage:" << stageName();
    QState::onEntry(event);
    m_resumeRequested = false;

    auto &piece = m_pipeline.piece();
    piece.state = QString("%1.AwaitingEditSessionApply").arg(stageName());

    m_editSessionEndedConnection =
        connect(editSessionManager, &EditSessionManager::editSessionEnded, this,
                [this] { QTimer::singleShot(0, this, [this] { requestResume(); }); });
    if (!editSessionManager->hasActiveTransaction())
        QTimer::singleShot(0, this, [this] { requestResume(); });
}

void AwaitingEditSessionApplyState::onExit(QEvent *event) {
    qDebug() << "AwaitingEditSessionApplyState::onExit" << "stage:" << stageName();
    if (!m_resumeRequested) {
        InferenceApplyGate::logDecision(m_pipeline.applyContext(), "pipeline-awaiting",
                                        InferenceApplyGate::Decision::Drop,
                                        "pipeline-awaiting-invalidated");
    }
    if (m_editSessionEndedConnection)
        disconnect(m_editSessionEndedConnection);
    m_editSessionEndedConnection = {};
    QState::onExit(event);
}

QString AwaitingEditSessionApplyState::stageName() const {
    switch (m_stage) {
        case Stage::Duration:
            return "Duration";
        case Stage::Pitch:
            return "Pitch";
        case Stage::Variance:
            return "Variance";
        case Stage::Acoustic:
            return "Acoustic";
    }
    return "Unknown";
}

void AwaitingEditSessionApplyState::requestResume() {
    m_resumeRequested = true;
    emit resumeRequested();
}
