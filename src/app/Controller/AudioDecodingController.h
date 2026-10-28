#ifndef AUDIODECODINGCONTROLLER_H
#define AUDIODECODINGCONTROLLER_H

#define audioDecodingController AudioDecodingController::instance()

#include <lite/Core/Singleton.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QObject>
#include <QPointer>
#include <QHash>
#include <QString>

#include <functional>

class AudioClip;
class DecodeAudioTask;
class DocumentWorkflowController;
class ResolveAudioPathTask;

class AudioDecodingController final : public QObject {
    Q_OBJECT

private:
    explicit AudioDecodingController(QObject *parent = nullptr);
    ~AudioDecodingController() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(AudioDecodingController)
    Q_DISABLE_COPY_MOVE(AudioDecodingController)

    void setGuiServices(DocumentWorkflowController *workflow,
                        std::function<void(const QString &)> notifier = {});

public slots:
    void onModelChanged();
    void onTrackChanged(AppModel::TrackChangeType type, qsizetype index, const Track *track);
    void onClipChanged(Track::ClipChangeType type, Clip *clip);

public:
    // Cascading resolution: use filePath's directory as the candidate and try to resolve all
    // still-missing audio clips (matched by file name + sha512; missing items without sha512 do not
    // participate). Called after the user manually relinks one file, to recover other files moved
    // to the same directory. Hits adopt the new path SILENTLY (no undo entry, no dirty flag) — by
    // design, undo boundaries follow "could the content have changed": a manual relink is an edit
    // (unverified user choice, undoable), while a sha512-verified hit is mathematically the
    // original file, a fact correction with no user intent to revert. Undoing a manual relink
    // therefore does NOT roll back cascade hits it triggered
    void resolveMissingClipsNear(const QString &filePath);

    // Waveform decoding in flight for this clip. The editor draws a placeholder while the peaks
    // are not readable yet, so the empty clip body never reads as a silent waveform.
    [[nodiscard]] bool isDecoding(int clipId) const;

signals:
    // Emitted when a clip enters or leaves the decoding set (isDecoding()). Views cannot derive
    // this from Clip signals: decoding starts without touching the model and finishes by writing
    // audioInfo, so the pair of edges has no single owner signal.
    void decodingStateChanged(int clipId, bool decoding);

    // Emitted when cascading resolution recovers a clip (the missing-media dialog refreshes its
    // row)
    void clipRelocated(int clipId, const QString &newPath);

    // Emitted for interactive requests when all path resolution has finished.
    // missingClipIds / unconfirmedClipIds are clips that need a user decision;
    // autoRelocatedCount is the number of clips relocated automatically (hash verified)
    void resolveSessionFinished(const QList<int> &missingClipIds,
                                const QList<int> &unconfirmedClipIds, int autoRelocatedCount);

private:
    QList<DecodeAudioTask *> m_tasks;
    QList<ResolveAudioPathTask *> m_resolveTasks;
    // Clip id -> the decode task currently responsible for it. A terminated task lingers in
    // m_tasks until its queued completion slot runs, and a relink may already have queued its
    // successor, so ownership is decided by identity rather than by "is a task running".
    QHash<int, DecodeAudioTask *> m_decodingTaskByClip;

    // Aggregated path resolution state during project load (D6: notify once the counter reaches
    // zero)
    int m_pendingResolveCount = 0;
    QList<int> m_missingClipIds;
    QList<int> m_unconfirmedClipIds;
    int m_autoRelocatedCount = 0;
    quint64 m_modelChangeEpoch = 0;
    QPointer<DocumentWorkflowController> m_documentWorkflow;
    std::function<void(const QString &)> m_notifier;

    // 路径回写同步触发 sourceChanged，后续任务沿用原检查的交互策略。
    struct SourceChangeContext {
        QPointer<AudioClip> clip;
        bool interactive = false;
    };

    SourceChangeContext m_sourceChangeContext;

    void startDecodingOrResolving(AudioClip *clip, bool forceDecode, bool interactive);
    void createAndStartTask(AudioClip *clip, bool interactive);
    void createAndStartResolveTask(AudioClip *clip, bool interactive);
    void connectClip(AudioClip *clip);
    void startDecodingAndResolving(bool interactive);
    void handleTaskFinished(DecodeAudioTask *task, bool interactive);
    void handleResolveTaskFinished(ResolveAudioPathTask *task, bool interactive);
    void handleCascadeResolveTaskFinished(ResolveAudioPathTask *task, bool interactive);
    void finishResolveIfSessionDone();
    void terminateTaskByClipId(int clipId);
    void terminateTasksByTrackId(int trackId);
};

#endif // AUDIODECODINGCONTROLLER_H
