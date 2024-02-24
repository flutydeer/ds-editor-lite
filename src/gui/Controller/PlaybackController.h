//
// Created by fluty on 2024/1/31.
//

#ifndef PLAYBACKCONTROLLER_H
#define PLAYBACKCONTROLLER_H

#include <QObject>

#include "Model/AppModel.h"
#include "Utils/Singleton.h"

class PlaybackController final : public QObject, public Singleton<PlaybackController> {
    Q_OBJECT
public:
    explicit PlaybackController();
    enum PlaybackStatus {
        Stopped,
        Playing,
        Paused,
    };
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

private:
    double m_position = 0;
    double m_lastPlayPosition = 0;
    double m_sampleRate = 48000;
    double m_tempo = 120;
    PlaybackStatus m_playbackStatus = Stopped;

    double samplePosToTick(int sample) const;
    int tickToSamplePos(double tick) const;
};



#endif // PLAYBACKCONTROLLER_H
