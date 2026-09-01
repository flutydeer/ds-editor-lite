#include "UI/Views/ClipEditor/CurveTransform/CurveTransformSession.h"
#include "Modules/Inference/Utils/BasePitchCurve.h"

#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/ParamProperties.h>
#include <lite/ProjectModel/AppModel/Params.h>

#include <QCoreApplication>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    bool expectNear(const double actual, const double expected, const double tolerance,
                    const char *message) {
        return expect(std::abs(actual - expected) <= tolerance, message);
    }

    DrawCurve curve(const int startTick, const QList<int> &values) {
        DrawCurve result;
        result.setLocalStart(startTick);
        result.setValues(values);
        return result;
    }

    int valueAt(const QList<DrawCurve *> &curves, const int tick) {
        for (const auto *item : curves) {
            if (item->localStart() <= tick && tick < item->localEndTick())
                return item->values().at((tick - item->localStart()) / item->step);
        }
        return -999999;
    }

    bool testMappings() {
        bool ok = true;
        DecibelParamProperties decibel;
        TensionParamProperties tension;
        MouthOpeningParamProperties mouth;

        ok &= expectNear(decibel.valueToNormalized(-96000), 0.0, 1e-12, "decibel lower endpoint");
        ok &= expectNear(decibel.valueToNormalized(0), 1.0, 1e-12, "decibel upper endpoint");
        ok &= expectNear(decibel.valueFromNormalizedDouble(0.0), -96000.0, 1e-9,
                         "decibel inverse lower endpoint");
        ok &= expectNear(tension.valueToNormalized(-10000), 0.0, 1e-12, "tension lower endpoint");
        ok &= expectNear(tension.valueToNormalized(0), 0.5, 1e-12, "tension center");
        ok &= expectNear(tension.valueFromNormalizedDouble(0.0), -10000.0, 1e-9,
                         "tension zero percent is minus ten");
        ok &= expectNear(mouth.valueToNormalized(250), 0.25, 1e-12, "mouth identity mapping");

        double previousDb = -std::numeric_limits<double>::infinity();
        double previousTension = -std::numeric_limits<double>::infinity();
        for (int value = 0; value <= 1000; ++value) {
            const auto normalized = value / 1000.0;
            const auto db = decibel.valueFromNormalizedDouble(normalized);
            const auto tensionValue = tension.valueFromNormalizedDouble(normalized);
            ok &= expect(db >= previousDb, "decibel inverse is monotonic");
            ok &= expect(tensionValue >= previousTension, "tension inverse is monotonic");
            previousDb = db;
            previousTension = tensionValue;
        }
        for (const int db : {-96000, -72000, -48000, -24000, 0}) {
            const auto roundTrip = decibel.valueFromNormalizedDouble(decibel.valueToNormalized(db));
            ok &= expectNear(roundTrip, db, 1e-7, "decibel mapping round trip");
        }
        for (const int value : {-10000, -5000, 0, 5000, 10000}) {
            const auto roundTrip =
                tension.valueFromNormalizedDouble(tension.valueToNormalized(value));
            ok &= expectNear(roundTrip, value, 1e-7, "tension mapping round trip");
        }
        return ok;
    }

    bool testSelectionDirectionAndPartitions() {
        using namespace CurveTransform;
        bool ok = true;
        auto first = curve(20, {100, 110, 120, 130});
        auto second = curve(60, {200, 210, 220, 230});
        const QList<DrawCurve *> originals{&first, &second};

        Session session;
        session.setSource(originals, {}, {});
        session.beginSelection(0);
        ok &= expect(session.finishSelection(100), "forward hole selection succeeds");
        ok &= expect(session.bounds().componentStart == 20 && session.bounds().componentEnd == 40,
                     "forward selection picks first component");

        session.beginSelection(100);
        ok &= expect(session.finishSelection(0), "reverse hole selection succeeds");
        ok &= expect(session.bounds().componentStart == 60 && session.bounds().componentEnd == 80,
                     "reverse selection picks first component in travel direction");

        session.beginSelection(45);
        ok &= expect(session.updateSelection(100), "selection preview finds right component");
        ok &= expect(session.bounds().componentStart == 60, "right preview component");
        ok &= expect(session.updateSelection(0), "direction reversal finds left component");
        ok &= expect(session.bounds().componentStart == 20, "reversal recomputes first component");
        ok &= expect(!session.updateSelection(44), "retreat to empty range hides preview");

        auto continuous = curve(0, QList<int>(20, 500));
        Config partitioned;
        partitioned.partitions = {
            {0,  45},
            {50, 95}
        };
        session.setSource({&continuous}, {}, partitioned);
        session.beginSelection(0);
        ok &= expect(session.finishSelection(95), "partitioned selection succeeds");
        ok &= expect(session.bounds().componentEnd == 50,
                     "pitch partition boundary splits continuous source");
        return ok;
    }

    bool testShouldersAndBoundaries() {
        using namespace CurveTransform;
        bool ok = true;
        auto source = curve(0, QList<int>(161, 500));
        Config config;
        config.tickToMilliseconds = [](const int tick) { return static_cast<double>(tick); };
        Session session;
        session.setSource({&source}, {}, config);
        session.beginSelection(200);
        ok &= expect(session.finishSelection(600), "wide selection succeeds");
        ok &= expect(session.bounds().c == 140 && session.bounds().d == 660,
                     "default shoulders use a fixed 60 ms length");
        ok &= expect(session.setBoundary(Boundary::C, 0),
                     "left shoulder can expand past the default length");
        ok &= expect(session.setBoundary(Boundary::D, 800),
                     "right shoulder can expand past the default length");
        ok &= expect(session.bounds().c == 0 && session.bounds().d == 800,
                     "expanded shoulders clamp to component");
        session.setBoundary(Boundary::A, 900);
        session.setBoundary(Boundary::B, -100);
        ok &= expect(session.bounds().a + 2 * SampleStep == session.bounds().b,
                     "minimum target retains two samples");

        session.setSource({&source}, {}, config);
        session.beginSelection(400);
        ok &= expect(session.finishSelection(420), "narrow selection succeeds");
        ok &= expect(session.bounds().c == 340 && session.bounds().d == 480,
                     "default shoulder length is independent of target width");

        auto shortSource = curve(0, QList<int>(5, 500));
        session.setSource({&shortSource}, {}, config);
        session.beginSelection(5);
        ok &= expect(session.finishSelection(15), "short selection succeeds");
        ok &= expect(session.bounds().c == session.bounds().componentStart &&
                         session.bounds().d == session.bounds().componentEnd,
                     "default shoulders clamp to short component boundaries");

        config.tickToMilliseconds = [](const int tick) { return tick * 2.0; };
        session.setSource({&source}, {}, config);
        session.beginSelection(200);
        ok &= expect(session.finishSelection(600), "tempo-aware shoulder selection succeeds");
        ok &= expect(session.bounds().c == 170 && session.bounds().d == 630,
                     "default shoulder cap is converted through the timeline");
        return ok;
    }

    bool testShapeAndScale() {
        using namespace CurveTransform;
        bool ok = true;
        MouthOpeningParamProperties properties;
        auto source = curve(0, {0, 250, 1000, 1000});

        Session shape;
        Config shapeConfig;
        shapeConfig.kind = Kind::Shape;
        shapeConfig.properties = &properties;
        shape.setSource({&source}, {}, shapeConfig);
        shape.beginSelection(0);
        ok &= expect(shape.finishSelection(15), "shape selection succeeds");
        ok &= expect(shape.beginTransform(), "shape transform starts");
        ok &= expect(!shape.hasEffectiveChange(), "one hundred percent is a no-op");
        shape.updateTransform(100.0);
        auto preview = shape.buildEditedPreview();
        ok &= expect(valueAt(preview, 5) == 500, "zero percent shape produces endpoint line");
        ok &= expect(shape.hasEffectiveChange(), "shape reports rounded change");
        qDeleteAll(preview);

        shape.updateTransform(-100.0);
        preview = shape.buildEditedPreview();
        ok &= expect(valueAt(preview, 5) == 0, "two hundred percent doubles line residual");
        qDeleteAll(preview);

        auto scaleSource = curve(0, {500, 500, 500, 500, 500, 500, 500});
        Session scale;
        Config scaleConfig;
        scaleConfig.kind = Kind::Scale;
        scaleConfig.properties = &properties;
        scale.setSource({&scaleSource}, {}, scaleConfig);
        scale.beginSelection(10);
        ok &= expect(scale.finishSelection(20), "scale selection succeeds");
        scale.setBoundary(Boundary::C, 0);
        scale.setBoundary(Boundary::D, 30);
        ok &= expect(scale.beginTransform(), "scale transform starts");
        scale.updateTransform(100.0);
        preview = scale.buildEditedPreview();
        ok &= expect(valueAt(preview, 0) == 500, "left shoulder outer edge is unchanged");
        ok &= expect(valueAt(preview, 5) == 250, "left shoulder uses smooth midpoint weight");
        ok &= expect(valueAt(preview, 15) == 0, "zero percent scale reaches visual lower bound");
        qDeleteAll(preview);

        scale.updateTransform(-100.0);
        preview = scale.buildEditedPreview();
        ok &= expect(valueAt(preview, 15) == 1000, "scale clamps at visual upper bound");
        qDeleteAll(preview);
        return ok;
    }

    bool testScaleMappingsAndSessionPhases() {
        using namespace CurveTransform;
        bool ok = true;

        DecibelParamProperties decibel;
        auto dbSource = curve(0, {-24000, -24000, -24000});
        Config dbConfig;
        dbConfig.kind = Kind::Scale;
        dbConfig.properties = &decibel;
        Session dbScale;
        dbScale.setSource({&dbSource}, {}, dbConfig);
        dbScale.beginSelection(0);
        ok &= expect(dbScale.phase() == Phase::Selecting, "selection phase begins explicitly");
        ok &= expect(dbScale.finishSelection(10), "decibel scale selection succeeds");
        ok &= expect(dbScale.phase() == Phase::Adjusting,
                     "selection release enters boundary adjustment");
        dbScale.setBoundary(Boundary::C, 0);
        ok &= expect(dbScale.phase() == Phase::Adjusting,
                     "boundary adjustment remains in the second phase");
        ok &= expect(dbScale.beginTransform(), "factor press enters final transform phase");
        ok &= expect(dbScale.phase() == Phase::Transforming,
                     "factor transform phase is irreversible");
        dbScale.updateTransform(1000.0);
        ok &= expectNear(dbScale.factor(), 0.0, 1e-12, "factor clamps at zero percent");
        auto preview = dbScale.buildEditedPreview();
        ok &=
            expect(valueAt(preview, 5) == -96000, "zero percent decibel scale reaches minus 96 dB");
        qDeleteAll(preview);
        dbScale.updateTransform(-1000.0);
        ok &= expectNear(dbScale.factor(), 2.0, 1e-12, "factor clamps at two hundred percent");
        dbScale.cancel();
        ok &= expect(dbScale.phase() == Phase::Idle, "cancellation clears the whole session");

        TensionParamProperties tension;
        auto tensionSource = curve(0, {0, 0, 0});
        Config tensionConfig;
        tensionConfig.kind = Kind::Scale;
        tensionConfig.properties = &tension;
        Session tensionScale;
        tensionScale.setSource({&tensionSource}, {}, tensionConfig);
        tensionScale.beginSelection(0);
        ok &= expect(tensionScale.finishSelection(10), "tension scale selection succeeds");
        ok &= expect(tensionScale.beginTransform(), "tension scale starts");
        tensionScale.updateTransform(100.0);
        preview = tensionScale.buildEditedPreview();
        ok &= expect(valueAt(preview, 5) == -10000,
                     "zero percent tension scale reaches its visual lower bound");
        qDeleteAll(preview);
        return ok;
    }

    bool testOutOfRangeParamSamples() {
        using namespace CurveTransform;
        bool ok = true;

        DecibelParamProperties decibel;
        auto decibelSource = curve(0, {-120000, -48000, 12000, -24000});
        Config shapeConfig;
        shapeConfig.kind = Kind::Shape;
        shapeConfig.properties = &decibel;
        Session shape;
        shape.setSource({&decibelSource}, {}, shapeConfig);
        shape.beginSelection(0);
        ok &= expect(shape.finishSelection(15), "out-of-range decibel selection succeeds");
        auto preview = shape.buildEditedPreview();
        ok &= expect(valueAt(preview, 0) == -120000 && valueAt(preview, 10) == 12000,
                     "one hundred percent preserves out-of-range source samples");
        qDeleteAll(preview);
        ok &= expect(shape.beginTransform(), "out-of-range decibel shape starts");
        ok &= expect(!shape.hasEffectiveChange(),
                     "one hundred percent out-of-range shape remains a no-op");
        shape.updateTransform(100.0);
        preview = shape.buildEditedPreview();
        ok &= expect(valueAt(preview, 0) == decibel.minimum &&
                         valueAt(preview, 10) == decibel.maximum,
                     "shape clamps out-of-range source and endpoint samples before mapping");
        qDeleteAll(preview);

        TensionParamProperties tension;
        auto tensionSource = curve(0, {-12000, 12000, 0});
        Config scaleConfig;
        scaleConfig.kind = Kind::Scale;
        scaleConfig.properties = &tension;
        Session scale;
        scale.setSource({&tensionSource}, {}, scaleConfig);
        scale.beginSelection(0);
        ok &= expect(scale.finishSelection(10), "out-of-range tension selection succeeds");
        ok &= expect(scale.beginTransform(), "out-of-range tension scale starts");
        ok &= expect(!scale.hasEffectiveChange(),
                     "one hundred percent out-of-range scale remains a no-op");
        scale.updateTransform(-100.0);
        preview = scale.buildEditedPreview();
        ok &= expect(valueAt(preview, 0) == tension.minimum &&
                         valueAt(preview, 5) == tension.maximum,
                     "scale clamps out-of-range source samples before mapping");
        qDeleteAll(preview);
        return ok;
    }

    bool testPitchAndEditedOnlySource() {
        using namespace CurveTransform;
        bool ok = true;
        auto edited = curve(0, {6100, 6200, 6300});
        Config config;
        config.kind = Kind::ModulatePitch;
        config.pitchBaselineAtTick = [](int) { return std::optional<double>(6000.0); };
        Session session;
        session.setSource({}, {&edited}, config);
        ok &= expect(session.hasSource(), "edited-only source supports transforms");
        session.beginSelection(0);
        ok &= expect(session.finishSelection(10), "pitch selection succeeds");
        ok &= expect(session.beginTransform(), "pitch transform starts");
        session.updateTransform(100.0);
        auto preview = session.buildEditedPreview();
        ok &= expect(valueAt(preview, 0) == 6000 && valueAt(preview, 5) == 6000 &&
                         valueAt(preview, 10) == 6300,
                     "pitch transform uses the drawing tool's half-open range");
        qDeleteAll(preview);
        session.updateTransform(-100.0);
        preview = session.buildEditedPreview();
        ok &= expect(valueAt(preview, 5) == 6400, "two hundred percent pitch doubles deviation");
        qDeleteAll(preview);

        ok &= expect(ParamInfo::supportsCurveTransform(ParamInfo::Pitch),
                     "pitch supports curve transforms");
        ok &= expect(ParamInfo::supportsCurveTransform(ParamInfo::MouthOpening),
                     "mouth opening supports curve transforms");
        ok &= expect(!ParamInfo::supportsCurveTransform(ParamInfo::Gender),
                     "offset parameter does not support transforms");
        return ok;
    }

    bool testNonSampleStepEditedCurve() {
        using namespace CurveTransform;
        bool ok = true;
        MouthOpeningParamProperties properties;
        auto edited = curve(0, {100, 200, 300, 400, 500});
        edited.step = 10;

        Config config;
        config.kind = Kind::Scale;
        config.properties = &properties;
        Session session;
        session.setSource({}, {&edited}, config);
        session.beginSelection(15);
        ok &= expect(session.finishSelection(25), "non-sample-step selection succeeds");
        ok &= expect(session.beginTransform(), "non-sample-step transform starts");
        session.updateTransform(100.0);
        auto preview = session.buildEditedPreview();
        ok &= expect(std::all_of(preview.cbegin(), preview.cend(),
                                 [](const auto *item) { return item->step == SampleStep; }),
                     "coarse edited segments are normalized before replacement");
        ok &= expect(valueAt(preview, 0) == 100 && valueAt(preview, 5) == 150 &&
                         valueAt(preview, 10) == 200,
                     "normalization preserves the untouched prefix");
        ok &= expect(valueAt(preview, 15) == 0 && valueAt(preview, 20) == 0 &&
                         valueAt(preview, 25) == 350,
                     "transform replaces only the selected samples");
        ok &= expect(valueAt(preview, 30) == 400 && valueAt(preview, 35) == 450 &&
                         valueAt(preview, 40) == 500 && valueAt(preview, 45) == 500,
                     "normalization preserves the untouched suffix");
        qDeleteAll(preview);
        return ok;
    }

    bool testFineEditedSamplesOutsideTransformArePreserved() {
        using namespace CurveTransform;
        bool ok = true;
        MouthOpeningParamProperties properties;
        QList<int> values(30, 100);
        values[3] = 900;
        values[20] = 600;
        values[21] = 610;
        values[22] = 620;
        values[23] = 630;
        values[24] = 640;
        values[27] = 800;
        auto edited = curve(0, values);
        edited.step = 1;

        Config config;
        config.kind = Kind::Scale;
        config.properties = &properties;
        Session session;
        session.setSource({}, {&edited}, config);
        session.beginSelection(10);
        ok &= expect(session.finishSelection(20), "fine-step selection succeeds");
        ok &= expect(session.beginTransform(), "fine-step transform starts");
        session.updateTransform(100.0);
        auto preview = session.buildEditedPreview();
        ok &= expect(valueAt(preview, 3) == 900 && valueAt(preview, 27) == 800,
                     "fine samples outside the transformed range are preserved");
        ok &= expect(valueAt(preview, 10) == 0 && valueAt(preview, 15) == 0,
                     "fine samples inside the transformed range are replaced");
        ok &= expect(valueAt(preview, 20) == 600 && valueAt(preview, 21) == 610 &&
                         valueAt(preview, 22) == 620 && valueAt(preview, 23) == 630 &&
                         valueAt(preview, 24) == 640,
                     "fine samples immediately after the transformed range are preserved");
        ok &= expect(std::any_of(preview.cbegin(), preview.cend(), [](const auto *item) {
                         return item->step == 1 && item->localStart() == 0;
                     }) &&
                         std::any_of(preview.cbegin(), preview.cend(), [](const auto *item) {
                             return item->step == 1 && item->localStart() == 20;
                         }),
                     "half-open replacement retains the untouched fine-resolution suffix");
        qDeleteAll(preview);
        return ok;
    }

    bool testCompleteSampleIntervals() {
        using namespace CurveTransform;
        bool ok = true;
        const auto partialEnd = completeSampleInterval(0, 17);
        ok &= expect(partialEnd && partialEnd->startTick == 0 && partialEnd->endTick == 10,
                     "partial piece end excludes its incomplete sample cell");
        const auto offset = completeSampleInterval(2, 17);
        ok &= expect(offset && offset->startTick == 5 && offset->endTick == 10,
                     "piece interval uses complete cells on the shared lattice");
        const auto aligned = completeSampleInterval(0, 20);
        ok &= expect(aligned && aligned->startTick == 0 && aligned->endTick == 15,
                     "aligned piece end retains its final complete sample cell");
        ok &= expect(!completeSampleInterval(2, 7),
                     "piece without a complete shared-lattice cell is excluded");
        return ok;
    }

    bool testIncompleteFineSampleCellIsExcluded() {
        using namespace CurveTransform;
        bool ok = true;
        MouthOpeningParamProperties properties;
        QList<int> values(17, 500);
        values[15] = 700;
        values[16] = 800;
        auto edited = curve(0, values);
        edited.step = 1;

        Config config;
        config.kind = Kind::Scale;
        config.properties = &properties;
        Session session;
        session.setSource({}, {&edited}, config);
        session.beginSelection(0);
        ok &= expect(session.finishSelection(20), "fine curve selection succeeds");
        ok &= expect(session.bounds().componentEnd == 15,
                     "incomplete final sample cell is excluded from the component");
        ok &= expect(session.beginTransform(), "fine curve transform starts");
        session.updateTransform(100.0);
        auto preview = session.buildEditedPreview();
        ok &= expect(valueAt(preview, 10) == 0, "complete fine sample cells are transformed");
        ok &= expect(valueAt(preview, 15) == 700 && valueAt(preview, 16) == 800,
                     "incomplete fine sample cell remains unchanged");
        ok &= expect(valueAt(preview, 17) == -999999,
                     "transform does not extend beyond the fine curve source");
        qDeleteAll(preview);
        return ok;
    }

    bool testMismatchedSamplePhasesAreAligned() {
        using namespace CurveTransform;
        bool ok = true;
        MouthOpeningParamProperties properties;
        auto original = curve(0, QList<int>(6, 100));
        auto edited = curve(0, {200, 300, 400, 500});
        edited.Curve::setLocalStart(2);

        Config config;
        config.kind = Kind::Scale;
        config.properties = &properties;
        Session session;
        session.setSource({&original}, {&edited}, config);
        session.beginSelection(5);
        ok &= expect(session.finishSelection(15), "mismatched-phase selection succeeds");
        ok &= expect(session.beginTransform(), "mismatched-phase transform starts");
        session.updateTransform(-100.0);
        auto preview = session.buildEditedPreview();
        ok &= expect(valueAt(preview, 5) == 520 && valueAt(preview, 10) == 720,
                     "calculation snapshots share the five-tick lattice");
        ok &= expect(valueAt(preview, 2) == 200 && valueAt(preview, 15) == 460,
                     "off-lattice edited samples outside the target are preserved");
        qDeleteAll(preview);
        return ok;
    }

    bool testBasePitchRestKeys() {
        bool ok = true;
        using InputNote = BasePitchCurve::InputNote;
        const auto filled = BasePitchCurve::fillRestKeys({
            {50, 10, 0.1, true },
            {60, 20, 0.2, false},
            {51, 30, 0.3, true },
            {52, 40, 0.4, true },
            {53, 50, 0.5, true },
            {70, 60, 0.6, false},
            {54, 70, 0.7, true },
        });
        ok &= expect(filled.at(0).key == 60, "leading rest takes nearest right note");
        ok &= expect(filled.at(2).key == 60 && filled.at(3).key == 60 && filled.at(4).key == 70,
                     "odd middle rest run favors the left side");
        ok &= expect(filled.at(6).key == 70, "trailing rest takes nearest left note");

        const auto even = BasePitchCurve::fillRestKeys({
            {60, 0, 0.1, false},
            {10, 0, 0.1, true },
            {11, 0, 0.1, true },
            {70, 0, 0.1, false},
        });
        ok &= expect(even.at(1).key == 60 && even.at(2).key == 70,
                     "even middle rest run splits evenly");
        const auto single = BasePitchCurve::fillRestKeys({
            {60, 0, 0.1, false},
            {10, 0, 0.1, true },
            {70, 0, 0.1, false}
        });
        ok &= expect(single.at(1).key == 60, "single middle rest takes the left note");
        const auto allRest = BasePitchCurve::fillRestKeys({
            {40, 0, 0.1, true},
            {50, 0, 0.2, true},
            {60, 0, 0.3, true}
        });
        ok &= expect(allRest.at(0).key == 40 && allRest.at(1).key == 50 && allRest.at(2).key == 60,
                     "all-rest piece preserves drawn keys");

        BasePitchCurve empty(std::vector<InputNote>{});
        ok &= expect(empty.isEmpty() && empty.GetPitchPoints(0.01).empty() &&
                         empty.SemitoneValueAt(0.0) == 0.0,
                     "empty base pitch curve is safe");
        BasePitchCurve zeroDuration(std::vector<InputNote>{
            {60, 0, 0.0, false}
        });
        ok &= expect(zeroDuration.isEmpty(), "zero-duration base pitch curve is safe");
        BasePitchCurve withCents(std::vector<InputNote>{
            {60, 99, 0.2, false}
        });
        BasePitchCurve withoutCents(std::vector<InputNote>{
            {60, 0, 0.2, false}
        });
        ok &= expectNear(withCents.SemitoneValueAt(0.1), withoutCents.SemitoneValueAt(0.1), 1e-12,
                         "base pitch continues to ignore cents");
        return ok;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    bool ok = true;
    ok &= testMappings();
    ok &= testSelectionDirectionAndPartitions();
    ok &= testCompleteSampleIntervals();
    ok &= testShouldersAndBoundaries();
    ok &= testShapeAndScale();
    ok &= testScaleMappingsAndSessionPhases();
    ok &= testOutOfRangeParamSamples();
    ok &= testPitchAndEditedOnlySource();
    ok &= testNonSampleStepEditedCurve();
    ok &= testFineEditedSamplesOutsideTransformArePreserved();
    ok &= testIncompleteFineSampleCellIsExcluded();
    ok &= testMismatchedSamplePhasesAreAligned();
    ok &= testBasePitchRestKeys();
    if (!ok)
        return 1;
    QTextStream(stdout) << "TestCurveTransform passed" << Qt::endl;
    return 0;
}
