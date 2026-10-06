#ifndef NOTEEDITUTILS_H
#define NOTEEDITUTILS_H

#include <lite/MusicBase/TimelineSnapUtils.h>
#include <lite/ProjectModel/Utils/NoteResizeUtils.h>

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
        const auto requestedDelta = snappedLocalTick - originalStart;
        const auto lengthSafeDelta =
            NoteResizeUtils::clampLeftDelta(originalLength, requestedDelta, minimumLength);
        // Do not resize the left edge past clip start (tick 0 inside the clip)
        return NoteResizeUtils::clampLeftMoveDelta(lengthSafeDelta, originalStart);
    }

    inline int rightResizeDelta(const int originalStart, const int originalLength,
                                const int snappedLocalTick, const int minimumLength) {
        return NoteResizeUtils::clampRightDelta(
            originalLength, snappedLocalTick - originalStart - originalLength, minimumLength);
    }

    // Delta of a boundary dragged jointly between two adjacent notes. The boundary
    // sits where the left note ends (== where the right note starts); the snap and
    // minimum-length rules match the single-note resize of the same edge.
    inline int jointBoundaryDelta(const int leftOriginalLength, const int rightOriginalLength,
                                  const int originalBoundaryTick, const int snappedLocalTick,
                                  const int minimumLength) {
        return NoteResizeUtils::clampJointBoundaryDelta(leftOriginalLength, rightOriginalLength,
                                                        snappedLocalTick - originalBoundaryTick,
                                                        minimumLength);
    }

    inline int lengthForSnappedEnd(const int startTick, const int snappedEndTick,
                                   const int minimumLength) {
        return std::max(std::max(1, minimumLength), snappedEndTick - startTick);
    }
}

#endif // NOTEEDITUTILS_H
