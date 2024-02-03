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
    explicit PlaybackController() = default;

    // bool isPlaying() const;
    // long position() const;
    // long lastPlayPosition() const;

signals:
    void positionChanged(double tick);
    void lastPositionChanged(double tick);

public slots:
    void play(){};
    void pause(){};
    void stop(){};
    void setPosition(double tick){};

private:
    long m_lastPlayPosition;
};



#endif // PLAYBACKCONTROLLER_H
