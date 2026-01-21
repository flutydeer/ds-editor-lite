//
// Created by FlutyDeer on 2026/1/3.
//

#ifndef DS_EDITOR_LITE_INFERACOUSTICSTATE_H
#define DS_EDITOR_LITE_INFERACOUSTICSTATE_H

#include <QState>

class InferPipeline;
class QFinalState;
class InferAcousticTask;

class InferAcousticState : public QState {
    Q_OBJECT

public:
    explicit InferAcousticState(InferPipeline &pipeline, QState *parent = nullptr);
    ~InferAcousticState() override = default;

private:
    void onEntry(QEvent *event) override;
    void onExit(QEvent *event) override;

    Q_SIGNAL void taskSuccessWithModelLocked();
    Q_SIGNAL void failed();
    Q_SIGNAL void ready();

    void onRunningInferenceStateEntered();
    void onAwaitingModelReleaseStateEntered();
    void onErrorStateEntered();
    void handleTaskFinished(InferAcousticTask &task);

    InferPipeline &m_pipeline;
    int taskId = -1;

    // Child states
    QState *m_runningInferenceState;
    QState *m_awaitingModelReleaseState;
    QState *m_errorState;
    QFinalState *m_finalState;
};

#endif // DS_EDITOR_LITE_INFERACOUSTICSTATE_H