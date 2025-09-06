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

private:
    QList<DecodeAudioTask *> m_tasks;

    void createAndStartTask(AudioClip *clip);
    void handleTaskFinished(DecodeAudioTask *task);
    void terminateTaskByClipId(int clipId);
    void terminateTasksByTrackId(int trackId);
};

#endif // AUDIODECODINGCONTROLLER_H
