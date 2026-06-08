//
// Created by FlutyDeer on 2026/1/3.
//

#include "InferPitchState.h"

#include "Modules/Inference/InferController.h"
#include "Modules/Inference/InferPipeline.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Model/AppModel/AppModel.h"

#include <QDebug>

namespace Helper = InferControllerHelper;

InferPitchState::InferPitchState(InferPipeline &pipeline, QState *parent)
    : BaseInferState(pipeline, parent) {
}

void InferPitchState::resetState() {
    auto &piece = m_pipeline.piece();
    Helper::resetPitch(piece);
}

void InferPitchState::buildTaskInput() {
    auto &piece = m_pipeline.piece();
    m_taskInput = Helper::buildInferPitchInput(piece, piece.clip->singerIdentifier());
}

IInferTask *InferPitchState::createTask() {
    return new InferPitchTask(m_taskInput);
}

void InferPitchState::addTaskToController(IInferTask *task) {
    inferController->addInferPitchTask(*static_cast<InferPitchTask *>(task));
}

void InferPitchState::cancelTaskInController(int taskId) {
    inferController->cancelInferPitchTask(taskId);
}

bool InferPitchState::finishTaskInController(IInferTask *task) {
    return inferController->finishCurrentInferPitchTask(static_cast<InferPitchTask *>(task));
}

void InferPitchState::setTaskResultToPipeline(IInferTask *task) {
    auto pitchTask = static_cast<InferPitchTask *>(task);
    m_pipeline.setPitchResult(pitchTask->result());
}

QString InferPitchState::getStateNamePrefix() const {
    return "Pitch";
}

bool InferPitchState::validateTaskResult(IInferTask *task, SingingClip *clip) {
    return true;
}
