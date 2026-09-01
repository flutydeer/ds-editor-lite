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

    static qint64 snapNearestWide(const qint64 tick, const qint64 step) {
        if (step <= 1)
            return tick;
        const qint64 lower = floorDiv(tick, step) * step;
        const qint64 upper = lower + step;
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

    static qint64 snapNearestWide(const int tick, const qint64 step, const Timeline &timeline) {
        if (step <= 1)
            return tick;
        if (tick < 0)
            return snapNearestWide(static_cast<qint64>(tick), step);
        const qint64 bar = timeline.tickToTime(tick).measure;
        const qint64 barStart = barToTickWide(timeline, bar);
        const qint64 barTicks = timeline.timeSignatureAt(static_cast<int>(bar)).ticksPerBar();
        qint64 lower;
        qint64 upper;
        if (step >= barTicks) {
            const qint64 hop = step / barTicks > 0 ? step / barTicks : 1;
            const qint64 lowerBar = bar - bar % hop;
            lower = barToTickWide(timeline, lowerBar);
            upper = barToTickWide(timeline, lowerBar + hop);
        } else {
            lower = barStart + (tick - barStart) / step * step;
            const qint64 barEnd = barStart + barTicks;
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

    static qint64 floorDiv(const qint64 dividend, const qint64 divisor) {
        const qint64 quotient = dividend / divisor;
        const qint64 remainder = dividend % divisor;
        if (remainder < 0)
            return quotient - 1;
        return quotient;
    }

    static qint64 barToTickWide(const Timeline &timeline, const qint64 bar) {
        const auto &signatures = timeline.timeSignatures();
        qint64 tick = 0;
        qsizetype index = 0;
        for (qsizetype i = 1; i < signatures.size() && signatures.at(i).barIndex <= bar; ++i) {
            tick +=
                (static_cast<qint64>(signatures.at(i).barIndex) - signatures.at(i - 1).barIndex) *
                signatures.at(i - 1).ticksPerBar();
            index = i;
        }
        return tick + (bar - signatures.at(index).barIndex) * signatures.at(index).ticksPerBar();
    }
};

#endif // TIMELINESNAPUTILS_H
