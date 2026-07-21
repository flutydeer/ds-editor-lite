//
// Created by OpenVPI on 2026/7/22.
//

#ifndef DS_EDITOR_LITE_INFERACOUSTICCACHEPROBETASK_H
#define DS_EDITOR_LITE_INFERACOUSTICCACHEPROBETASK_H

#include <atomic>

#include "IInferTask.h"
#include "InferAcousticTask.h"

class InferAcousticCacheProbeTask final : public IInferTask {
    Q_OBJECT

public:
    [[nodiscard]] int clipId() const override;
    [[nodiscard]] int pieceId() const override;
    [[nodiscard]] InferenceTaskContext inferenceContext() const override;
    [[nodiscard]] bool success() const override;

    explicit InferAcousticCacheProbeTask(InferAcousticTask::InferAcousticInput input);
    [[nodiscard]] bool cacheHit() const;
    [[nodiscard]] QString result() const;

private:
    void runTask() override;

    InferAcousticTask::InferAcousticInput m_input;
    QString m_result;
    std::atomic<bool> m_cacheHit{false};
    std::atomic<bool> m_success{false};
};

#endif // DS_EDITOR_LITE_INFERACOUSTICCACHEPROBETASK_H
