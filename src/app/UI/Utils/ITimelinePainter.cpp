//
// Created by fluty on 2024/2/3.
//

#include "ITimelinePainter.h"

#include "Utils/TimelineSnapUtils.h"

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr double kFadeInStartRatio = 0.6;
constexpr double kFadeInEndRatio = 1.0;
constexpr double kVisibilityEpsilon = 0.001;
constexpr double kSnapVisibilityThreshold = 0.5;

double clamp01(double value) {
    if (value < 0.0)
        return 0.0;
    if (value > 1.0)
        return 1.0;
    return value;
}

double smoothStep(double t) {
    const auto clamped = clamp01(t);
    return clamped * clamped * (3.0 - 2.0 * clamped);
}

double spacingVisibility(double spacing, double minimumSpacing) {
    if (minimumSpacing <= 0)
        return 1.0;
    const double start = minimumSpacing * kFadeInStartRatio;
    const double end = minimumSpacing * kFadeInEndRatio;
    if (spacing <= start)
        return 0.0;
    if (spacing >= end)
        return 1.0;
    return smoothStep((spacing - start) / (end - start));
}

template <typename DrawFn>
void drawWithOpacity(QPainter *painter, double opacity, DrawFn &&draw) {
    const auto alpha = clamp01(opacity);
    if (alpha <= 0.0)
        return;
    const auto previousOpacity = painter->opacity();
    painter->setOpacity(previousOpacity * alpha);
    draw();
    painter->setOpacity(previousOpacity);
}

int previousGridTick(double tick, int step) {
    if (step <= 0)
        return 0;
    return static_cast<int>(std::floor(tick / step)) * step;
}

std::vector<int> buildSubdivisionCandidates(int beatTicks, int minSubdivisionTicks) {
    std::vector<int> result;
    if (beatTicks <= 1 || minSubdivisionTicks >= beatTicks)
        return result;

    for (int step = beatTicks / 2; step >= minSubdivisionTicks; step /= 2) {
        result.push_back(step);
        if (step == 1)
            break;
    }

    if (beatTicks % minSubdivisionTicks == 0 &&
        std::find(result.begin(), result.end(), minSubdivisionTicks) == result.end()) {
        result.push_back(minSubdivisionTicks);
    }

    std::sort(result.begin(), result.end(), std::greater<>());
    return result;
}

struct StepLevel {
    int step = 0;
    int level = 0;
    double opacity = 1.0;
};

int levelIndexForTick(int tick, const std::vector<StepLevel> &levels) {
    for (int i = 0; i < static_cast<int>(levels.size()); ++i) {
        if (tick % levels[i].step == 0)
            return i;
    }
    return -1;
}

std::vector<StepLevel> buildBarLevels(double barSpacing, int barTicks, int minimumSpacing) {
    int baseHop = 1;
    double spacing = barSpacing;
    while (spacing < minimumSpacing) {
        baseHop *= 2;
        spacing *= 2;
    }

    std::vector<StepLevel> levels;
    int level = 0;
    for (int hop = baseHop; hop >= 1; hop /= 2) {
        const double opacity = hop == baseHop
                                   ? 1.0
                                   : spacingVisibility(barSpacing * hop, minimumSpacing);
        if (opacity > kVisibilityEpsilon)
            levels.push_back({barTicks * hop, level, opacity});
        ++level;
        if (hop == 1)
            break;
    }
    return levels;
}

std::vector<StepLevel> buildSubdivisionLevels(const std::vector<int> &candidates,
                                              double ticksPerPixel, int minimumSpacing) {
    std::vector<StepLevel> levels;
    int level = 0;
    for (const int step : candidates) {
        const double spacing = step / ticksPerPixel;
        const double opacity = spacingVisibility(spacing, minimumSpacing);
        if (opacity > kVisibilityEpsilon)
            levels.push_back({step, level, opacity});
        ++level;
    }
    return levels;
}

} // namespace

int ITimelinePainter::logicalGridStepForScale(double ticksPerPixel) const {
    const int minSubdivisionTicks = TimelineSnapUtils::quantizeToTicks(m_quantize);
    if (ticksPerPixel <= 0)
        return minSubdivisionTicks;

    const int denominator = std::max(1, m_denominator);
    const int beatTicks = TimelineSnapUtils::ticksPerBeat(denominator);
    const int barTicks = std::max(
        beatTicks, TimelineSnapUtils::ticksPerWholeNote() * std::max(1, m_numerator) / denominator);

    const auto subdivisionCandidates = buildSubdivisionCandidates(beatTicks, minSubdivisionTicks);
    for (auto it = subdivisionCandidates.rbegin(); it != subdivisionCandidates.rend(); ++it) {
        if (spacingVisibility(*it / ticksPerPixel, m_minimumSpacing) >= kSnapVisibilityThreshold)
            return *it;
    }

    if (spacingVisibility(beatTicks / ticksPerPixel, m_minimumSpacing) >= kSnapVisibilityThreshold)
        return beatTicks;

    const auto barSpacing = barTicks / ticksPerPixel;
    const auto barLevels = buildBarLevels(barSpacing, barTicks, m_minimumSpacing);
    for (auto it = barLevels.rbegin(); it != barLevels.rend(); ++it) {
        if (it->opacity >= kSnapVisibilityThreshold)
            return it->step;
    }

    if (!barLevels.empty())
        return barLevels.front().step;

    return barTicks;
}

void ITimelinePainter::drawTimeline(QPainter *painter, double startTick, double endTick,
                                    double rectWidth) {
    if (rectWidth <= 0 || endTick <= startTick)
        return;

    const auto ticksPerPixel = (endTick - startTick) / rectWidth;
    if (ticksPerPixel <= 0)
        return;

    const int denominator = std::max(1, m_denominator);
    const int beatTicks = TimelineSnapUtils::ticksPerBeat(denominator);
    const int barTicks = std::max(
        beatTicks, TimelineSnapUtils::ticksPerWholeNote() * std::max(1, m_numerator) / denominator);
    const auto barSpacing = barTicks / ticksPerPixel;
    const auto barLevels = buildBarLevels(barSpacing, barTicks, m_minimumSpacing);

    if (!barLevels.empty()) {
        const int barDrawStep = barLevels.back().step;
        const int firstBarTick = previousGridTick(startTick, barDrawStep);
        for (int tick = firstBarTick; tick <= endTick; tick += barDrawStep) {
            const int index = levelIndexForTick(tick, barLevels);
            if (index == -1)
                continue;
            const auto &line = barLevels[index];
            const auto bar = tick / barTicks + 1;
            drawWithOpacity(painter, line.opacity, [&] { drawBar(painter, tick, bar); });
        }
    }

    const double beatSpacing = beatTicks / ticksPerPixel;
    const double beatOpacity = spacingVisibility(beatSpacing, m_minimumSpacing);
    if (beatOpacity > kVisibilityEpsilon) {
        const int firstBeatTick = previousGridTick(startTick, beatTicks);
        for (int tick = firstBeatTick; tick <= endTick; tick += beatTicks) {
            if (tick % barTicks == 0)
                continue;
            const auto bar = tick / barTicks + 1;
            const auto beat = (tick % barTicks) / beatTicks + 1;
            drawWithOpacity(painter, beatOpacity, [&] { drawBeat(painter, tick, bar, beat); });
        }
    }

    const int minSubdivisionTicks = TimelineSnapUtils::quantizeToTicks(m_quantize);
    const auto subdivisionCandidates = buildSubdivisionCandidates(beatTicks, minSubdivisionTicks);
    const int subdivisionLevelCount = static_cast<int>(subdivisionCandidates.size());
    const auto subdivisionLevels =
        buildSubdivisionLevels(subdivisionCandidates, ticksPerPixel, m_minimumSpacing);
    if (!subdivisionLevels.empty() && subdivisionLevelCount > 0) {
        const int subdivisionDrawStep = subdivisionLevels.back().step;
        const int firstSubdivisionTick = previousGridTick(startTick, subdivisionDrawStep);
        for (int tick = firstSubdivisionTick; tick <= endTick; tick += subdivisionDrawStep) {
            if (tick % barTicks == 0 || tick % beatTicks == 0)
                continue;
            const int index = levelIndexForTick(tick, subdivisionLevels);
            if (index == -1)
                continue;
            const auto &line = subdivisionLevels[index];
            drawWithOpacity(painter, line.opacity, [&] {
                drawSubdivision(painter, tick, line.level, subdivisionLevelCount);
            });
        }
    }
}

int ITimelinePainter::pixelsPerQuarterNote() const {
    return m_pixelsPerQuarterNote;
}

void ITimelinePainter::setPixelsPerQuarterNote(int px) {
    m_pixelsPerQuarterNote = px;
}

void ITimelinePainter::setTimeSignature(int numerator, int denominator) {
    m_numerator = std::max(1, numerator);
    m_denominator = std::max(1, denominator);
}

void ITimelinePainter::setQuantize(int quantize) {
    m_quantize = std::max(1, quantize);
}

int ITimelinePainter::denominator() const {
    return m_denominator;
}
