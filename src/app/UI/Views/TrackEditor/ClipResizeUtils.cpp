#include "ClipResizeUtils.h"

#include <algorithm>

namespace ClipResizeUtils {
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
