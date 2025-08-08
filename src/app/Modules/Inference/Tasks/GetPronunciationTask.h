//
// Created by fluty on 24-9-10.
//

#ifndef GETPRONUNCIATIONTASK_H
#define GETPRONUNCIATIONTASK_H

#include "Modules/Task/Task.h"


class Note;

class GetPronunciationTask : public Task {
    Q_OBJECT

public:
    explicit GetPronunciationTask(int clipId, const QList<Note *> &notes);
    int clipId() const;
    QList<Note *> notesRef;
    QStringList result;

private:
    void runTask() override;
    QList<QString> getPronunciations(const QList<Note *> &notes) const;
    int m_clipId = -1;
    QList<Note *> m_notes;
    QString m_previewText;
};



#endif // GETPRONUNCIATIONTASK_H
