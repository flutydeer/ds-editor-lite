#include "CurveRenderUtils.h"

#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/Support/MathUtils.h>

#include <cmath>

CurveRenderSamples CurveRenderUtils::sampleCurve(const DrawCurve &curve, const double startTick,
                                                 const double endTick, const double pixelsPerTick,
                                                 const double minimumPointDistance) {
    const auto &values = curve.values();
    if (values.size() < 2 || curve.step <= 0 || pixelsPerTick <= 0.0)
        return {};

    const auto start = curve.localStart();
    const auto startIndex =
        start >= startTick
            ? 0
            : (MathUtils::roundDown(static_cast<int>(startTick), curve.step) - start) / curve.step;
    if (startIndex < 0 || startIndex >= values.size())
        return {};

    CurveRenderSamples result;
    result.pointIndices.reserve(values.size() - startIndex);
    result.pointIndices.append(startIndex);
    result.lastVisitedIndex = startIndex;
    auto lastAcceptedX = (start + startIndex * curve.step) * pixelsPerTick;
    for (auto index = startIndex; index < values.size(); ++index) {
        const auto tick = start + index * curve.step;
        const auto x = tick * pixelsPerTick;
        if (std::abs(x - lastAcceptedX) > minimumPointDistance) {
            result.pointIndices.append(index);
            lastAcceptedX = x;
        }
        result.lastVisitedIndex = index;
        if (tick > endTick)
            break;
    }
    return result;
}
