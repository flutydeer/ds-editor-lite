//
// Created by fluty on 2024/1/31.
//

#ifndef PLAYBACKCONTROLLER_H
#define PLAYBACKCONTROLLER_H

#define playbackController PlaybackController::instance()

#include "Model/AppModel/AppModel.h"
#include "Utils/Singleton.h"
#include "Global/PlaybackGlobal.h"

#include <QObject>

using namespace PlaybackGlobal;

class PlaybackControllerPrivate;

class PlaybackController final : public QObject {
    Q_OBJECT

private:
    explicit PlaybackController();
    ~PlaybackController() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(PlaybackController)
    Q_DISABLE_COPY_MOVE(PlaybackController)

    PlaybackStatus playbackStatus() const;

    double position() const;
    double lastPosition() const;

    double tempo() const;

    // bool isPlaying() const;
    // long position() const;
    // long lastPlayPosition() const;

signals:
    void positionChanged(double tick);
    void lastPositionChanged(double tick);
    void playbackStatusChanged(PlaybackStatus status);
    void levelMetersUpdated(const AppModel::LevelMetersUpdatedArgs &args);

public slots:
    void play();
    void pause();
    void stop();

    void setPosition(double tick);
    void setLastPosition(double tick);

    void sampleRateChanged(double sr);
    void onTempoChanged(double tempo);
    void onModelChanged();

private:
    Q_DECLARE_PRIVATE(PlaybackController)
    PlaybackControllerPrivate *d_ptr;
};



#endif // PLAYBACKCONTROLLER_H
