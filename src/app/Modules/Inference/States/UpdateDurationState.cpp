//
// Created by FlutyDeer on 2026/1/3.
//

#include "UpdateDurationState.h"

#include "Model/AppModel/AppModel.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferPipeline.h"

namespace Helper = InferControllerHelper;

UpdateDurationState::UpdateDurationState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
}

void UpdateDurationState::onEntry(QEvent *event) {
    qDebug() << "UpdateDurationState::onEntry";
    const auto singingClip =
        dynamic_cast<SingingClip *>(appModel->findClipById(m_pipeline.piece().clipId()));
    const auto &piece = m_pipeline.piece();
    InferControllerHelper::updatePhoneOffset(piece.notes, m_pipeline.durationResult(),
                                             *singingClip);

    // TODO: 可能需要将更新相对参数的方法提取出来
    Helper::getParamDirtyPiecesAndUpdateInput(ParamInfo::Expressiveness, *singingClip);
    Helper::getParamDirtyPiecesAndUpdateInput(ParamInfo::Gender, *singingClip);
    Helper::getParamDirtyPiecesAndUpdateInput(ParamInfo::Velocity, *singingClip);
    QState::onEntry(event);
    emit updateSuccess();
}

void UpdateDurationState::onExit(QEvent *event) {
    qDebug() << "UpdateDurationState::onExit";
    QState::onExit(event);
}
