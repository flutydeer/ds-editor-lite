#ifndef PITCHDISPLAYSTRATEGY_H
#define PITCHDISPLAYSTRATEGY_H

#include "Interface/EditorViewState.h"

#include <QList>

class AnchorCurve;
class AnchorNode;
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

    bool operator==(const PitchDisplayInterval &) const = default;
};

enum class PitchDisplayCurveSource {
    Original,
    Edited,
    Merged,
};

enum class PitchDisplayColorRole {
    Original,
    Edited,
};

struct PitchDisplayLayer {
    PitchDisplayCurveSource curveSource = PitchDisplayCurveSource::Original;
    PitchDisplayColorRole colorRole = PitchDisplayColorRole::Original;
    int maximumAlpha = 255;
    QList<PitchDisplayInterval> hiddenCoverage;
    QList<PitchDisplayInterval> dashedCoverage;
};

struct AnchorDisplayOpacity {
    int nodeMaximumAlpha = 255;
    int curveMaximumAlpha = 255;
};

namespace PitchDisplayStrategy {
    class MergedCurveCache {
    public:
        MergedCurveCache() = default;
        ~MergedCurveCache();

        MergedCurveCache(const MergedCurveCache &) = delete;
        MergedCurveCache &operator=(const MergedCurveCache &) = delete;

        [[nodiscard]] const QList<DrawCurve *> &
            mergedCurves(const QList<DrawCurve *> &originalCurves,
                         const QList<DrawCurve *> &editedCurves);
        void invalidate();

    private:
        QList<DrawCurve *> m_curves;
        bool m_valid = false;
    };

    [[nodiscard]] PitchDisplayMode
        displayModeForEditMode(EditorViewGlobal::PianoRollEditMode editMode);
    [[nodiscard]] QList<PitchDisplayLayer>
        displayLayers(PitchDisplayMode mode, const QList<DrawCurve *> &editedCurves,
                      const QList<PitchDisplayInterval> &anchorCoverage);
    [[nodiscard]] AnchorDisplayOpacity anchorOpacity(PitchDisplayMode mode);
    [[nodiscard]] int anchorPreviewAlpha();
    [[nodiscard]] int anchorInteractionPreviewAlpha();
    [[nodiscard]] int anchorSelectionFillAlpha();
    [[nodiscard]] int anchorSelectionBorderAlpha();
    [[nodiscard]] QList<PitchDisplayInterval> drawCurveCoverage(const QList<DrawCurve *> &curves);
    [[nodiscard]] QList<AnchorNode *>
        anchorCurveNodes(AnchorCurve *curve, const AnchorEditor::AnchorOverlayState &state);
    [[nodiscard]] QList<PitchDisplayInterval>
        anchorCoverage(const AnchorEditor::AnchorOverlayState &state);
    [[nodiscard]] QList<PitchDisplayInterval>
        combineCoverage(const QList<PitchDisplayInterval> &first,
                        const QList<PitchDisplayInterval> &second);
}

#endif // PITCHDISPLAYSTRATEGY_H
