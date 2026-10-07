#ifndef NOTERESIZEUTILS_H
#define NOTERESIZEUTILS_H

#include <algorithm>

namespace NoteResizeUtils {
    inline int clampLeftDelta(const int originalLength, const int requestedDelta,
                              const int minimumLength = 1) {
        return std::min(requestedDelta,
                        originalLength - std::max(1, minimumLength));
    }

    inline int clampRightDelta(const int originalLength, const int requestedDelta,
                               const int minimumLength = 1) {
        return std::max(requestedDelta,
                        std::max(1, minimumLength) - originalLength);
    }

    inline int clampLeftMoveDelta(const int requestedDelta, const int minLocalStart) {
        // Do not allow a note to be moved to the left of clip start (tick 0 inside the clip)
        return std::max(requestedDelta, -minLocalStart);
    }

    // Delta of a shared boundary dragged jointly between two adjacent notes: the
    // left note lengthens by the delta, the right one shortens by it, so the legal
    // range is bounded from below by the left note's minimum length and from above
    // by the right one's. A note already shorter than the minimum inverts the
    // range; the boundary then stays still instead of picking a side.
    inline int clampJointBoundaryDelta(const int leftNoteLength, const int rightNoteLength,
                                       const int requestedDelta, const int minimumLength = 1) {
        const auto minLength = std::max(1, minimumLength);
        const auto lower = minLength - leftNoteLength;
        const auto upper = rightNoteLength - minLength;
        if (lower > upper)
            return 0;
        return std::clamp(requestedDelta, lower, upper);
    }
}

#endif // NOTERESIZEUTILS_H
