//
// Created by OrangeCat on 24-9-3.
//

#ifndef PARAMCONTROLLER_H
#define PARAMCONTROLLER_H

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Track.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/InferDurationTask.h"
#include "Utils/Singleton.h"

#include <QObject>


class GetPronTask;

class ParamController : public QObject, public Singleton<ParamController> {
    Q_OBJECT

public:
    explicit ParamController();

private slots:
    void onModelChanged();
    void onTrackChanged(AppModel::TrackChangeType type, qsizetype index, Track *track);
    void onClipChanged(Track::ClipChangeType type, Clip *clip);
    void onModuleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);

private:
    void handleClipInserted(Clip *clip);
    void handleClipRemoved(Clip *clip);

    void handleNoteChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes, SingingClip *clip);

    void handleLanguageModuleStatusChanged(AppStatus::ModuleStatus status);

    void handleGetPronTaskFinished(GetPronTask *task, bool terminate);
    void handleInferDurTaskFinished(InferDurationTask *task, bool terminate);
    static bool validateForInferDuration(int clipId);

    void createAndStartGetPronTask(SingingClip *clip);
    void cancelClipRelatedTasks(Clip *clip);

    QList<Track *> m_tracks;
    // QList<Clip *> m_clips;
    QList<GetPronTask *> m_pendingGetPronTasks;
    QList<GetPronTask *> m_runningGetPronTasks;

    QList<InferDurationTask *> m_pendingInferDurTasks;
    QList<InferDurationTask *> m_runningInferDurTasks;
};



#endif // PARAMCONTROLLER_H
