//
// Created by fluty on 24-9-26.
//

#ifndef INFERCONTROLLERPRIVATE_H
#define INFERCONTROLLERPRIVATE_H

#include "Controller/ModelChangeHandler.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Task/TaskQueue.h"
#include "Modules/Inference/EditSessionManager.h"
#include "Modules/Inference/Models/InferenceTaskContext.h"
#include "Modules/Inference/Models/PhonemeNameResult.h"
#include "Tasks/InferAcousticTask.h"
#include "Tasks/InferAcousticCacheProbeTask.h"
#include "Tasks/InferDurationTask.h"
#include "Tasks/InferPitchTask.h"
#include "Tasks/InferVarianceTask.h"
#include "Global/PlaybackGlobal.h"
#include "Global/AppOptionsGlobal.h"

#include <QList>
#include <QHash>
#include <QStringList>

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
    void onEditSessionEnded(const EditSession &session, EditSessionEndReason reason);
    void onInferOptionChanged(AppOptionsGlobal::Option option);
    void onPlaybackStatusChanged(PlaybackGlobal::PlaybackStatus status);

protected:
    void handleModelChanged() override;
    void handleTempoChanged(double tempo) override;
    void handleSingingClipInserted(SingingClip *clip) override;
    void handleSingingClipRemoved(SingingClip *clip) override;
    void handlePiecesChanged(const PieceList &newPieces, const PieceList &discardedPieces,
                             SingingClip *clip) override;
    void handleNoteChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes,
                           SingingClip *clip) override;
    void handleParamChanged(ParamInfo::Name name, Param::Type type, SingingClip *clip) override;
    void handleSpeakerMixChanged(SingingClip *clip) override;

public:
    void handleLanguageModuleStatusChanged(AppStatus::ModuleStatus status);
    void handleGetPronTaskFinished(GetPronunciationTask &task);
    void handleGetPhoneTaskFinished(GetPhonemeNameTask &task);

    bool allRequiredModulesReady() const;
    bool canStartClipInference(const SingingClip &clip) const;
    void scheduleRetryAllSingingClips();
    void retryAllSingingClips();

    static void recreateAllInferTasks();

    void createAndRunGetPronTask(const SingingClip &clip);
    void createAndRunGetPhoneTask(const SingingClip &clip);

    void createPipeline(InferPiece &piece);
    void handlePipelineDropped(InferPipeline *pipeline, const QString &reason);

    void reset();

    void cancelAllInferTasks();

    void cancelClipRelatedTasks(const SingingClip *clip);
    void cancelPieceRelatedTasks(int pieceId);

    enum class PendingApplyResult { Applied, Dropped, Deferred };

    struct PendingPronunciationApply {
        InferenceTaskContext context;
        QStringList pronunciations;
    };

    struct PendingPhonemeNameApply {
        InferenceTaskContext context;
        QList<PhonemeNameResult> phonemeNames;
    };

    PendingApplyResult tryApplyPronunciation(const InferenceTaskContext &context,
                                             const QStringList &pronunciations,
                                             const QString &phase);
    PendingApplyResult tryApplyPhonemeName(const InferenceTaskContext &context,
                                           const QList<PhonemeNameResult> &phonemeNames,
                                           const QString &phase);
    void storePendingPronunciationApply(const InferenceTaskContext &context,
                                        const QStringList &pronunciations);
    void storePendingPhonemeNameApply(const InferenceTaskContext &context,
                                      const QList<PhonemeNameResult> &phonemeNames);
    void flushPendingApplies(const EditSession &session, EditSessionEndReason reason);
    void clearAllPendingApplies(const QString &reason);
    void clearPendingForClip(int clipId, const QString &reason);

    void notifyNextPipeline(const QList<InferPipeline *> &pipelines, int index);

    AppStatus::EditObjectType m_lastEditObjectType = AppStatus::EditObjectType::None;

    TaskQueue<GetPronunciationTask> m_getPronTasks;
    TaskQueue<GetPhonemeNameTask> m_getPhoneTasks;
    TaskQueue<InferDurationTask> m_inferDurTasks;
    TaskQueue<InferPitchTask> m_inferPitchTasks;
    TaskQueue<InferVarianceTask> m_inferVarianceTasks;
    TaskQueue<InferAcousticTask> m_inferAcousticTasks;
    TaskQueue<InferAcousticCacheProbeTask> m_inferAcousticCacheProbeTasks;

    QHash<int, PendingPronunciationApply> m_pendingPronunciationApplies;
    QHash<int, PendingPhonemeNameApply> m_pendingPhonemeNameApplies;

    QList<InferPipeline *> m_inferPipelines;

    bool m_autoStartAcousticInfer = true;
    bool m_retryAllScheduled = false;

private:
    InferController *q_ptr = nullptr;
};


#endif // INFERCONTROLLERPRIVATE_H
