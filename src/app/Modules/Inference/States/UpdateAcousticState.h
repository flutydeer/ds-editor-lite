//
// Created by FlutyDeer on 2026/1/3.
//

#ifndef DS_EDITOR_LITE_UPDATEACOUSTICSTATE_H
#define DS_EDITOR_LITE_UPDATEACOUSTICSTATE_H

#include <QState>

class InferPipeline;

class UpdateAcousticState : public QState {
    Q_OBJECT

public:
    explicit UpdateAcousticState(InferPipeline &pipeline, QState *parent = nullptr);

signals:
    void updateSuccess();
    void pieceNotFound();

private:
    void onEntry(QEvent *event) override;
    void onExit(QEvent *event) override;

    InferPipeline &m_pipeline;
};

#endif // DS_EDITOR_LITE_UPDATEACOUSTICSTATE_H