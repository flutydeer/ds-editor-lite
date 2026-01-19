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
#include "Global/PlaybackGlobal.h"
#include "Global/AppOptionsGlobal.h"

#include <QList>

class GetPronunciationTask;
class GetPhonemeNameTask;
class InferController;
class InferPipeline;

class InferControllerPrivate final : public ModelChangeHandler {
    Q_OBJECT
    Q_DECLARE_PUBLIC(InferController)

public:
    explicit InferControllerPrivate(InferController *q) : ModelChangeHandler(q), q_ptr(q) {};

public slots:
    void onModuleStatusChanged(AppStatus::ModuleType module, AppStatus::ModuleStatus status);
    void onEditingChanged(AppStatus::EditObjectType type);
    void onInferOptionChanged(AppOptionsGlobal::Option option);
    void onPlaybackStatusChanged(PlaybackGlobal::PlaybackStatus status);

public:
    void handleTempoChanged(double tempo) override;
    void handleSingingClipInserted(SingingClip *clip) override;
    void handleSingingClipRemoved(SingingClip *clip) override;
    void handlePiecesChanged(const PieceList &newPieces, const PieceList &discardedPieces,
                             SingingClip *clip) override;
    void handleNoteChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes,
                           SingingClip *clip) override;
    void handleParamChanged(ParamInfo::Name name, Param::Type type, SingingClip *clip) override;

    void handleLanguageModuleStatusChanged(AppStatus::ModuleStatus status);
    void handleGetPronTaskFinished(GetPronunciationTask &task);
    void handleGetPhoneTaskFinished(GetPhonemeNameTask &task);
    void handleInferAcousticTaskFinished(InferAcousticTask &task);

    static void recreateAllInferTasks();

    void createAndRunGetPronTask(const SingingClip &clip);
    void createAndRunGetPhoneTask(const SingingClip &clip);

    void createPipeline(InferPiece &piece);
    void createAndRunInferAcousticTask(InferPiece &piece);

    void reset();

    void cancelAllInferTasks();

    void cancelClipRelatedTasks(const SingingClip *clip);
    void cancelPieceRelatedTasks(int pieceId);

    AppStatus::EditObjectType m_lastEditObjectType = AppStatus::EditObjectType::None;

    TaskQueue<GetPronunciationTask> m_getPronTasks;
    TaskQueue<GetPhonemeNameTask> m_getPhoneTasks;
    TaskQueue<InferDurationTask> m_inferDurTasks;
    TaskQueue<InferPitchTask> m_inferPitchTasks;
    TaskQueue<InferVarianceTask> m_inferVarianceTasks;
    TaskQueue<InferAcousticTask> m_inferAcousticTasks;

    QList<InferPipeline *> m_inferPipelines;

    bool m_autoStartAcousticInfer = true;

private:
    InferController *q_ptr = nullptr;
};


#endif // INFERCONTROLLERPRIVATE_H
