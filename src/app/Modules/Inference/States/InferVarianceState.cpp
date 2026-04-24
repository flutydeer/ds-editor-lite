//
// Created by FlutyDeer on 2026/1/3.
//

#include "InferVarianceState.h"

#include "Modules/Inference/InferController.h"
#include "Modules/Inference/InferPipeline.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Model/AppModel/AppModel.h"

#include <QDebug>

namespace Helper = InferControllerHelper;

InferVarianceState::InferVarianceState(InferPipeline &pipeline, QState *parent)
    : BaseInferState(pipeline, parent) {
}

void InferVarianceState::prepareTaskInput() {
    auto &piece = m_pipeline.piece();
    m_taskInput = Helper::buildInferVarianceInput(piece, piece.clip->singerIdentifier());
    Helper::resetVariance(piece);
}

IInferTask *InferVarianceState::createTask() {
    return new InferVarianceTask(m_taskInput);
}

void InferVarianceState::addTaskToController(IInferTask *task) {
    inferController->addInferVarianceTask(*static_cast<InferVarianceTask *>(task));
}

void InferVarianceState::cancelTaskInController(int taskId) {
    inferController->cancelInferVarianceTask(taskId);
}

void InferVarianceState::finishTaskInController() {
    inferController->finishCurrentInferVarianceTask();
}

void InferVarianceState::setTaskResultToPipeline(IInferTask *task) {
    auto varianceTask = static_cast<InferVarianceTask *>(task);
    m_pipeline.setVarianceResult(varianceTask->result());
}

QString InferVarianceState::getStateNamePrefix() const {
    return "Variance";
}

bool InferVarianceState::validateTaskResult(IInferTask *task, SingingClip *clip) {
    return true;
}
