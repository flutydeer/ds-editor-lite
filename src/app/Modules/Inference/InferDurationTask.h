//
// Created by OrangeCat on 24-9-4.
//

#ifndef INFERDURATIONTASK_H
#define INFERDURATIONTASK_H

#include "Model/AppModel/Phoneme.h"
#include "Modules/Task/Task.h"


class Note;

class InferDurationTask : public Task {

public:
    explicit InferDurationTask(int clipId, const QList<Note *> &notes);
    ~InferDurationTask() override;
    const QList<Note*> &notes() const {
        return m_notes;
    }
    QList<Phoneme> result();
    int clipId = -1;

private:
    void runTask() override;
    void abort();

    QMutex m_mutex;
    QList<Note *> m_notes;
    QString m_previewText;
    QList<Phoneme> m_result;
};



#endif // INFERDURATIONTASK_H
