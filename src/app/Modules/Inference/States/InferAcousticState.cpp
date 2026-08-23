#include "InferAcousticState.h"

#include "Modules/Inference/InferPipeline.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferenceAutomationBridge.h"
#include "Modules/Inference/InferController.h"
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QDebug>

namespace Helper = InferControllerHelper;

InferAcousticState::InferAcousticState(InferPipeline &pipeline, QState *parent)
    : BaseInferState(pipeline, parent) {
}

bool InferAcousticState::resetState() {
    auto &piece = m_pipeline.piece();
    Automation::InferenceMutationRequest request;
    request.kind = Automation::InferenceMutationKind::ResetStage;
    request.clipId = Automation::ClipId(piece.clipId());
    request.pieceId = Automation::PieceId(piece.id());
    request.stage = Automation::InferenceStage::Acoustic;
    const auto result = InferenceAutomationBridge::executeCurrent(request);
    if (!result)
        qWarning() << "Failed to reset acoustic inference state:" << result.getError().message;
    return static_cast<bool>(result);
}

void InferAcousticState::buildTaskInput() {
    auto &piece = m_pipeline.piece();
    m_taskInput = Helper::buildInferAcousticInput(piece, piece.clip->singerIdentifier());
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

bool InferAcousticState::finishTaskInController(IInferTask *task) {
    return inferController->finishCurrentInferAcousticTask(
        static_cast<InferAcousticTask *>(task));
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
