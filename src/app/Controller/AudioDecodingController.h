//
// Created by fluty on 2024/7/11.
//

#ifndef AUDIODECODINGCONTROLLER_H
#define AUDIODECODINGCONTROLLER_H

#define audioDecodingController AudioDecodingController::instance()

#include "Utils/Singleton.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Track.h"

#include <QObject>

class AudioClip;
class DecodeAudioTask;
class ResolveAudioPathTask;

class AudioDecodingController final : public QObject {
    Q_OBJECT

private:
    explicit AudioDecodingController(QObject *parent = nullptr);
    ~AudioDecodingController() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(AudioDecodingController)
    Q_DISABLE_COPY_MOVE(AudioDecodingController)

public slots:
    void onModelChanged();
    void onTrackChanged(AppModel::TrackChangeType type, qsizetype index, const Track *track);
    void onClipChanged(Track::ClipChangeType type, Clip *clip);

signals:
    // Emitted when all path resolution after project load has finished.
    // missingClipIds / unconfirmedClipIds are clips that need a user decision;
    // autoRelocatedCount is the number of clips relocated automatically (hash verified)
    void resolveSessionFinished(const QList<int> &missingClipIds,
                                const QList<int> &unconfirmedClipIds, int autoRelocatedCount);

private:
    QList<DecodeAudioTask *> m_tasks;
    QList<ResolveAudioPathTask *> m_resolveTasks;

    // Aggregated path resolution state during project load (D6: notify once the counter reaches zero)
    int m_pendingResolveCount = 0;
    QList<int> m_missingClipIds;
    QList<int> m_unconfirmedClipIds;
    int m_autoRelocatedCount = 0;

    void createAndStartTask(AudioClip *clip);
    void createAndStartResolveTask(AudioClip *clip);
    void handleTaskFinished(DecodeAudioTask *task);
    void handleResolveTaskFinished(ResolveAudioPathTask *task);
    void finishResolveIfSessionDone();
    void terminateTaskByClipId(int clipId);
    void terminateTasksByTrackId(int trackId);
};

#endif // AUDIODECODINGCONTROLLER_H
