#ifndef CLIPRESIZEUTILS_H
#define CLIPRESIZEUTILS_H

#include <lite/ProjectModel/AppModel/Clip.h>

#include <algorithm>

namespace ClipResizeUtils {
    template <typename Iterator, typename EndPosition>
    int furthestContentEnd(Iterator first, const Iterator last, const int emptyContentLength,
                           EndPosition endPosition) {
        if (first == last)
            return emptyContentLength;

        auto furthestEnd = static_cast<int>(endPosition(*first));
        while (++first != last)
            furthestEnd = std::max(furthestEnd, static_cast<int>(endPosition(*first)));
        return furthestEnd;
    }

    bool updateRightEdge(Clip::ClipCommonProperties &properties, int requestedClipLength,
                         bool lengthResizable, int contentLength);
}

#endif // CLIPRESIZEUTILS_H
