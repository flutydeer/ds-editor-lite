//
// Created by OrangeCat on 24-9-4.
//

#ifndef INFERDURATIONTASK_H
#define INFERDURATIONTASK_H

#include <atomic>

#include <synthrt/SVS/Inference.h>

#include "IInferTask.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Models/InferInputBase.h"
#include "Modules/Inference/Models/InferInputNote.h"
#include "Modules/Inference/Models/SingerIdentifier.h"
#include "Modules/Task/Task.h"

class InferDurationTask final : public IInferTask {
    Q_OBJECT

public:
    class InferDurInput {
    public:
        INFER_INPUT_COMMON_MEMBERS
        bool operator==(const InferDurInput &other) const;
    };

    int clipId() const override;
    int pieceId() const override;
    bool success() const override;

    explicit InferDurationTask(InferDurInput input);
    InferDurInput input() const;
    QList<InferInputNote> result() const;

private:
    void runTask() override;
    bool runInference(const GenericInferModel &model, std::vector<double> &outDuration,
                      QString &error);
    void terminate() override;
    void abort();
    void buildPreviewText();
    GenericInferModel buildInputJson() const;
    bool processOutput(const GenericInferModel &model);

    mutable QReadWriteLock m_rwLock;
    srt::NO<srt::Inference> m_inferenceDuration;
    QString m_previewText;
    InferDurInput m_input;
    InferDurInput m_result;
    QString m_inputHash;
    std::atomic<bool> m_success{false};
};



#endif // INFERDURATIONTASK_H
