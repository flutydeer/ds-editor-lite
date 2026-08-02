#ifndef PITCHDISPLAYSTRATEGY_H
#define PITCHDISPLAYSTRATEGY_H

#include <QList>

class DrawCurve;

namespace AnchorEditor {
    struct AnchorOverlayState;
}

enum class PitchDisplayMode {
    Final,
    Draw,
    Anchor,
};

struct PitchDisplayInterval {
    double startTick = 0;
    double endTick = 0;
};

namespace PitchDisplayStrategy {
    [[nodiscard]] QList<PitchDisplayInterval> drawCurveCoverage(const QList<DrawCurve *> &curves);
    [[nodiscard]] QList<PitchDisplayInterval>
        anchorCoverage(const AnchorEditor::AnchorOverlayState &state);
    [[nodiscard]] QList<PitchDisplayInterval>
        combineCoverage(const QList<PitchDisplayInterval> &first,
                        const QList<PitchDisplayInterval> &second);
}

#endif // PITCHDISPLAYSTRATEGY_H
