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

    InferenceTaskResolution resolution;
    if (!m_pipeline.resolveApplyContext(resolution, m_pipeline.durationResult().count())) {
        QTimer::singleShot(0, this, [this] { emit pieceNotFound(); });
        return;
    }

    InferControllerHelper::updatePhoneOffset(resolution.notes, m_pipeline.durationResult(),
                                             *resolution.clip);

    // TODO: 可能需要将更新相对参数的方法提取出来
    Helper::getParamDirtyPiecesAndUpdateInput(ParamInfo::Expressiveness, *resolution.clip);
    Helper::getParamDirtyPiecesAndUpdateInput(ParamInfo::Gender, *resolution.clip);
    Helper::getParamDirtyPiecesAndUpdateInput(ParamInfo::Velocity, *resolution.clip);
    QTimer::singleShot(0, this, [this] { emit updateSuccess(); });
}

void UpdateDurationState::onExit(QEvent *event) {
    qDebug() << "UpdateDurationState::onExit";
    QState::onExit(event);
}
