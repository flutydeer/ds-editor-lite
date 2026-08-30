#include "ITimelinePainter.h"

#include <lite/MusicBase/TimelineSnapUtils.h>

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

    std::vector<int> buildSubdivisionCandidates(int beatTicks, int minSubdivisionTicks) {
        std::vector<int> result;
        if (beatTicks <= 1 || minSubdivisionTicks >= beatTicks)
            return result;

        for (int step = beatTicks / 2; step >= minSubdivisionTicks; step /= 2) {
            result.push_back(step);
            if (step == 1)
                break;
        }

        // The minimum step may not divide the beat evenly (e.g. quarter-note
        // triplet = 320 ticks vs a 480-tick beat). It still needs to be drawn so
        // the triplet grid is visible; the binary halving loop above can never
        // produce such a step.
        if (minSubdivisionTicks < beatTicks &&
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

    // Bar thinning levels expressed in whole bars (hop = draw every n-th bar).
    // Measures are not equally wide across signature changes, so thinning is
    // keyed on the measure number instead of a global tick modulo.
    struct BarLevel {
        int hop = 1;
        int level = 0;
        double opacity = 1.0;
    };

    std::vector<BarLevel> buildBarLevels(double barSpacing, int minimumSpacing) {
        int baseHop = 1;
        double spacing = barSpacing;
        while (spacing < minimumSpacing && spacing > 0) {
            baseHop *= 2;
            spacing *= 2;
        }

        std::vector<BarLevel> levels;
        int level = 0;
        for (int hop = baseHop; hop >= 1; hop /= 2) {
            const double opacity =
                hop == baseHop ? 1.0 : spacingVisibility(barSpacing * hop, minimumSpacing);
            if (opacity > kVisibilityEpsilon)
                levels.push_back({hop, level, opacity});
            ++level;
            if (hop == 1)
                break;
        }
        return levels;
    }

} // namespace

int ITimelinePainter::logicalGridStepForScale(const double ticksPerPixel, const int atTick) const {
    const int minSubdivisionTicks = TimelineSnapUtils::quantizeToTicks(m_quantize);
    if (ticksPerPixel <= 0)
        return minSubdivisionTicks;

    // Beat and bar lengths follow the signature of the measure at `atTick`.
    const auto signature =
        m_timeline.timeSignatureAt(m_timeline.tickToTime(std::max(0, atTick)).measure);
    const int beatTicks = signature.ticksPerBeat();
    const int barTicks = signature.ticksPerBar();

    const auto subdivisionCandidates = buildSubdivisionCandidates(beatTicks, minSubdivisionTicks);
    for (auto it = subdivisionCandidates.rbegin(); it != subdivisionCandidates.rend(); ++it) {
        if (spacingVisibility(*it / ticksPerPixel, m_minimumSpacing) >= kSnapVisibilityThreshold)
            return *it;
    }

    if (spacingVisibility(beatTicks / ticksPerPixel, m_minimumSpacing) >= kSnapVisibilityThreshold)
        return beatTicks;

    const auto barSpacing = barTicks / ticksPerPixel;
    const auto barLevels = buildBarLevels(barSpacing, m_minimumSpacing);
    for (auto it = barLevels.rbegin(); it != barLevels.rend(); ++it) {
        if (it->opacity >= kSnapVisibilityThreshold)
            return barTicks * it->hop;
    }

    if (!barLevels.empty())
        return barTicks * barLevels.front().hop;

    return barTicks;
}

void ITimelinePainter::drawTimeline(QPainter *painter, double startTick, double endTick,
                                    double rectWidth) {
    if (rectWidth <= 0 || endTick <= startTick)
        return;

    const auto ticksPerPixel = (endTick - startTick) / rectWidth;
    if (ticksPerPixel <= 0)
        return;

    const int minSubdivisionTicks = TimelineSnapUtils::quantizeToTicks(m_quantize);
    const int firstVisibleTick = std::max(0, static_cast<int>(std::floor(startTick)));

    int startBar = m_timeline.tickToTime(firstVisibleTick).measure;
    {
        // Back off far enough that a thinned-out bar just left of the view is
        // still painted; its number label can poke into the view.
        const double firstBarSpacing =
            m_timeline.timeSignatureAt(startBar).ticksPerBar() / ticksPerPixel;
        const auto firstBarLevels = buildBarLevels(firstBarSpacing, m_minimumSpacing);
        if (!firstBarLevels.empty())
            startBar = std::max(0, startBar - firstBarLevels.front().hop);
    }

    // Per-signature-segment caches: recomputed only when the measure length
    // or beat length changes while walking the bars.
    int cachedBarTicks = -1;
    int cachedBeatTicks = -1;
    std::vector<BarLevel> barLevels;
    std::vector<int> subdivisionCandidates;
    std::vector<StepLevel> subdivisionLevels;
    double beatOpacity = 0.0;

    constexpr int maxBars = 200000; // safety bound against degenerate input
    int barsWalked = 0;
    for (int bar = startBar; barsWalked < maxBars; bar++, barsWalked++) {
        const int barStartTick = m_timeline.barToTick(bar);
        if (barStartTick > endTick)
            break;
        const auto signature = m_timeline.timeSignatureAt(bar);
        const int beatTicks = signature.ticksPerBeat();
        const int barTicks = signature.ticksPerBar();
        if (barTicks != cachedBarTicks || beatTicks != cachedBeatTicks) {
            cachedBarTicks = barTicks;
            cachedBeatTicks = beatTicks;
            barLevels = buildBarLevels(barTicks / ticksPerPixel, m_minimumSpacing);
            beatOpacity = spacingVisibility(beatTicks / ticksPerPixel, m_minimumSpacing);
            subdivisionCandidates = buildSubdivisionCandidates(beatTicks, minSubdivisionTicks);
            subdivisionLevels =
                buildSubdivisionLevels(subdivisionCandidates, ticksPerPixel, m_minimumSpacing);
        }

        for (const auto &line : barLevels) {
            if (bar % line.hop != 0)
                continue;
            drawWithOpacity(painter, line.opacity,
                            [&] { drawBar(painter, barStartTick, bar + 1); });
            break;
        }

        // Bars entirely left of the view only contribute their (clipped) bar
        // line; their beats and subdivisions can never become visible.
        if (barStartTick + barTicks <= firstVisibleTick)
            continue;

        // Triplet grids: when the subdivision step does not divide the beat
        // evenly (e.g. quarter-note triplet 320 vs a 480-tick beat), the
        // independent beat lines would land between triplet lines and break
        // the equal spacing. Skip them so the grid is purely the triplet
        // step. For binary steps (or steps wider than the beat, where beat
        // lines are the only grid), beat lines stay.
        const bool tripletSkipsBeatLines =
            minSubdivisionTicks < beatTicks && beatTicks % minSubdivisionTicks != 0;
        if (!tripletSkipsBeatLines && beatOpacity > kVisibilityEpsilon) {
            for (int beat = 1; beat < signature.numerator; beat++) {
                const int tick = barStartTick + beat * beatTicks;
                if (tick > endTick)
                    break;
                drawWithOpacity(painter, beatOpacity,
                                [&] { drawBeat(painter, tick, bar + 1, beat + 1); });
            }
        }

        const int subdivisionLevelCount = static_cast<int>(subdivisionCandidates.size());
        if (!subdivisionLevels.empty() && subdivisionLevelCount > 0) {
            const int drawStep = subdivisionLevels.back().step;
            for (int offset = drawStep; offset < barTicks; offset += drawStep) {
                // Beat-line positions are normally drawn by drawBeat; with a
                // triplet grid the beat lines are skipped, so the subdivision
                // pass must cover those positions itself to keep equal spacing.
                if (offset % beatTicks == 0 && !tripletSkipsBeatLines)
                    continue;
                const int tick = barStartTick + offset;
                if (tick > endTick)
                    break;
                const int index = levelIndexForTick(offset, subdivisionLevels);
                if (index == -1)
                    continue;
                const auto &line = subdivisionLevels[index];
                drawWithOpacity(painter, line.opacity, [&] {
                    drawSubdivision(painter, tick, line.level, subdivisionLevelCount);
                });
            }
        }
    }
}

int ITimelinePainter::pixelsPerQuarterNote() const {
    return m_pixelsPerQuarterNote;
}

void ITimelinePainter::setPixelsPerQuarterNote(int px) {
    m_pixelsPerQuarterNote = px;
}

void ITimelinePainter::setTimeline(const Timeline &timeline) {
    m_timeline = timeline;
}

void ITimelinePainter::setQuantize(int quantize) {
    m_quantize = std::max(1, quantize);
}

const Timeline &ITimelinePainter::timeline() const {
    return m_timeline;
}
