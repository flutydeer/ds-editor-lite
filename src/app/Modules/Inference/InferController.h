#ifndef INFERCONTROLLER_H
#define INFERCONTROLLER_H

#define inferController InferController::instance()

#include <lite/Core/Singleton.h>

#include <QObject>

class InferControllerPrivate;
class InferDurationTask;
class InferPitchTask;
class InferVarianceTask;
class InferAcousticTask;
class InferAcousticCacheProbeTask;
class Task;
class Track;

class InferController final : public QObject {
    Q_OBJECT

public:
    LITE_SINGLETON_DECLARE_INSTANCE(InferController)
    Q_DISABLE_COPY_MOVE(InferController)

    void addInferDurationTask(InferDurationTask &task);
    void cancelInferDurationTask(int taskId);
    bool finishCurrentInferDurationTask(InferDurationTask *task = nullptr);

    void addInferPitchTask(InferPitchTask &task);
    void cancelInferPitchTask(int taskId);
    bool finishCurrentInferPitchTask(InferPitchTask *task = nullptr);

    void addInferVarianceTask(InferVarianceTask &task);
    void cancelInferVarianceTask(int taskId);
    bool finishCurrentInferVarianceTask(InferVarianceTask *task = nullptr);

    void addInferAcousticTask(InferAcousticTask &task);
    void cancelInferAcousticTask(int taskId);
    bool finishCurrentInferAcousticTask(InferAcousticTask *task = nullptr);

    void addInferAcousticCacheProbeTask(InferAcousticCacheProbeTask &task);
    void cancelInferAcousticCacheProbeTask(int taskId);
    bool finishCurrentInferAcousticCacheProbeTask(InferAcousticCacheProbeTask *task = nullptr);

    // Starts acoustic inference for all Pending pieces that belong to the given tracks
    // (empty list = all tracks). Idempotent: pipelines not awaiting acoustic inference
    // ignore the trigger. Used by audio export so unrendered pieces do not stall it.
    void startPendingAcousticInference(const QList<Track *> &tracks = {});

    // Suspends inference started on behalf of the given tracks (empty list = all
    // tracks): the currently running acoustic task finishes naturally, the remaining
    // Running/Pending pipelines return to the lazy probe-wait state. Mirrors what
    // playback stop does, so canceling an export behaves like stopping playback.
    void suspendPendingAcousticInference(const QList<Track *> &tracks = {});

private:
    explicit InferController(QObject *parent = nullptr);
    ~InferController() override;

    Q_DECLARE_PRIVATE(InferController)
    InferControllerPrivate *d_ptr;
};



#endif // INFERCONTROLLER_H
