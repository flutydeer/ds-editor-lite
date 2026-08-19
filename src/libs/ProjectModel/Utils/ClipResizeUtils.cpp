#include "ClipResizeUtils.h"

#include <algorithm>

namespace ClipResizeUtils {
    bool updateLeftEdge(Clip::ClipCommonProperties &properties, int requestedVisibleStart,
                        const int minimumVisibleLength) {
        const auto visibleEnd = properties.start + properties.clipStart + properties.clipLen;
        const auto minimumLength = std::max(1, minimumVisibleLength);
        // Cap the visible left edge at tick 0: opendspx rejects a negative
        // clip pos (start + clipStart must stay non-negative).
        requestedVisibleStart = std::max(0, requestedVisibleStart);
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
            properties.clipLen =
                std::min(clipLength, std::max(0, properties.length - properties.clipStart));
        }
        return properties.clipLen > 0;
    }
}
