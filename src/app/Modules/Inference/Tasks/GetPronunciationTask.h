//
// Created by fluty on 24-9-10.
//

#ifndef GETPRONUNCIATIONTASK_H
#define GETPRONUNCIATIONTASK_H

#include "Modules/Inference/Models/NoteInferenceSnapshot.h"
#include "Modules/PackageManager/Models/SingerInfo.h"
#include "Modules/Task/Task.h"

#include <QStringList>

class GetPronunciationTask : public Task {
    Q_OBJECT

public:
    explicit GetPronunciationTask(int clipId, quint64 clipRevision,
                                  const QList<NoteInferenceSnapshot> &notes,
                                  const SingerInfo &singerInfo);
    int clipId() const;
    quint64 clipRevision() const;
    QList<int> noteIds() const;
    QStringList result;

private:
    void runTask() override;
    QStringList getPronunciations(const QList<NoteInferenceSnapshot> &notes) const;
    int m_clipId = -1;
    quint64 m_clipRevision = 0;
    SingerInfo m_singerInfo;
    QList<NoteInferenceSnapshot> m_notes;
    QString m_previewText;
};



#endif // GETPRONUNCIATIONTASK_H
