//
// Created by FlutyDeer on 2026/1/3.
//

#ifndef DS_EDITOR_LITE_INFERVARIANCESTATE_H
#define DS_EDITOR_LITE_INFERVARIANCESTATE_H

#include "BaseInferState.h"
#include "Modules/Inference/Tasks/InferVarianceTask.h"

class InferVarianceState : public BaseInferState {
    Q_OBJECT

public:
    explicit InferVarianceState(InferPipeline &pipeline, QState *parent = nullptr);
    ~InferVarianceState() override = default;

private:
    void prepareTaskInput() override;
    IInferTask *createTask() override;
    void addTaskToController(IInferTask *task) override;
    void cancelTaskInController(int taskId) override;
    void finishTaskInController() override;
    void setTaskResultToPipeline(IInferTask *task) override;
    QString getStateNamePrefix() const override;
    bool validateTaskResult(IInferTask *task, SingingClip *clip) override;

    InferVarianceTask::InferVarianceInput m_taskInput;
};

#endif // DS_EDITOR_LITE_INFERVARIANCESTATE_H
