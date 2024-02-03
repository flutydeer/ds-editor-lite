//
// Created by fluty on 2024/1/31.
//

#ifndef PLAYBACKCONTROLLER_H
#define PLAYBACKCONTROLLER_H

#include <QObject>

#include "Utils/Singleton.h"

class PlaybackController final : public QObject, public Singleton<PlaybackController> {
    Q_OBJECT
public:
    explicit PlaybackController();

    // bool isPlaying() const;
    // long position() const;
    // long lastPlayPosition() const;

signals:
    void positionChanged(double tick);
    void lastPositionChanged(double tick);

public slots:
    void play();
    void pause(){};
    void stop(){};
    void setPosition(double tick);
    void sampleRateChanged(int sr);
    void onTempoChanged(double tempo);

private:
    long m_position = 0;
    long m_lastPlayPosition = 0;
    int m_sampleRate = 48000;
    double m_tempo = 120;

    double samplePosToTick(int sample) const;
    int tickToSamplePos(double tick) const;
};



#endif // PLAYBACKCONTROLLER_H
