//
// Created by OrangeCat on 24-9-4.
//

#ifndef INFERDURATIONTASK_H
#define INFERDURATIONTASK_H

#include "Model/Inference/InferDurNote.h"
#include "Modules/Task/Task.h"

class InferDurationTask final : public Task {
public:
    struct InferDurInput {
        int clipId = -1;
        int pieceId = -1;
        QList<InferDurNote> notes;
        QString configPath;
        double tempo;
        bool operator==(const InferDurInput &other) const;
    };

    int clipId() const;
    int pieceId() const;
    bool success = false;

    explicit InferDurationTask(InferDurInput input);
    InferDurInput input();
    QList<InferDurNote> result();

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
};



#endif // INFERDURATIONTASK_H
