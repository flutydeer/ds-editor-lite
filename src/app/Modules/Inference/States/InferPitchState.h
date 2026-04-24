//
// Created by FlutyDeer on 2026/1/3.
//

#ifndef DS_EDITOR_LITE_INFERPITCHSTATE_H
#define DS_EDITOR_LITE_INFERPITCHSTATE_H

#include "BaseInferState.h"
#include "Modules/Inference/Tasks/InferPitchTask.h"

class InferPitchState : public BaseInferState {
    Q_OBJECT

public:
    explicit InferPitchState(InferPipeline &pipeline, QState *parent = nullptr);
    ~InferPitchState() override = default;

private:
    void prepareTaskInput() override;
    IInferTask *createTask() override;
    void addTaskToController(IInferTask *task) override;
    void cancelTaskInController(int taskId) override;
    void finishTaskInController() override;
    void setTaskResultToPipeline(IInferTask *task) override;
    QString getStateNamePrefix() const override;
    bool validateTaskResult(IInferTask *task, SingingClip *clip) override;

    InferPitchTask::InferPitchInput m_taskInput;
};

#endif // DS_EDITOR_LITE_INFERPITCHSTATE_H
