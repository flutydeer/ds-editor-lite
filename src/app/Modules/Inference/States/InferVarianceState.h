//
// Created by FlutyDeer on 2026/1/3.
//

#ifndef DS_EDITOR_LITE_INFERVARIANCESTATE_H
#define DS_EDITOR_LITE_INFERVARIANCESTATE_H

#include <QState>

class InferPipeline;
class QFinalState;
class InferVarianceTask;

class InferVarianceState : public QState {
    Q_OBJECT

public:
    explicit InferVarianceState(InferPipeline &pipeline, QState *parent = nullptr);
    ~InferVarianceState() override = default;

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
    void handleTaskFinished(InferVarianceTask &task);

    InferPipeline &m_pipeline;
    InferVarianceTask *currentTask = nullptr;

    // Child states
    QState *m_runningInferenceState;
    QState *m_awaitingModelReleaseState;
    QState *m_errorState;
    QFinalState *m_finalState;
};

#endif // DS_EDITOR_LITE_INFERVARIANCESTATE_H