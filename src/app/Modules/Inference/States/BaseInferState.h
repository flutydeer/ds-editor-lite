//
// Created by FlutyDeer on 2026/4/25.
//

#ifndef DS_EDITOR_LITE_BASEINFERSTATE_H
#define DS_EDITOR_LITE_BASEINFERSTATE_H

#include <QState>

class InferPipeline;
class QFinalState;
class IInferTask;
class SingingClip;

class BaseInferState : public QState {
    Q_OBJECT

public:
    explicit BaseInferState(InferPipeline &pipeline, QState *parent = nullptr);
    virtual ~BaseInferState() = default;

protected:
    void onEntry(QEvent *event) override;
    void onExit(QEvent *event) override;

    Q_SIGNAL void taskSuccessWithModelLocked();
    Q_SIGNAL void failed();
    Q_SIGNAL void ready();

    void onRunningInferenceStateEntered();
    void onRunningInferenceStateExited();
    void onAwaitingModelReleaseStateEntered();
    void onErrorStateEntered();
    void handleTaskFinished(IInferTask &task);

    virtual void prepareTaskInput() = 0;
    virtual IInferTask *createTask() = 0;
    virtual void addTaskToController(IInferTask *task) = 0;
    virtual void cancelTaskInController(int taskId) = 0;
    virtual void finishTaskInController() = 0;
    virtual void setTaskResultToPipeline(IInferTask *task) = 0;
    virtual QString getStateNamePrefix() const = 0;
    virtual bool validateTaskResult(IInferTask *task, SingingClip *clip) = 0;

    InferPipeline &m_pipeline;
    IInferTask *currentTask = nullptr;

    QState *m_runningInferenceState;
    QState *m_awaitingModelReleaseState;
    QState *m_errorState;
    QFinalState *m_finalState;
};

#endif // DS_EDITOR_LITE_BASEINFERSTATE_H
