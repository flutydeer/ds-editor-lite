//
// Created by FlutyDeer on 2026/1/3.
//

#include "InferAcousticState.h"

#include "Modules/Inference/InferPipeline.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferController.h"
#include "Model/AppModel/AppModel.h"

#include <QDebug>

namespace Helper = InferControllerHelper;

InferAcousticState::InferAcousticState(InferPipeline &pipeline, QState *parent)
    : BaseInferState(pipeline, parent) {
}

void InferAcousticState::prepareTaskInput() {
    auto &piece = m_pipeline.piece();
    m_taskInput = Helper::buildInferAcousticInput(piece, piece.clip->singerIdentifier());
    Helper::resetAcoustic(piece);
}

IInferTask *InferAcousticState::createTask() {
    return new InferAcousticTask(m_taskInput);
}

void InferAcousticState::addTaskToController(IInferTask *task) {
    inferController->addInferAcousticTask(*static_cast<InferAcousticTask *>(task));
}

void InferAcousticState::cancelTaskInController(int taskId) {
    inferController->cancelInferAcousticTask(taskId);
}

void InferAcousticState::finishTaskInController() {
    inferController->finishCurrentInferAcousticTask();
}

void InferAcousticState::setTaskResultToPipeline(IInferTask *task) {
    auto acousticTask = static_cast<InferAcousticTask *>(task);
    m_pipeline.setAcousticResult(acousticTask->result());
}

QString InferAcousticState::getStateNamePrefix() const {
    return "Acoustic";
}

bool InferAcousticState::validateTaskResult(IInferTask *task, SingingClip *clip) {
    return true;
}
