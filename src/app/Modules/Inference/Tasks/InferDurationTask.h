//
// Created by OrangeCat on 24-9-4.
//

#ifndef INFERDURATIONTASK_H
#define INFERDURATIONTASK_H

#include "IInferTask.h"
#include "Modules/Inference/Models/InferInputBase.h"
#include "Modules/Inference/Models/InferInputNote.h"
#include "Modules/Task/Task.h"

class InferDurationTask final : public IInferTask {
public:
    class InferDurInput {
    public:
        INFER_INPUT_COMMON_MEMBERS
        bool operator==(const InferDurInput &other) const;
    };

    int clipId() const override;
    int pieceId() const override;
    [[nodiscard]] bool success() const override;

    explicit InferDurationTask(InferDurInput input);
    InferDurInput input();
    QList<InferInputNote> result();

private:
    void runTask() override;
    void terminate() override;
    void abort();
    void buildPreviewText();
    QString buildInputJson() const;
    bool processOutput(const QString &json);

    QMutex m_mutex;
    QString m_previewText;
    InferDurInput m_input;
    InferDurInput m_result;
    bool m_success = false;
};



#endif // INFERDURATIONTASK_H
