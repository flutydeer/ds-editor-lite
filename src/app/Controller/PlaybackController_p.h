#ifndef PLAYBACKCONTROLLER_P_H
#define PLAYBACKCONTROLLER_P_H

class PlaybackControllerPrivate : public QObject {
    Q_OBJECT
    Q_DECLARE_PUBLIC(PlaybackController)

public:
    explicit PlaybackControllerPrivate(PlaybackController *q) : q_ptr(q) {
    }

    double m_position = 0;
    double m_lastPlayPosition = 0;
    double m_sampleRate = 48000;
    PlaybackStatus m_playbackStatus = Stopped;

private:
    PlaybackController *q_ptr;
};

#endif // PLAYBACKCONTROLLER_P_H
