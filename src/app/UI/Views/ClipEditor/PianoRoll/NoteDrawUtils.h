#ifndef NOTEDRAWUTILS_H
#define NOTEDRAWUTILS_H

#include <algorithm>

namespace NoteDrawUtils {
    inline int lengthForSnappedEnd(const int startTick, const int snappedEndTick,
                                   const int minimumLength) {
        return std::max(minimumLength, snappedEndTick - startTick);
    }
}

#endif // NOTEDRAWUTILS_H
