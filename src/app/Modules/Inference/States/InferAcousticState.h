//
// Created by FlutyDeer on 2026/1/3.
//

#ifndef DS_EDITOR_LITE_INFERACOUSTICSTATE_H
#define DS_EDITOR_LITE_INFERACOUSTICSTATE_H

#include "BaseInferState.h"
#include "Modules/Inference/Tasks/InferAcousticTask.h"

class InferAcousticState : public BaseInferState {
    Q_OBJECT

public:
    explicit InferAcousticState(InferPipeline &pipeline, QState *parent = nullptr);
    ~InferAcousticState() override = default;

private:
    void prepareTaskInput() override;
    IInferTask *createTask() override;
    void addTaskToController(IInferTask *task) override;
    void cancelTaskInController(int taskId) override;
    void finishTaskInController() override;
    void setTaskResultToPipeline(IInferTask *task) override;
    QString getStateNamePrefix() const override;
    bool validateTaskResult(IInferTask *task, SingingClip *clip) override;

    InferAcousticTask::InferAcousticInput m_taskInput;
};

#endif // DS_EDITOR_LITE_INFERACOUSTICSTATE_H
