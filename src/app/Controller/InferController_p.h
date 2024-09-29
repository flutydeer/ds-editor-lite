//
// Created by fluty on 24-9-26.
//

#ifndef INFERCONTROLLERPRIVATE_H
#define INFERCONTROLLERPRIVATE_H

#include "ModelChangeHandler.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppModel/Track.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/InferDurationTask.h"
#include "Modules/Task/TaskQueue.h"

class GetPronunciationTask;
class GetPhonemeNameTask;
class InferController;

class InferControllerPrivate : public ModelChangeHandler {
    Q_OBJECT
    Q_DECLARE_PUBLIC(InferController)

public:
    explicit InferControllerPrivate(InferController *q) : ModelChangeHandler(q), q_ptr(q){};

public slots:
    void onModuleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);
    void onEditingChanged(AppStatus::EditObjectType type);

public:
    void handleSingingClipInserted(SingingClip *clip) override;
    void handleSingingClipRemoved(SingingClip *clip) override;
    void handleNoteChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes,
                           SingingClip *clip) override;

    void handleLanguageModuleStatusChanged(AppStatus::ModuleStatus status);
    void handleGetPronTaskFinished(GetPronunciationTask *task);
    void handleGetPhoneTaskFinished(GetPhonemeNameTask *task);
    void handleInferDurTaskFinished(InferDurationTask *task);

    void createAndRunGetPronTask(SingingClip *clip);
    void createAndRunGetPhoneTask(SingingClip *clip);
    void createAndRunInferDurTask(SingingClip *clip);
    void cancelClipRelatedTasks(const Clip *clip);
    void cancelPieceRelatedTasks(const InferPiece *piece);

    void runNextGetPronTask();
    void runNextGetPhoneTask();
    void runNextInferDurTask();

    AppStatus::EditObjectType m_lastEditObjectType = AppStatus::EditObjectType::None;

    QList<Track *> m_tracks;
    QMap<int, QList<int>> m_clipPieceDict;

    TaskQueue<GetPronunciationTask> m_getPronTasks;
    TaskQueue<GetPhonemeNameTask> m_getPhoneTasks;
    TaskQueue<InferDurationTask> m_inferDurTasks;

private:
    InferController *q_ptr = nullptr;
};



#endif // INFERCONTROLLERPRIVATE_H
