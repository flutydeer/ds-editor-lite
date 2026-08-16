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
}

#endif // NOTERESIZEUTILS_H
