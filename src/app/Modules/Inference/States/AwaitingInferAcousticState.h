//
// Created by FlutyDeer on 2026/1/21.
//

#ifndef DS_EDITOR_LITE_AWAITINGINFERACOUSTICSTATE_H
#define DS_EDITOR_LITE_AWAITINGINFERACOUSTICSTATE_H

#include <QState>

class InferPipeline;

class AwaitingInferAcousticState : public QState {
    Q_OBJECT

public:
    AwaitingInferAcousticState(InferPipeline &pipeline, QState *parent = nullptr);

private:
    void onEntry(QEvent *event) override;
    void onExit(QEvent *event) override;

    InferPipeline &m_pipeline;
};

#endif // DS_EDITOR_LITE_AWAITINGINFERACOUSTICSTATE_H
