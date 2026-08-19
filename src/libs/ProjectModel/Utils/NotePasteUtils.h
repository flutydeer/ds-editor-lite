#ifndef NOTEPASTEUTILS_H
#define NOTEPASTEUTILS_H

#include <lite/ProjectModel/Utils/ClipResizeUtils.h>

#include <algorithm>

namespace NotePasteUtils {
    struct SourceBounds {
        int start = 0;
        int end = 0;
    };

    struct Plan {
        int localAnchor = 0;
        int offset = 0;
        int pastedEnd = 0;
    };

    inline int resolveLocalAnchor(const Clip::ClipCommonProperties &target,
                                  const int requestedGlobalTick,
                                  const int snappedGlobalTick) {
        const auto visibleStart = target.start + target.clipStart;
        const auto globalAnchor = requestedGlobalTick <= visibleStart
                                      ? visibleStart
                                      : std::max(snappedGlobalTick, visibleStart);
        return globalAnchor - target.start;
    }

    inline Plan plan(const Clip::ClipCommonProperties &target, const int requestedGlobalTick,
                     const int snappedGlobalTick, const SourceBounds source) {
        const auto localAnchor =
            resolveLocalAnchor(target, requestedGlobalTick, snappedGlobalTick);
        const auto offset = localAnchor - source.start;
        return {localAnchor, offset, source.end + offset};
    }

    inline bool extendClipToFit(Clip::ClipCommonProperties &target, const int pastedEnd) {
        if (pastedEnd <= target.clipStart + target.clipLen)
            return false;
        return ClipResizeUtils::updateRightEdge(target, pastedEnd - target.clipStart, 1, true,
                                                std::max(target.length, pastedEnd));
    }
}

#endif // NOTEPASTEUTILS_H
