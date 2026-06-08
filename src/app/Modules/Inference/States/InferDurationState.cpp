//
// Created by FlutyDeer on 2026/1/3.
//

#include "InferDurationState.h"

#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferPipeline.h"
#include "Modules/Inference/InferController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/SingingClip.h"

#include <QDebug>

namespace Helper = InferControllerHelper;

InferDurationState::InferDurationState(InferPipeline &pipeline, QState *parent)
    : BaseInferState(pipeline, parent) {
}

void InferDurationState::resetState() {
    auto &piece = m_pipeline.piece();
    Helper::resetPhoneOffset(piece.notes, piece);
}

void InferDurationState::buildTaskInput() {
    auto &piece = m_pipeline.piece();
    m_taskInput = Helper::buildInferDurInput(piece, piece.clip->singerIdentifier());
}

IInferTask *InferDurationState::createTask() {
    return new InferDurationTask(m_taskInput);
}

void InferDurationState::addTaskToController(IInferTask *task) {
    inferController->addInferDurationTask(*static_cast<InferDurationTask *>(task));
}

void InferDurationState::cancelTaskInController(int taskId) {
    inferController->cancelInferDurationTask(taskId);
}

bool InferDurationState::finishTaskInController(IInferTask *task) {
    return inferController->finishCurrentInferDurationTask(
        static_cast<InferDurationTask *>(task));
}

void InferDurationState::setTaskResultToPipeline(IInferTask *task) {
    auto durationTask = static_cast<InferDurationTask *>(task);
    m_pipeline.setDurationResult(durationTask->result());
}

QString InferDurationState::getStateNamePrefix() const {
    return "Duration";
}

bool InferDurationState::validateTaskResult(IInferTask *task, SingingClip *clip) {
    auto durationTask = static_cast<InferDurationTask *>(task);
    const auto piece = clip->findPieceById(durationTask->pieceId());
    if (!piece)
        return false;

    const auto modelNoteCount = piece->notes.count();
    const auto taskNoteCount = durationTask->result().count();
    if (modelNoteCount != taskNoteCount) {
        qWarning() << "Drop duration result because model note count does not equal task note count"
                   << "clipId:" << durationTask->clipId() << "pieceId:" << durationTask->pieceId()
                   << "modelNoteCount:" << modelNoteCount << "taskNoteCount:" << taskNoteCount;
        return false;
    }
    return true;
}
