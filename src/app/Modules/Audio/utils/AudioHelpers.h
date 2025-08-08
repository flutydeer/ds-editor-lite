#ifndef AUDIO_AUDIOHELPERS_H
#define AUDIO_AUDIOHELPERS_H

#include <QtGlobal>

class AudioHelpers {
public:
    static constexpr qint64 msecToSample(const int msec, const double sampleRate) {
        return static_cast<qint64>(sampleRate / 1000.0 * msec);
    }
};

#endif // AUDIO_AUDIOHELPERS_H
