#ifndef TIMELINESNAPUTILS_H
#define TIMELINESNAPUTILS_H

#include "MusicTime.h"
#include "Timeline.h"

class TimelineSnapUtils {
public:
    static constexpr int ticksPerWholeNote() {
        return MusicTime::ticksPerWholeNote;
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

    // Measure-anchored snapping for absolute positions: the grid restarts at
    // every measure line, so positions land on the lines the user actually
    // sees when the time signature changes mid-project. Steps of a whole
    // measure or more snap in whole measures, matching the thinned-out bar
    // grid at low zoom. Deltas and lengths should keep using the plain
    // overloads above.
    static int snapDown(const int tick, const int step, const Timeline &timeline) {
        if (step <= 1)
            return tick;
        if (tick < 0)
            return snapDown(tick, step);
        const int bar = timeline.tickToTime(tick).measure;
        const int barStart = timeline.barToTick(bar);
        const int barTicks = timeline.timeSignatureAt(bar).ticksPerBar();
        if (step >= barTicks) {
            const int hop = step / barTicks > 0 ? step / barTicks : 1;
            return timeline.barToTick(bar - bar % hop);
        }
        return barStart + (tick - barStart) / step * step;
    }

    static int snapNearest(const int tick, const int step, const Timeline &timeline) {
        if (step <= 1)
            return tick;
        if (tick < 0)
            return snapNearest(tick, step);
        const int bar = timeline.tickToTime(tick).measure;
        const int barStart = timeline.barToTick(bar);
        const int barTicks = timeline.timeSignatureAt(bar).ticksPerBar();
        int lower;
        int upper;
        if (step >= barTicks) {
            const int hop = step / barTicks > 0 ? step / barTicks : 1;
            const int lowerBar = bar - bar % hop;
            lower = timeline.barToTick(lowerBar);
            upper = timeline.barToTick(lowerBar + hop);
        } else {
            lower = barStart + (tick - barStart) / step * step;
            // The next measure line is always a snap target, even when the
            // step does not divide the measure evenly.
            const int barEnd = barStart + barTicks;
            upper = lower + step < barEnd ? lower + step : barEnd;
        }
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
