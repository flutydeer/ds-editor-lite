//
// Created by FlutyDeer on 2026/1/3.
//

#ifndef DS_EDITOR_LITE_INFERPITCHSTATE_H
#define DS_EDITOR_LITE_INFERPITCHSTATE_H

#include <QState>

class InferPipeline;
class QFinalState;
class InferPitchTask;

class InferPitchState : public QState {
    Q_OBJECT

public:
    explicit InferPitchState(InferPipeline &pipeline, QState *parent = nullptr);
    ~InferPitchState() override = default;

private:
    void onEntry(QEvent *event) override;
    void onExit(QEvent *event) override;

    Q_SIGNAL void taskSuccessWithModelLocked();
    Q_SIGNAL void failed();
    Q_SIGNAL void ready();

    void onRunningInferenceStateEntered();
    void onRunningInferenceStateExited();
    void onAwaitingModelReleaseStateEntered();
    void onErrorStateEntered();
    void handleTaskFinished(InferPitchTask &task);

    InferPipeline &m_pipeline;
    InferPitchTask *currentTask = nullptr;

    // Child states
    QState *m_runningInferenceState;
    QState *m_awaitingModelReleaseState;
    QState *m_errorState;
    QFinalState *m_finalState;
};

#endif // DS_EDITOR_LITE_INFERPITCHSTATE_H