//
// Created by FlutyDeer on 2026/1/22.
//

#ifndef DS_EDITOR_LITE_PLAYBACKREADYSTATE_H
#define DS_EDITOR_LITE_PLAYBACKREADYSTATE_H

#include <QState>

class InferPipeline;

class PlaybackReadyState : public QState {
    Q_OBJECT

public:
    explicit PlaybackReadyState(InferPipeline &pipeline, QState *parent = nullptr);
    ~PlaybackReadyState() override = default;

private:
    void onEntry(QEvent *event) override;
    void onExit(QEvent *event) override;

    InferPipeline &m_pipeline;
};

#endif // DS_EDITOR_LITE_PLAYBACKREADYSTATE_H
