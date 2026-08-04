#ifndef AUTOPAGETURNUTILS_H
#define AUTOPAGETURNUTILS_H

class Timeline;

namespace AutoPageTurnUtils {

    inline constexpr double minimumPageDurationMs = 1000.0;

    [[nodiscard]] bool isPageDurationAvailable(const Timeline &timeline, double startTick,
                                               double endTick);

} // namespace AutoPageTurnUtils

#endif // AUTOPAGETURNUTILS_H
