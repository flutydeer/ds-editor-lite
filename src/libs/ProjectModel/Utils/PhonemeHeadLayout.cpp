#include <lite/ProjectModel/Utils/PhonemeHeadLayout.h>

#include <lite/MusicBase/Timeline.h>

#include <algorithm>
#include <cmath>

PhonemeHeadLayout PhonemeHeadLayout::calculate(const double paddingStartMs,
                                               const double headAvailableLengthMs,
                                               const QList<int> &firstNoteOffsets) {
    PhonemeHeadLayout result;
    result.baseWordLengthMs = paddingStartMs;
    result.maximumHeadLengthMs = std::max(paddingStartMs, headAvailableLengthMs);
    if (!firstNoteOffsets.isEmpty())
        result.minimumFirstOffsetMs =
            std::min(0, *std::min_element(firstNoteOffsets.cbegin(), firstNoteOffsets.cend()));
    result.requiredHeadLengthMs =
        std::max(paddingStartMs, -static_cast<double>(result.minimumFirstOffsetMs));
    return result;
}

bool PhonemeHeadLayout::hasValidBounds() const {
    return std::isfinite(baseWordLengthMs) && std::isfinite(requiredHeadLengthMs) &&
           std::isfinite(maximumHeadLengthMs) && baseWordLengthMs >= 0 &&
           maximumHeadLengthMs >= 0;
}

bool PhonemeHeadLayout::isWithinBounds() const {
    if (!hasValidBounds())
        return false;
    return requiredHeadLengthMs <= maximumHeadLengthMs ||
           qFuzzyCompare(requiredHeadLengthMs + 1.0, maximumHeadLengthMs + 1.0);
}

int PhonemeHeadLayout::minimumAllowedOffsetMs() const {
    return qCeil(-maximumHeadLengthMs);
}

double PhonemeHeadLayout::pieceStartMs(const double firstNoteStartMs) const {
    return firstNoteStartMs - requiredHeadLengthMs;
}

double PhonemeHeadLayout::earliestAllowedStartMs(const double firstNoteStartMs) const {
    return firstNoteStartMs - maximumHeadLengthMs;
}

int PhonemeHeadLayout::pieceStartTick(const Timeline &timeline,
                                      const int firstNoteGlobalTick) const {
    const auto firstNoteStartMs = timeline.tickToMs(firstNoteGlobalTick);
    return qCeil(timeline.msToTick(pieceStartMs(firstNoteStartMs)));
}

int PhonemeHeadLayout::earliestAllowedStartTick(const Timeline &timeline,
                                                const int firstNoteGlobalTick) const {
    const auto firstNoteStartMs = timeline.tickToMs(firstNoteGlobalTick);
    return qCeil(timeline.msToTick(earliestAllowedStartMs(firstNoteStartMs)));
}
