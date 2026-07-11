//
// Created by FlutyDeer on 2026/1/3.
//

#include "UpdateDurationState.h"

#include <QTimer>

#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferPipeline.h"

namespace Helper = InferControllerHelper;

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

    InferControllerHelper::updatePhoneOffset(gate.resolution.notes, m_pipeline.durationResult(),
                                             *gate.resolution.clip);

    // TODO: 可能需要将更新相对参数的方法提取出来
    Helper::getParamDirtyPiecesAndUpdateInput(ParamInfo::Expressiveness, *gate.resolution.clip);
    Helper::getParamDirtyPiecesAndUpdateInput(ParamInfo::Gender, *gate.resolution.clip);
    Helper::getParamDirtyPiecesAndUpdateInput(ParamInfo::Velocity, *gate.resolution.clip);
    QTimer::singleShot(0, this, [this] { emit updateSuccess(); });
}

void UpdateDurationState::onExit(QEvent *event) {
    qDebug() << "UpdateDurationState::onExit";
    QState::onExit(event);
}
