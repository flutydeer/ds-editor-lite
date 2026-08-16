#include "ClipResizeUtils.h"

#include <algorithm>

namespace ClipResizeUtils {
    bool updateLeftEdge(Clip::ClipCommonProperties &properties, const int requestedVisibleStart,
                        const int minimumVisibleLength) {
        const auto visibleEnd = properties.start + properties.clipStart + properties.clipLen;
        const auto minimumLength = std::max(1, minimumVisibleLength);
        const auto visibleStart = std::min(requestedVisibleStart, visibleEnd - minimumLength);

        properties.clipStart = std::max(0, visibleStart - properties.start);
        properties.clipLen = visibleEnd - (properties.start + properties.clipStart);
        return true;
    }

    bool updateRightEdge(Clip::ClipCommonProperties &properties, const int requestedClipLength,
                         const int minimumVisibleLength, const bool lengthResizable,
                         const int contentLength) {
        const auto clipLength = std::max(requestedClipLength, std::max(1, minimumVisibleLength));

        if (lengthResizable) {
            properties.length = std::max(properties.clipStart + clipLength, contentLength);
            properties.clipLen = clipLength;
        } else {
            properties.clipLen = std::min(clipLength,
                                          std::max(0, properties.length - properties.clipStart));
        }
        return properties.clipLen > 0;
    }
}
