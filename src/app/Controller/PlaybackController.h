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
class PlaybackController final : public QObject, public Singleton<PlaybackController> {
    Q_OBJECT
public:
    explicit PlaybackController();
    ~PlaybackController() override;
    [[nodiscard]] PlaybackStatus playbackStatus() const;

    [[nodiscard]] double position() const;
    [[nodiscard]] double lastPosition() const;

    [[nodiscard]] double tempo() const;

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
