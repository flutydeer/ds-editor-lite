//
// Created by OrangeCat on 24-9-4.
//

#ifndef INFERDURATIONTASK_H
#define INFERDURATIONTASK_H

#include "IInferTask.h"
#include "Model/Inference/InferDurPitNote.h"
#include "Modules/Task/Task.h"

class InferDurationTask final : public IInferTask {
public:
    struct InferDurInput {
        int clipId = -1;
        int pieceId = -1;
        QList<InferDurPitNote> notes;
        QString configPath;
        double tempo;
        bool operator==(const InferDurInput &other) const;
    };

    int clipId() const override;
    int pieceId() const override;
    [[nodiscard]] bool success() const override;

    explicit InferDurationTask(InferDurInput input);
    InferDurInput input();
    QList<InferDurPitNote> result();

private:
    void runTask() override;
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
