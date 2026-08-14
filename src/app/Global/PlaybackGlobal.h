#ifndef PLAYBACKGLOBAL_H
#define PLAYBACKGLOBAL_H

namespace PlaybackGlobal {
    inline constexpr int positionUpdateIntervalMs = 8;

    enum PlaybackStatus {
        Stopped,
        Playing,
        Paused,
    };
}

#endif //PLAYBACKGLOBAL_H
