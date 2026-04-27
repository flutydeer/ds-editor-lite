#ifndef TIMELINESNAPUTILS_H
#define TIMELINESNAPUTILS_H

#include "Global/AppGlobal.h"

class TimelineSnapUtils {
public:
    static constexpr int ticksPerWholeNote() {
        return AppGlobal::ticksPerWholeNote;
    }

    static int ticksPerBeat(int denominator) {
        if (denominator <= 0)
            denominator = 4;
        const int step = ticksPerWholeNote() / denominator;
        return step > 0 ? step : 1;
    }

    static int quantizeToTicks(int quantize) {
        if (quantize <= 0)
            quantize = 16;
        const int step = ticksPerWholeNote() / quantize;
        return step > 0 ? step : 1;
    }

    static int quantizeStep(int quantize, bool snapOff = false) {
        return snapOff ? 1 : quantizeToTicks(quantize);
    }

    static int snapDown(int tick, int step) {
        if (step <= 1)
            return tick;
        return floorDiv(tick, step) * step;
    }

    static int snapNearest(int tick, int step) {
        if (step <= 1)
            return tick;
        const int lower = snapDown(tick, step);
        const int upper = lower + step;
        if (tick - lower > upper - tick)
            return upper;
        return lower;
    }

private:
    static int floorDiv(int dividend, int divisor) {
        const int quotient = dividend / divisor;
        const int remainder = dividend % divisor;
        if (remainder < 0)
            return quotient - 1;
        return quotient;
    }
};

#endif // TIMELINESNAPUTILS_H
