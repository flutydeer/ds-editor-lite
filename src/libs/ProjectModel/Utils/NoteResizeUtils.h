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
}

#endif // NOTERESIZEUTILS_H
