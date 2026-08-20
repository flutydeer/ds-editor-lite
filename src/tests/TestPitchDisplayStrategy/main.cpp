#include "UI/Views/ClipEditor/AnchorEditor/AnchorEditController.h"
#include "UI/Views/ClipEditor/CurveRenderUtils.h"
#include "UI/Views/ClipEditor/PianoRoll/PitchDisplayStrategy.h"

#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>

#include <QCoreApplication>
#include <QTextStream>

namespace {
    int failures = 0;

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++failures;
    }

    AnchorCurve *makeAnchorCurve(const std::initializer_list<int> ticks) {
        auto *curve = new AnchorCurve;
        for (const auto tick : ticks)
            curve->insertNode(new AnchorNode(tick, 6000));
        return curve;
    }

    void testDisplayModeMapping() {
        using namespace EditorViewGlobal;
        expect(PitchDisplayStrategy::displayModeForEditMode(Select) == PitchDisplayMode::Final,
               "selection mode must use the final pitch presentation");
        expect(PitchDisplayStrategy::displayModeForEditMode(DrawPitch) == PitchDisplayMode::Draw,
               "drawing mode must use the draw pitch presentation");
        expect(PitchDisplayStrategy::displayModeForEditMode(ErasePitch) == PitchDisplayMode::Draw,
               "erase mode must use the draw pitch presentation");
        expect(PitchDisplayStrategy::displayModeForEditMode(BakePitch) == PitchDisplayMode::Draw,
               "bake mode must use the draw pitch presentation");
        expect(PitchDisplayStrategy::displayModeForEditMode(EditPitchAnchor) ==
                   PitchDisplayMode::Anchor,
               "anchor mode must use the anchor pitch presentation");
    }

    void testPitchEditModeClassification() {
        using namespace EditorViewGlobal;
        expect(!isPitchEditMode(Select), "selection mode must not edit pitch");
        expect(!isPitchEditMode(IntervalSelect), "interval selection mode must not edit pitch");
        expect(!isPitchEditMode(DrawNote), "note drawing mode must not edit pitch");
        expect(!isPitchEditMode(EraseNote), "note erase mode must not edit pitch");
        expect(!isPitchEditMode(SplitNote), "note split mode must not edit pitch");
        expect(isPitchEditMode(DrawPitch), "pitch drawing mode must edit pitch");
        expect(isPitchEditMode(EditPitchAnchor), "pitch anchor mode must edit pitch");
        expect(isPitchEditMode(ErasePitch), "pitch erase mode must edit pitch");
        expect(isPitchEditMode(BakePitch), "pitch bake mode must edit pitch");
    }

    void testDisplayLayers() {
        DrawCurve edited;
        edited.setLocalStart(10);
        edited.setValues({6000, 6100, 6200});
        const QList<DrawCurve *> editedCurves{&edited};
        const QList<PitchDisplayInterval> anchorCoverage{
            {15, 30}
        };

        const auto finalLayers = PitchDisplayStrategy::displayLayers(PitchDisplayMode::Final,
                                                                     editedCurves, anchorCoverage);
        expect(finalLayers.size() == 1 &&
                   finalLayers.first().curveSource == PitchDisplayCurveSource::Merged &&
                   finalLayers.first().maximumAlpha == 210 &&
                   finalLayers.first().hiddenCoverage == anchorCoverage,
               "final mode must render one merged layer outside anchor coverage");

        const auto drawLayers = PitchDisplayStrategy::displayLayers(PitchDisplayMode::Draw,
                                                                    editedCurves, anchorCoverage);
        expect(drawLayers.size() == 2 &&
                   drawLayers.first().curveSource == PitchDisplayCurveSource::Original &&
                   drawLayers.last().curveSource == PitchDisplayCurveSource::Edited &&
                   drawLayers.last().maximumAlpha == 230 &&
                   drawLayers.last().dashedCoverage == anchorCoverage,
               "draw mode must share its original and edited layer policy");

        const auto anchorLayers = PitchDisplayStrategy::displayLayers(PitchDisplayMode::Anchor,
                                                                      editedCurves, anchorCoverage);
        expect(anchorLayers.size() == 1 &&
                   anchorLayers.first().curveSource == PitchDisplayCurveSource::Merged &&
                   anchorLayers.first().maximumAlpha == 80 &&
                   anchorLayers.first().dashedCoverage == anchorCoverage,
               "anchor mode must render one muted merged layer");
    }

    void testAnchorCoverage() {
        auto *source = makeAnchorCurve({0, 10});
        auto *target = makeAnchorCurve({20, 30});
        auto *dragged = source->nodes().toList().last();
        dragged->setPos(25);

        AnchorEditor::AnchorOverlayState state;
        state.visibleCurves = {source, target};
        state.dragging = true;
        state.dragNodeInfos = {
            {dragged, source, target, 10, 6000}
        };
        const auto coverage = PitchDisplayStrategy::anchorCoverage(state);
        expect(coverage.size() == 1 && coverage.first().startTick == 20 &&
                   coverage.first().endTick == 30,
               "anchor coverage must use the shared drag presentation state");

        delete source;
        delete target;
    }

    void testCurveSampling() {
        DrawCurve curve;
        curve.setLocalStart(0);
        curve.setValues({0, 1, 2, 3, 4, 5});

        const auto sparse = CurveRenderUtils::sampleCurve(curve, 0, 20, 0.2, 1.0);
        expect(sparse.pointIndices == QVector<int>({0, 2, 4}) && sparse.lastVisitedIndex == 5,
               "sampling must retain the classic strict pixel-distance threshold");

        const auto clipped = CurveRenderUtils::sampleCurve(curve, 12, 20, 1.0, 1.0);
        expect(clipped.pointIndices == QVector<int>({2, 3, 4, 5}) && clipped.lastVisitedIndex == 5,
               "sampling must retain one accepted point beyond the visible end");
    }

    void testMergedCurveCache() {
        DrawCurve original;
        original.setLocalStart(0);
        original.setValues({100, 100, 100});
        DrawCurve edited;
        edited.setLocalStart(0);
        edited.setValues({200, 200, 200});
        const QList<DrawCurve *> originalCurves{&original};
        const QList<DrawCurve *> editedCurves{&edited};

        PitchDisplayStrategy::MergedCurveCache cache;
        const auto &initial = cache.mergedCurves(originalCurves, editedCurves);
        expect(initial.size() == 1 && initial.first()->values().first() == 200,
               "merged curve cache must build the current pitch data");

        edited.setValues({300, 300, 300});
        const auto &cached = cache.mergedCurves(originalCurves, editedCurves);
        expect(cached.first()->values().first() == 200,
               "merged curve cache must reuse data between snapshots");

        cache.invalidate();
        const auto &rebuilt = cache.mergedCurves(originalCurves, editedCurves);
        expect(rebuilt.first()->values().first() == 300,
               "merged curve cache must rebuild after pitch data changes");
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    testDisplayModeMapping();
    testPitchEditModeClassification();
    testDisplayLayers();
    testAnchorCoverage();
    testCurveSampling();
    testMergedCurveCache();
    return failures == 0 ? 0 : 1;
}
