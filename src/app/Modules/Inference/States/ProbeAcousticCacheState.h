//
// Created by OpenVPI on 2026/7/22.
//

#ifndef DS_EDITOR_LITE_PROBEACOUSTICCACHESTATE_H
#define DS_EDITOR_LITE_PROBEACOUSTICCACHESTATE_H

#include "Modules/Inference/Tasks/InferAcousticCacheProbeTask.h"

#include <QState>

class InferPipeline;

class ProbeAcousticCacheState final : public QState {
    Q_OBJECT

public:
    explicit ProbeAcousticCacheState(InferPipeline &pipeline, QState *parent = nullptr);
    ~ProbeAcousticCacheState() override = default;

signals:
    void cacheHit();
    void cacheMissWithLazyInference();
    void cacheMissWithImmediateInference();
    void dropped();

private:
    void onEntry(QEvent *event) override;
    void onExit(QEvent *event) override;
    void handleTaskFinished(InferAcousticCacheProbeTask &task);
    void cancelCurrentTask();

    InferPipeline &m_pipeline;
    InferAcousticCacheProbeTask *m_currentTask = nullptr;
};

#endif // DS_EDITOR_LITE_PROBEACOUSTICCACHESTATE_H
