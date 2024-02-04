//
// Created by fluty on 2024/2/4.
//

#ifndef VOLUMEUTILS_H
#define VOLUMEUTILS_H

#include <QtGlobal>

class VolumeUtils {
public:
    static float linearTodB(float volume) {
        if (volume > 0)
            return (float) qMax(20 * std::log10(volume), -70.0);
        return -70;
    }
    static float dBToLinear(float gain) {
        if (gain <= -70)
            return 0;
        return std::pow(10, gain / 20.0);
    }
};


#endif // VOLUMEUTILS_H
