#ifndef PHONEMEHEADLAYOUT_H
#define PHONEMEHEADLAYOUT_H

#include <QList>

class Timeline;

class PhonemeHeadLayout {
public:
    double baseWordLengthMs = 0;
    int minimumFirstOffsetMs = 0;
    double requiredHeadLengthMs = 0;
    double maximumHeadLengthMs = 0;

    [[nodiscard]] static PhonemeHeadLayout calculate(double paddingStartMs,
                                                     double headAvailableLengthMs,
                                                     const QList<int> &firstNoteOffsets = {});

    [[nodiscard]] bool hasValidBounds() const;
    [[nodiscard]] bool isWithinBounds() const;
    [[nodiscard]] int minimumAllowedOffsetMs() const;
    [[nodiscard]] double pieceStartMs(double firstNoteStartMs) const;
    [[nodiscard]] double earliestAllowedStartMs(double firstNoteStartMs) const;
    [[nodiscard]] int pieceStartTick(const Timeline &timeline, int firstNoteGlobalTick) const;
    [[nodiscard]] int earliestAllowedStartTick(const Timeline &timeline,
                                               int firstNoteGlobalTick) const;
};

#endif // PHONEMEHEADLAYOUT_H
