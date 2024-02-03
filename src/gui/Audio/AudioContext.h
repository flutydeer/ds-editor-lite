//
// Created by Crs_1 on 2024/2/4.
//

#ifndef DS_EDITOR_LITE_AUDIOCONTEXT_H
#define DS_EDITOR_LITE_AUDIOCONTEXT_H

#include <QObject>

#include "Controller/PlaybackController.h"

class AudioContext : public QObject {
    Q_OBJECT
public:
    explicit AudioContext(QObject *parent = nullptr);

public slots:
    void handlePlaybackStatusChange(PlaybackController::PlaybackStatus status);
    void handlePlaybackPositionChange(double positionTick);

    void handleVstCallbackPositionChange(qint64 positionSample);
};



#endif // DS_EDITOR_LITE_AUDIOCONTEXT_H
