//
// Created by fluty on 24-9-26.
//

#ifndef INFERCONTROLLERPRIVATE_H
#define INFERCONTROLLERPRIVATE_H

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppModel/Track.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/InferDurationTask.h"
#include "Utils/Queue.h"

#include <QObject>

class GetPronunciationTask;
class GetPhonemeNameTask;
class InferController;

class InferControllerPrivate : public QObject {
    Q_OBJECT
    Q_DECLARE_PUBLIC(InferController)

public:
    explicit InferControllerPrivate(InferController *q) : q_ptr(q){};

public slots:
    void onModelChanged();
    void onTrackChanged(AppModel::TrackChangeType type, qsizetype index, Track *track);
    void onClipChanged(Track::ClipChangeType type, Clip *clip);
    void onModuleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);
    void onEditingChanged(AppStatus::EditObjectType type);

public:
    void handleClipInserted(Clip *clip);
    void handleClipRemoved(Clip *clip);

    void handleNoteChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes,
                           SingingClip *clip);

    void handleLanguageModuleStatusChanged(AppStatus::ModuleStatus status);
    void handleGetPronTaskFinished(GetPronunciationTask *task);
    void handleGetPhonemeNameTaskFinished(GetPhonemeNameTask *task);
    void handleInferDurTaskFinished(InferDurationTask *task);
    static bool validateForInferDuration(int clipId);

    void createAndRunGetPronTask(SingingClip *clip);
    void createAndRunGetPhonemeNameTask(SingingClip *clip);
    void createAndRunInferDurTask(SingingClip *clip);
    void cancelClipRelatedTasks(Clip *clip);
    void runNextGetPronTask();
    void runNextGetPhonemeNameTask();
    void runNextInferDurTask();

    AppStatus::EditObjectType m_lastEditObjectType = AppStatus::EditObjectType::None;

    QList<Track *> m_tracks;
    // QList<Clip *> m_clips;

    Queue<GetPronunciationTask *> m_getPronTaskQueue;
    GetPronunciationTask *m_runningGetPronTask = nullptr;

    Queue<GetPhonemeNameTask *> m_getPhonemeNameTaskQueue;
    GetPhonemeNameTask *m_runningGetPhonemeNameTask = nullptr;

    Queue<InferDurationTask *> m_inferDurTaskQueue;
    InferDurationTask *m_runningInferDurTask = nullptr;

private:
    InferController *q_ptr = nullptr;
};



#endif // INFERCONTROLLERPRIVATE_H
