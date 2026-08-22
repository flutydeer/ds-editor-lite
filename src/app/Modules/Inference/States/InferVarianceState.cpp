#include "InferVarianceState.h"

#include "Modules/Inference/InferController.h"
#include "Modules/Inference/InferPipeline.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferenceAutomationBridge.h"
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QDebug>

namespace Helper = InferControllerHelper;

InferVarianceState::InferVarianceState(InferPipeline &pipeline, QState *parent)
    : BaseInferState(pipeline, parent) {
}

bool InferVarianceState::resetState() {
    auto &piece = m_pipeline.piece();
    Automation::InferenceMutationRequest request;
    request.kind = Automation::InferenceMutationKind::ResetStage;
    request.clipId = Automation::ClipId(piece.clipId());
    request.pieceId = Automation::PieceId(piece.id());
    request.stage = Automation::InferenceStage::Variance;
    const auto result = InferenceAutomationBridge::executeCurrent(request);
    if (!result)
        qWarning() << "Failed to reset variance inference state:" << result.getError().message;
    return static_cast<bool>(result);
}

void InferVarianceState::buildTaskInput() {
    auto &piece = m_pipeline.piece();
    m_taskInput = Helper::buildInferVarianceInput(piece, piece.clip->singerIdentifier());
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

bool InferVarianceState::finishTaskInController(IInferTask *task) {
    return inferController->finishCurrentInferVarianceTask(
        static_cast<InferVarianceTask *>(task));
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
