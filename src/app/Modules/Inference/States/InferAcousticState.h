//
// Created by FlutyDeer on 2026/1/3.
//

#ifndef DS_EDITOR_LITE_INFERACOUSTICSTATE_H
#define DS_EDITOR_LITE_INFERACOUSTICSTATE_H

#include <QState>

class InferPipeline;
class QFinalState;

class InferAcousticState : public QState {
    Q_OBJECT

public:
    explicit InferAcousticState(InferPipeline &pipeline, QState *parent = nullptr);
    ~InferAcousticState() override = default;

private:
    void onEntry(QEvent *event) override;
    void onExit(QEvent *event) override;

    Q_SIGNAL void taskSuccessWithModelLocked();
    Q_SIGNAL void taskFailed();
    Q_SIGNAL void moveToReadyState();

    void onRunningInferenceStateEntered();

    void onAwaitingModelReleaseStateEntered();

    void onErrorStateEntered();

    InferPipeline &m_pipeline;

    // Child states
    QState *m_runningInferenceState;
    QState *m_awaitingModelReleaseState;
    QState *m_errorState;
    QFinalState *m_finalState;
};

#endif // DS_EDITOR_LITE_INFERACOUSTICSTATE_H