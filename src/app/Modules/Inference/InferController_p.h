//
// Created by fluty on 24-9-26.
//

#ifndef INFERCONTROLLERPRIVATE_H
#define INFERCONTROLLERPRIVATE_H

#include "Controller/ModelChangeHandler.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Task/TaskQueue.h"
#include "Tasks/InferAcousticTask.h"
#include "Tasks/InferDurationTask.h"
#include "Tasks/InferPitchTask.h"
#include "Tasks/InferVarianceTask.h"

class GetPronunciationTask;
class GetPhonemeNameTask;
class InferController;

class InferControllerPrivate final : public ModelChangeHandler {
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
    void handleGetPronTaskFinished(GetPronunciationTask &task);
    void handleGetPhoneTaskFinished(GetPhonemeNameTask &task);
    void handleInferDurTaskFinished(InferDurationTask &task);
    void handleInferPitchTaskFinished(InferPitchTask &task);
    void handleInferVarianceTaskFinished(InferVarianceTask &task);
    void handleInferAcousticTaskFinished(InferAcousticTask &task);

    void createAndRunGetPronTask(SingingClip &clip);
    void createAndRunGetPhoneTask(SingingClip &clip);

    void createAndRunInferDurTask(InferPiece &piece);
    void createAndRunInferPitchTask(InferPiece &piece);
    void createAndRunInferVarianceTask(InferPiece &piece);
    void createAndRunInferAcousticTask(InferPiece &piece);

    void cancelClipRelatedTasks(SingingClip *clip);
    void cancelPieceRelatedTasks(int pieceId);

    void runNextGetPronTask();
    void runNextGetPhoneTask();
    void runNextInferDurTask();
    void runNextInferPitchTask();
    void runNextInferVarianceTask();
    void runNextInferAcousticTask();

    AppStatus::EditObjectType m_lastEditObjectType = AppStatus::EditObjectType::None;


    QMap<int /*pieceId*/, InferDurationTask::InferDurInput> m_lastInferDurInputs;
    QMap<int /*pieceId*/, InferPitchTask::InferPitchInput> m_lastInferPitchInputs;
    QMap<int /*pieceId*/, InferVarianceTask::InferVarianceInput> m_lastInferVarianceInputs;
    QMap<int /*pieceId*/, InferAcousticTask::InferAcousticInput> m_lastInferAcousticInputs;

    TaskQueue<GetPronunciationTask> m_getPronTasks;
    TaskQueue<GetPhonemeNameTask> m_getPhoneTasks;
    TaskQueue<InferDurationTask> m_inferDurTasks;
    TaskQueue<InferPitchTask> m_inferPitchTasks;
    TaskQueue<InferVarianceTask> m_inferVarianceTasks;
    TaskQueue<InferAcousticTask> m_inferAcousticTasks;

    const QString m_singerConfigPath =
        R"(E:\Qixuan_v2.5.0_DiffSinger_OpenUtau\dsconfig.yaml)";

private:
    InferController *q_ptr = nullptr;
};



#endif // INFERCONTROLLERPRIVATE_H
