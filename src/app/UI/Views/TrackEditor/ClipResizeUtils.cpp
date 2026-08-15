#include "ClipResizeUtils.h"

#include <algorithm>

namespace ClipResizeUtils {
    bool updateLeftEdge(Clip::ClipCommonProperties &properties, const int requestedVisibleStart) {
        const auto visibleEnd = properties.start + properties.clipStart + properties.clipLen;
        if (requestedVisibleStart >= visibleEnd)
            return false;

        properties.clipStart = std::max(0, requestedVisibleStart - properties.start);
        properties.clipLen = visibleEnd - (properties.start + properties.clipStart);
        return true;
    }

    bool updateRightEdge(Clip::ClipCommonProperties &properties, const int requestedClipLength,
                         const bool lengthResizable, const int contentLength) {
        if (requestedClipLength <= 0)
            return false;

        if (lengthResizable) {
            properties.length = std::max(properties.clipStart + requestedClipLength, contentLength);
            properties.clipLen = requestedClipLength;
        } else {
            properties.clipLen = std::min(requestedClipLength,
                                          std::max(0, properties.length - properties.clipStart));
        }
        return true;
    }
}
