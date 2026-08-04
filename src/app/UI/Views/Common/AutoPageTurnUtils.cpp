#include "AutoPageTurnUtils.h"

#include <lite/MusicBase/Timeline.h>

#include <cmath>

namespace AutoPageTurnUtils {

    bool isPageDurationAvailable(const Timeline &timeline, const double startTick,
                                 const double endTick) {
        if (!std::isfinite(startTick) || !std::isfinite(endTick) || endTick <= startTick)
            return false;

        const auto startMs = timeline.tickToMs(startTick);
        const auto endMs = timeline.tickToMs(endTick);
        return std::isfinite(startMs) && std::isfinite(endMs) &&
               endMs - startMs >= minimumPageDurationMs;
    }

} // namespace AutoPageTurnUtils
