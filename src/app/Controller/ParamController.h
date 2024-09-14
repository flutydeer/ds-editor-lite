//
// Created by OrangeCat on 24-9-3.
//

#ifndef PARAMCONTROLLER_H
#define PARAMCONTROLLER_H

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Track.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/InferDurationTask.h"
#include "Utils/Queue.h"
#include "Utils/Singleton.h"

#include <QObject>



class GetPronunciationTask;
class GetPhonemeNameTask;

class ParamController : public QObject, public Singleton<ParamController> {
    Q_OBJECT

public:
    explicit ParamController();

private slots:
    void onModelChanged();
    void onTrackChanged(AppModel::TrackChangeType type, qsizetype index, Track *track);
    void onClipChanged(Track::ClipChangeType type, Clip *clip);
    void onModuleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);
    void onEditingChanged(bool isEditing);

private:
    void handleClipInserted(Clip *clip);
    void handleClipRemoved(Clip *clip);

    void handleNoteChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes, SingingClip *clip);

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

    QList<Track *> m_tracks;
    // QList<Clip *> m_clips;

    Queue<GetPronunciationTask *> m_getPronTaskQueue;
    GetPronunciationTask *m_runningGetPronTask = nullptr;

    Queue<GetPhonemeNameTask *> m_getPhonemeNameTaskQueue;
    GetPhonemeNameTask *m_runningGetPhonemeNameTask = nullptr;

    Queue<InferDurationTask *> m_inferDurTaskQueue;
    InferDurationTask *m_runningInferDurTask = nullptr;
};



#endif // PARAMCONTROLLER_H
