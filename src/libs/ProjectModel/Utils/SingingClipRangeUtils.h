#ifndef SINGINGCLIPRANGEUTILS_H
#define SINGINGCLIPRANGEUTILS_H

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/SingingClipSlicer/SingingClipSlicerGlobal.h>
#include <lite/ProjectModel/Utils/ClipResizeUtils.h>

#include <algorithm>
#include <cmath>

namespace SingingClipRangeUtils {
    inline int paddedContentEnd(const Clip::ClipCommonProperties &properties,
                                const Timeline &timeline, const int contentEnd) {
        const auto globalContentEnd = properties.start + contentEnd;
        const auto paddedGlobalEnd = timeline.msToTick(
            timeline.tickToMs(globalContentEnd) + SingingClipSlicerGlobal::tailPaddingLengthMax);
        return std::max(contentEnd,
                        static_cast<int>(std::ceil(paddedGlobalEnd - properties.start)));
    }

    inline bool extendRightToFit(Clip::ClipCommonProperties &properties,
                                 const Timeline &timeline, const int contentEnd) {
        const auto requiredEnd = paddedContentEnd(properties, timeline, contentEnd);
        if (requiredEnd <= properties.clipStart + properties.clipLen)
            return false;

        return ClipResizeUtils::updateRightEdge(
            properties, requiredEnd - properties.clipStart, 1, true,
            std::max(properties.length, requiredEnd));
    }
}

#endif // SINGINGCLIPRANGEUTILS_H
