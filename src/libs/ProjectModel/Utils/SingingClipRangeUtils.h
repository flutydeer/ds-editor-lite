#ifndef SINGINGCLIPRANGEUTILS_H
#define SINGINGCLIPRANGEUTILS_H

#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/Utils/ClipResizeUtils.h>

namespace SingingClipRangeUtils {
    // 剪辑右侧为音符编辑保留的固定安全余量（tick）。
    constexpr int tailPaddingTicks = 120;

    inline int paddedContentEnd(const int contentEnd) {
        return contentEnd + tailPaddingTicks;
    }

    inline bool extendRightToFit(Clip::ClipCommonProperties &properties, const int contentEnd) {
        const auto requiredEnd = paddedContentEnd(contentEnd);
        if (requiredEnd <= properties.clipStart + properties.clipLen)
            return false;

        return ClipResizeUtils::updateRightEdge(
            properties, requiredEnd - properties.clipStart, 1, true,
            std::max(properties.length, requiredEnd));
    }
}

#endif // SINGINGCLIPRANGEUTILS_H
