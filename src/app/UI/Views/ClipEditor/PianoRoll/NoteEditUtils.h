#ifndef NOTEEDITUTILS_H
#define NOTEEDITUTILS_H

#include <lite/MusicBase/TimelineSnapUtils.h>

#include <algorithm>

namespace NoteEditUtils {
    inline int snapLocalDown(const double globalTick, const int clipStart, const int step,
                             const Timeline &timeline) {
        return TimelineSnapUtils::snapDown(static_cast<int>(globalTick), step, timeline) -
               clipStart;
    }

    inline int snapLocalNearest(const double globalTick, const int clipStart, const int step,
                                const Timeline &timeline) {
        return TimelineSnapUtils::snapNearest(static_cast<int>(globalTick), step, timeline) -
               clipStart;
    }

    inline int moveDelta(const double rawDeltaTick, const int step) {
        return TimelineSnapUtils::snapNearest(static_cast<int>(rawDeltaTick), step);
    }

    inline int leftResizeDelta(const int originalStart, const int originalLength,
                               const int snappedLocalTick, const int minimumLength) {
        return std::min(snappedLocalTick - originalStart, originalLength - minimumLength);
    }

    inline int rightResizeDelta(const int originalStart, const int originalLength,
                                const int snappedLocalTick, const int minimumLength) {
        return std::max(snappedLocalTick - originalStart - originalLength,
                        minimumLength - originalLength);
    }

    inline int lengthForSnappedEnd(const int startTick, const int snappedEndTick,
                                   const int minimumLength) {
        return std::max(minimumLength, snappedEndTick - startTick);
    }
}

#endif // NOTEEDITUTILS_H
