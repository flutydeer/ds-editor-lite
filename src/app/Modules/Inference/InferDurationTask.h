//
// Created by OrangeCat on 24-9-4.
//

#ifndef INFERDURATIONTASK_H
#define INFERDURATIONTASK_H

#include "Model/Inference/InferDurationDataModel.h"
#include "Modules/Task/Task.h"

class InferDurationTask : public Task {

public:
    explicit InferDurationTask(int clipId, const QList<InferDurNote> &input);
    QList<InferDurNote> result();
    int clipId = -1;

private:
    void runTask() override;
    void abort();
    void buildPreviewText();

    QMutex m_mutex;
    QList<InferDurNote> m_notes;
    QString m_previewText;
};



#endif // INFERDURATIONTASK_H
