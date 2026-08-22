#ifndef GETPRONUNCIATIONTASK_H
#define GETPRONUNCIATIONTASK_H

#include "Automation/AutomationTypes.h"
#include "Modules/Inference/Models/NoteInferenceSnapshot.h"
#include "Modules/Inference/Models/PronunciationFetchResult.h"
#include <lite/ProjectModel/Voice/SingerInfo.h>
#include <lite/Tasking/Task.h>

#include <QStringList>

class GetPronunciationTask final : public Task {
    Q_OBJECT

public:
    explicit GetPronunciationTask(Automation::DocumentVersion documentVersion, int clipId,
                                  quint64 clipRevision,
                                  const QList<NoteInferenceSnapshot> &notes,
                                  const SingerInfo &singerInfo);
    Automation::DocumentVersion documentVersion() const;
    int clipId() const;
    quint64 clipRevision() const;
    QList<int> noteIds() const;
    QList<PronunciationFetchResult> result;

private:
    void runTask() override;
    QList<PronunciationFetchResult>
        getPronunciationResults(const QList<NoteInferenceSnapshot> &notes) const;
    int m_clipId = -1;
    Automation::DocumentVersion m_documentVersion;
    quint64 m_clipRevision = 0;
    SingerInfo m_singerInfo;
    QList<NoteInferenceSnapshot> m_notes;
    QString m_previewText;
};



#endif // GETPRONUNCIATIONTASK_H
