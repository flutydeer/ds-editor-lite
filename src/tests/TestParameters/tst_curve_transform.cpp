#include "tst_parameters.h"

#include "UI/Views/ClipEditor/CurveTransform/CurveTransformSession.h"
#include "Modules/Inference/Utils/BasePitchCurve.h"

#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/ParamProperties.h>
#include <lite/ProjectModel/AppModel/Params.h>

#include <QCoreApplication>
#include <QtTest/QTest>
#include <QScopeGuard>
#include "../TestSupport/TestAssertions.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

    bool expectNear(const double actual, const double expected, const double tolerance,
                    const char *message,
                    const std::source_location location = std::source_location::current()) {
        return TestSupport::expect(std::abs(actual - expected) <= tolerance,
                                   QStringLiteral("%1 (actual %2, expected %3)")
                                       .arg(QString::fromUtf8(message))
                                       .arg(actual, 0, 'g', 17)
                                       .arg(expected, 0, 'g', 17),
                                   location);
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

}

void ParametersTests::curveTransformMappings() {
    DecibelParamProperties decibel;
    TensionParamProperties tension;
    MouthOpeningParamProperties mouth;

    QVERIFY(expectNear(decibel.valueToNormalized(-96000), 0.0, 1e-12, "decibel lower endpoint"));
    QVERIFY(expectNear(decibel.valueToNormalized(0), 1.0, 1e-12, "decibel upper endpoint"));
    QVERIFY(expectNear(decibel.valueFromNormalizedDouble(0.0), -96000.0, 1e-9,
                       "decibel inverse lower endpoint"));
    QVERIFY(expectNear(tension.valueToNormalized(-10000), 0.0, 1e-12, "tension lower endpoint"));
    QVERIFY(expectNear(tension.valueToNormalized(0), 0.5, 1e-12, "tension center"));
    QVERIFY(expectNear(tension.valueFromNormalizedDouble(0.0), -10000.0, 1e-9,
                       "tension zero percent is minus ten"));
    QVERIFY(expectNear(mouth.valueToNormalized(250), 0.25, 1e-12, "mouth identity mapping"));

    double previousDb = -std::numeric_limits<double>::infinity();
    double previousTension = -std::numeric_limits<double>::infinity();
    for (int value = 0; value <= 1000; ++value) {
        const auto normalized = value / 1000.0;
        const auto db = decibel.valueFromNormalizedDouble(normalized);
        const auto tensionValue = tension.valueFromNormalizedDouble(normalized);
        QVERIFY2((db >= previousDb), "decibel inverse is monotonic");
        QVERIFY2((tensionValue >= previousTension), "tension inverse is monotonic");
        previousDb = db;
        previousTension = tensionValue;
    }
    for (const int db : {-96000, -72000, -48000, -24000, 0}) {
        const auto roundTrip = decibel.valueFromNormalizedDouble(decibel.valueToNormalized(db));
        QVERIFY(expectNear(roundTrip, db, 1e-7, "decibel mapping round trip"));
    }
    for (const int value : {-10000, -5000, 0, 5000, 10000}) {
        const auto roundTrip = tension.valueFromNormalizedDouble(tension.valueToNormalized(value));
        QVERIFY(expectNear(roundTrip, value, 1e-7, "tension mapping round trip"));
    }
}

void ParametersTests::curveTransformSelectionDirectionAndPartitions() {
    using namespace CurveTransform;
    auto first = curve(20, {100, 110, 120, 130});
    auto second = curve(60, {200, 210, 220, 230});
    const QList<DrawCurve *> originals{&first, &second};

    Session session;
    session.setSource(originals, {}, {});
    session.beginSelection(0);
    QVERIFY2((session.finishSelection(100)), "forward hole selection succeeds");
    QVERIFY2((session.bounds().componentStart == 20 && session.bounds().componentEnd == 40),
             "forward selection picks first component");

    session.beginSelection(100);
    QVERIFY2((session.finishSelection(0)), "reverse hole selection succeeds");
    QVERIFY2((session.bounds().componentStart == 60 && session.bounds().componentEnd == 80),
             "reverse selection picks first component in travel direction");

    session.beginSelection(45);
    QVERIFY2((session.updateSelection(100)), "selection preview finds right component");
    QCOMPARE((session.bounds().componentStart), (60));
    QVERIFY2((session.updateSelection(0)), "direction reversal finds left component");
    QCOMPARE((session.bounds().componentStart), (20));
    QVERIFY2((!session.updateSelection(44)), "retreat to empty range hides preview");

    auto continuous = curve(0, QList<int>(20, 500));
    Config partitioned;
    partitioned.partitions = {
        {0,  45},
        {50, 95}
    };
    session.setSource({&continuous}, {}, partitioned);
    session.beginSelection(0);
    QVERIFY2((session.finishSelection(95)), "partitioned selection succeeds");
    QCOMPARE((session.bounds().componentEnd), (50));
}

void ParametersTests::curveTransformExplicitRange() {
    using namespace CurveTransform;
    auto source = curve(0, QList<int>(40, 400));
    MouthOpeningParamProperties properties;
    Config config;
    config.kind = Kind::Scale;
    config.properties = &properties;
    config.tickToMilliseconds = [](const int tick) { return double(tick); };
    config.partitions = {
        {0,   95 },
        {100, 195}
    };
    Session session;
    session.setSource({&source}, {}, config);
    QVERIFY2((session.selectRange(80, std::numeric_limits<int>::max()) &&
              session.bounds().a == 80 && session.bounds().b == 100),
             "explicit range clamps to the first touched pitch partition");
    QVERIFY2((session.selectRange(40, 60, 20, 110) && session.bounds().d == 100),
             "explicit shoulder clamps to the selected component");
    QVERIFY2((session.selectRange(40, 60)), "explicit range accepts default shoulders");
    QVERIFY2((session.bounds().c == 0 && session.bounds().d == 100),
             "default shoulders stop at the component boundaries");
    QVERIFY2((session.selectRange(39, 48) && session.bounds().a == 40 && session.bounds().b == 50),
             "minimum selection width is checked after alignment");
    QVERIFY2(
        (session.selectRange(36, 59, 18, 81) && session.beginTransform() && session.setFactor(0.5)),
        "off-grid range and shoulders use the shared GUI alignment");
    auto preview = session.buildEditedPreview();
    const auto cleanupPreview = qScopeGuard([&preview] { qDeleteAll(preview); });
    QVERIFY2((valueAt(preview, 20) == 400 && valueAt(preview, 30) == 300 &&
              valueAt(preview, 40) == 200 && valueAt(preview, 70) == 300),
             "explicit transform applies the shared smooth shoulders");
    qDeleteAll(preview);
    preview.clear();
}

void ParametersTests::curveTransformShouldersAndBoundaries() {
    using namespace CurveTransform;
    auto source = curve(0, QList<int>(161, 500));
    Config config;
    config.tickToMilliseconds = [](const int tick) { return static_cast<double>(tick); };
    Session session;
    session.setSource({&source}, {}, config);
    session.beginSelection(200);
    QVERIFY2((session.finishSelection(600)), "wide selection succeeds");
    QVERIFY2((session.bounds().c == 140 && session.bounds().d == 660),
             "default shoulders use a fixed 60 ms length");
    QVERIFY2((session.beginBoundaryDrag(Boundary::A)), "left target boundary drag starts");
    QVERIFY2((session.updateBoundaryDrag(20)),
             "left target boundary can approach the component start");
    QVERIFY2((session.bounds().a == 20 && session.bounds().c == 0),
             "left shoulder is temporarily shortened at the component start");
    QVERIFY2((session.updateBoundaryDrag(200)),
             "left target boundary can move away from the component start");
    QVERIFY2((session.bounds().a == 200 && session.bounds().c == 140),
             "left shoulder restores its drag-start width after moving back");
    session.endBoundaryDrag();
    QVERIFY2((session.beginBoundaryDrag(Boundary::B)), "right target boundary drag starts");
    QVERIFY2((session.updateBoundaryDrag(800)),
             "right target boundary can approach the component end");
    QVERIFY2((session.bounds().b == 800 && session.bounds().d == 805),
             "right shoulder is temporarily shortened at the component end");
    QVERIFY2((session.updateBoundaryDrag(600)),
             "right target boundary can move away from the component end");
    QVERIFY2((session.bounds().b == 600 && session.bounds().d == 660),
             "right shoulder restores its drag-start width after moving back");
    session.endBoundaryDrag();
    QVERIFY2((session.beginBoundaryDrag(Boundary::C) && session.updateBoundaryDrag(0)),
             "left shoulder can expand past the default length");
    session.endBoundaryDrag();
    QVERIFY2((session.beginBoundaryDrag(Boundary::D) && session.updateBoundaryDrag(800)),
             "right shoulder can expand past the default length");
    session.endBoundaryDrag();
    QVERIFY2((session.bounds().c == 0 && session.bounds().d == 800),
             "expanded shoulders clamp to component");
    session.beginBoundaryDrag(Boundary::A);
    session.updateBoundaryDrag(900);
    session.endBoundaryDrag();
    session.beginBoundaryDrag(Boundary::B);
    session.updateBoundaryDrag(-100);
    session.endBoundaryDrag();
    QCOMPARE((session.bounds().a + 2 * SampleStep), (session.bounds().b));

    session.setSource({&source}, {}, config);
    session.beginSelection(400);
    QVERIFY2((session.finishSelection(420)), "narrow selection succeeds");
    QVERIFY2((session.bounds().c == 340 && session.bounds().d == 480),
             "default shoulder length is independent of target width");

    auto shortSource = curve(0, QList<int>(5, 500));
    session.setSource({&shortSource}, {}, config);
    session.beginSelection(5);
    QVERIFY2((session.finishSelection(15)), "short selection succeeds");
    QVERIFY2((session.bounds().c == session.bounds().componentStart &&
              session.bounds().d == session.bounds().componentEnd),
             "default shoulders clamp to short component boundaries");

    config.tickToMilliseconds = [](const int tick) { return tick * 2.3; };
    session.setSource({&source}, {}, config);
    session.beginSelection(200);
    QVERIFY2((session.finishSelection(600)), "tempo-aware shoulder selection succeeds");
    QVERIFY2((session.bounds().c == 175 && session.bounds().d == 625),
             "default shoulders stay on the grid within the time cap");
}

void ParametersTests::curveTransformShapeAndScale() {
    using namespace CurveTransform;
    MouthOpeningParamProperties properties;
    auto source = curve(0, {0, 250, 1000, 1000});

    Session shape;
    Config shapeConfig;
    shapeConfig.kind = Kind::Shape;
    shapeConfig.properties = &properties;
    shape.setSource({&source}, {}, shapeConfig);
    shape.beginSelection(0);
    QVERIFY2((shape.finishSelection(15)), "shape selection succeeds");
    QVERIFY2((shape.beginTransform()), "shape transform starts");
    QVERIFY2((shape.hasEffectiveChange()),
             "one hundred percent can materialize Original as Edited");
    shape.updateTransform(100.0);
    auto preview = shape.buildEditedPreview();
    const auto cleanupPreview = qScopeGuard([&preview] { qDeleteAll(preview); });
    QCOMPARE((valueAt(preview, 5)), (500));
    QVERIFY2((shape.hasEffectiveChange()), "shape reports rounded change");
    qDeleteAll(preview);
    preview.clear();

    shape.updateTransform(-100.0);
    preview = shape.buildEditedPreview();
    QCOMPARE((valueAt(preview, 5)), (0));
    qDeleteAll(preview);
    preview.clear();

    auto scaleSource = curve(0, {500, 500, 500, 500, 500, 500, 500});
    Session scale;
    Config scaleConfig;
    scaleConfig.kind = Kind::Scale;
    scaleConfig.properties = &properties;
    scale.setSource({&scaleSource}, {}, scaleConfig);
    scale.beginSelection(10);
    QVERIFY2((scale.finishSelection(20)), "scale selection succeeds");
    scale.beginBoundaryDrag(Boundary::C);
    scale.updateBoundaryDrag(0);
    scale.endBoundaryDrag();
    scale.beginBoundaryDrag(Boundary::D);
    scale.updateBoundaryDrag(30);
    scale.endBoundaryDrag();
    QVERIFY2((scale.beginTransform()), "scale transform starts");
    scale.updateTransform(100.0);
    preview = scale.buildEditedPreview();
    QCOMPARE((valueAt(preview, 0)), (500));
    QCOMPARE((valueAt(preview, 5)), (250));
    QCOMPARE((valueAt(preview, 15)), (0));
    qDeleteAll(preview);
    preview.clear();

    scale.updateTransform(-100.0);
    preview = scale.buildEditedPreview();
    QCOMPARE((valueAt(preview, 15)), (1000));
    qDeleteAll(preview);
    preview.clear();

    auto editedSource = curve(0, {250, 750});
    Session unchanged;
    unchanged.setSource({}, {&editedSource}, scaleConfig);
    unchanged.beginSelection(0);
    QVERIFY2((unchanged.finishSelection(10)), "edited-only selection succeeds");
    QVERIFY2((unchanged.beginTransform()), "edited-only transform starts");
    QVERIFY2((!unchanged.hasEffectiveChange()), "identical Edited output does not create a commit");
}

void ParametersTests::curveTransformScaleMappingsAndSessionPhases() {
    using namespace CurveTransform;

    DecibelParamProperties decibel;
    auto dbSource = curve(0, {-24000, -24000, -24000});
    Config dbConfig;
    dbConfig.kind = Kind::Scale;
    dbConfig.properties = &decibel;
    Session dbScale;
    dbScale.setSource({&dbSource}, {}, dbConfig);
    dbScale.beginSelection(0);
    QCOMPARE((dbScale.phase()), (Phase::Selecting));
    QVERIFY2((dbScale.finishSelection(10)), "decibel scale selection succeeds");
    QCOMPARE((dbScale.phase()), (Phase::Adjusting));
    dbScale.beginBoundaryDrag(Boundary::C);
    dbScale.updateBoundaryDrag(0);
    dbScale.endBoundaryDrag();
    QCOMPARE((dbScale.phase()), (Phase::Adjusting));
    QVERIFY2((dbScale.beginTransform()), "factor press enters final transform phase");
    QCOMPARE((dbScale.phase()), (Phase::Transforming));
    dbScale.updateTransform(1000.0);
    QVERIFY(expectNear(dbScale.factor(), 0.0, 1e-12, "factor clamps at zero percent"));
    auto preview = dbScale.buildEditedPreview();
    const auto cleanupPreview = qScopeGuard([&preview] { qDeleteAll(preview); });
    QCOMPARE((valueAt(preview, 5)), (-96000));
    qDeleteAll(preview);
    preview.clear();
    dbScale.updateTransform(-1000.0);
    QVERIFY(expectNear(dbScale.factor(), 2.0, 1e-12, "factor clamps at two hundred percent"));
    dbScale.cancel();
    QCOMPARE((dbScale.phase()), (Phase::Idle));

    TensionParamProperties tension;
    auto tensionSource = curve(0, {0, 0, 0});
    Config tensionConfig;
    tensionConfig.kind = Kind::Scale;
    tensionConfig.properties = &tension;
    Session tensionScale;
    tensionScale.setSource({&tensionSource}, {}, tensionConfig);
    tensionScale.beginSelection(0);
    QVERIFY2((tensionScale.finishSelection(10)), "tension scale selection succeeds");
    QVERIFY2((tensionScale.beginTransform()), "tension scale starts");
    tensionScale.updateTransform(100.0);
    preview = tensionScale.buildEditedPreview();
    QCOMPARE((valueAt(preview, 5)), (-10000));
    qDeleteAll(preview);
    preview.clear();
}

void ParametersTests::curveTransformOutOfRangeParamSamples() {
    using namespace CurveTransform;

    DecibelParamProperties decibel;
    auto decibelSource = curve(0, {-120000, -48000, 12000, -24000});
    Config shapeConfig;
    shapeConfig.kind = Kind::Shape;
    shapeConfig.properties = &decibel;
    Session shape;
    shape.setSource({&decibelSource}, {}, shapeConfig);
    shape.beginSelection(0);
    QVERIFY2((shape.finishSelection(15)), "out-of-range decibel selection succeeds");
    auto preview = shape.buildEditedPreview();
    const auto cleanupPreview = qScopeGuard([&preview] { qDeleteAll(preview); });
    QVERIFY2((valueAt(preview, 0) == decibel.minimum && valueAt(preview, 10) == decibel.maximum),
             "shape clamps out-of-range source and endpoint samples before mapping");
    qDeleteAll(preview);
    preview.clear();
    QVERIFY2((shape.beginTransform()), "out-of-range decibel shape starts");
    QVERIFY2((shape.hasEffectiveChange()), "neutral factor can commit normalization changes");

    TensionParamProperties tension;
    auto tensionSource = curve(0, {-12000, 12000, 0});
    Config scaleConfig;
    scaleConfig.kind = Kind::Scale;
    scaleConfig.properties = &tension;
    Session scale;
    scale.setSource({&tensionSource}, {}, scaleConfig);
    scale.beginSelection(0);
    QVERIFY2((scale.finishSelection(10)), "out-of-range tension selection succeeds");
    preview = scale.buildEditedPreview();
    QVERIFY2((valueAt(preview, 0) == tension.minimum && valueAt(preview, 5) == tension.maximum),
             "scale clamps out-of-range source samples before mapping");
    qDeleteAll(preview);
    preview.clear();
}

void ParametersTests::curveTransformPitchAndEditedOnlySource() {
    using namespace CurveTransform;
    auto edited = curve(0, {6100, 6200, 6300});
    Config config;
    config.kind = Kind::ModulatePitch;
    config.pitchBaselineAtTick = [](int) { return std::optional<double>(6000.0); };
    Session session;
    session.setSource({}, {&edited}, config);
    QVERIFY2((session.hasSource()), "edited-only source supports transforms");
    session.beginSelection(0);
    QVERIFY2((session.finishSelection(10)), "pitch selection succeeds");
    QVERIFY2((session.beginTransform()), "pitch transform starts");
    session.updateTransform(100.0);
    auto preview = session.buildEditedPreview();
    const auto cleanupPreview = qScopeGuard([&preview] { qDeleteAll(preview); });
    QVERIFY2((valueAt(preview, 0) == 6000 && valueAt(preview, 5) == 6000 &&
              valueAt(preview, 10) == 6300),
             "pitch transform uses the drawing tool's half-open range");
    qDeleteAll(preview);
    preview.clear();
    session.updateTransform(-100.0);
    preview = session.buildEditedPreview();
    QCOMPARE((valueAt(preview, 5)), (6400));
    qDeleteAll(preview);
    preview.clear();

    QVERIFY2((ParamInfo::hasOriginalParam(ParamInfo::Pitch)),
             "pitch has an original curve for transforms");
    QVERIFY2((ParamInfo::hasOriginalParam(ParamInfo::MouthOpening)),
             "mouth opening has an original curve for transforms");
    QVERIFY2((!ParamInfo::hasOriginalParam(ParamInfo::Gender)),
             "offset parameter has no original curve to transform");
}

void ParametersTests::curveTransformNonSampleStepEditedCurve() {
    using namespace CurveTransform;
    MouthOpeningParamProperties properties;
    auto edited = curve(0, {100, 200, 300, 400, 500});
    edited.step = 10;

    Config config;
    config.kind = Kind::Scale;
    config.properties = &properties;
    Session session;
    session.setSource({}, {&edited}, config);
    session.beginSelection(15);
    QVERIFY2((session.finishSelection(25)), "non-sample-step selection succeeds");
    QVERIFY2((session.beginTransform()), "non-sample-step transform starts");
    session.updateTransform(100.0);
    auto preview = session.buildEditedPreview();
    const auto cleanupPreview = qScopeGuard([&preview] { qDeleteAll(preview); });
    QVERIFY2((std::all_of(preview.cbegin(), preview.cend(),
                          [](const auto *item) { return item->step == SampleStep; })),
             "coarse edited segments are normalized before replacement");
    QVERIFY2(
        (valueAt(preview, 0) == 100 && valueAt(preview, 5) == 150 && valueAt(preview, 10) == 200),
        "normalization preserves the untouched prefix");
    QVERIFY2(
        (valueAt(preview, 15) == 0 && valueAt(preview, 20) == 0 && valueAt(preview, 25) == 350),
        "transform replaces only the selected samples");
    QVERIFY2((valueAt(preview, 30) == 400 && valueAt(preview, 35) == 450 &&
              valueAt(preview, 40) == 500 && valueAt(preview, 45) == 500),
             "normalization preserves the untouched suffix");
    qDeleteAll(preview);
    preview.clear();
}

void ParametersTests::curveTransformFineEditedSamplesOutsideTransformArePreserved() {
    using namespace CurveTransform;
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
    QVERIFY2((session.finishSelection(20)), "fine-step selection succeeds");
    QVERIFY2((session.beginTransform()), "fine-step transform starts");
    session.updateTransform(100.0);
    auto preview = session.buildEditedPreview();
    const auto cleanupPreview = qScopeGuard([&preview] { qDeleteAll(preview); });
    QVERIFY2((valueAt(preview, 3) == 900 && valueAt(preview, 27) == 800),
             "fine samples outside the transformed range are preserved");
    QVERIFY2((valueAt(preview, 10) == 0 && valueAt(preview, 15) == 0),
             "fine samples inside the transformed range are replaced");
    QVERIFY2((valueAt(preview, 20) == 600 && valueAt(preview, 21) == 610 &&
              valueAt(preview, 22) == 620 && valueAt(preview, 23) == 630 &&
              valueAt(preview, 24) == 640),
             "fine samples immediately after the transformed range are preserved");
    QVERIFY2(
        (std::any_of(preview.cbegin(), preview.cend(),
                     [](const auto *item) { return item->step == 1 && item->localStart() == 0; }) &&
         std::any_of(preview.cbegin(), preview.cend(),
                     [](const auto *item) { return item->step == 1 && item->localStart() == 20; })),
        "half-open replacement retains the untouched fine-resolution suffix");
    qDeleteAll(preview);
    preview.clear();
}

void ParametersTests::curveTransformSingleSampleEditedRemaindersArePreserved() {
    using namespace CurveTransform;
    MouthOpeningParamProperties properties;
    auto edited = curve(0, {100, 200, 300, 400});

    Config config;
    config.kind = Kind::Scale;
    config.properties = &properties;
    Session session;
    session.setSource({}, {&edited}, config);
    session.beginSelection(5);
    QVERIFY2((session.finishSelection(15)), "inner edited selection succeeds");
    QVERIFY2((session.beginTransform()), "inner edited transform starts");
    QVERIFY2((!session.hasEffectiveChange()),
             "neutral transform leaves existing Edited values unchanged");
    session.updateTransform(100.0);
    auto preview = session.buildEditedPreview();
    const auto cleanupPreview = qScopeGuard([&preview] { qDeleteAll(preview); });
    QVERIFY2((std::all_of(preview.cbegin(), preview.cend(),
                          [](const auto *item) { return item->values().size() >= 2; })),
             "transform preview contains no incomplete draw curves");
    QVERIFY2((valueAt(preview, 0) == 100 && valueAt(preview, 5) == 0 && valueAt(preview, 10) == 0 &&
              valueAt(preview, 15) == 400),
             "single-sample edited remainders survive both transform edges");
    qDeleteAll(preview);
    preview.clear();
}

void ParametersTests::curveTransformCompleteSampleIntervals() {
    using namespace CurveTransform;
    const auto partialEnd = completeSampleInterval(0, 17);
    QVERIFY2((partialEnd && partialEnd->startTick == 0 && partialEnd->endTick == 10),
             "partial piece end excludes its incomplete sample cell");
    const auto offset = completeSampleInterval(2, 17);
    QVERIFY2((offset && offset->startTick == 5 && offset->endTick == 10),
             "piece interval uses complete cells on the shared lattice");
    const auto aligned = completeSampleInterval(0, 20);
    QVERIFY2((aligned && aligned->startTick == 0 && aligned->endTick == 15),
             "aligned piece end retains its final complete sample cell");
    QVERIFY2((!completeSampleInterval(2, 7)),
             "piece without a complete shared-lattice cell is excluded");
}

void ParametersTests::curveTransformIncompleteFineSampleCellIsExcluded() {
    using namespace CurveTransform;
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
    QVERIFY2((session.finishSelection(20)), "fine curve selection succeeds");
    QCOMPARE((session.bounds().componentEnd), (15));
    QVERIFY2((session.beginTransform()), "fine curve transform starts");
    session.updateTransform(100.0);
    auto preview = session.buildEditedPreview();
    const auto cleanupPreview = qScopeGuard([&preview] { qDeleteAll(preview); });
    QCOMPARE((valueAt(preview, 10)), (0));
    QVERIFY2((valueAt(preview, 15) == 700 && valueAt(preview, 16) == 800),
             "incomplete fine sample cell remains unchanged");
    QCOMPARE((valueAt(preview, 17)), (-999999));
    qDeleteAll(preview);
    preview.clear();
}

void ParametersTests::curveTransformMismatchedSamplePhasesAreAligned() {
    using namespace CurveTransform;
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
    QVERIFY2((session.finishSelection(15)), "mismatched-phase selection succeeds");
    QVERIFY2((session.beginTransform()), "mismatched-phase transform starts");
    session.updateTransform(-100.0);
    auto preview = session.buildEditedPreview();
    const auto cleanupPreview = qScopeGuard([&preview] { qDeleteAll(preview); });
    QVERIFY2((valueAt(preview, 5) == 520 && valueAt(preview, 10) == 720),
             "calculation snapshots share the five-tick lattice");
    QVERIFY2((valueAt(preview, 2) == 200 && valueAt(preview, 15) == 460),
             "off-lattice edited samples outside the target are preserved");
    qDeleteAll(preview);
    preview.clear();
}

void ParametersTests::curveTransformBasePitchRestKeys() {
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
    QCOMPARE((filled.at(0).key), (60));
    QVERIFY2((filled.at(2).key == 60 && filled.at(3).key == 60 && filled.at(4).key == 70),
             "odd middle rest run favors the left side");
    QCOMPARE((filled.at(6).key), (70));

    const auto even = BasePitchCurve::fillRestKeys({
        {60, 0, 0.1, false},
        {10, 0, 0.1, true },
        {11, 0, 0.1, true },
        {70, 0, 0.1, false},
    });
    QVERIFY2((even.at(1).key == 60 && even.at(2).key == 70), "even middle rest run splits evenly");
    const auto single = BasePitchCurve::fillRestKeys({
        {60, 0, 0.1, false},
        {10, 0, 0.1, true },
        {70, 0, 0.1, false}
    });
    QCOMPARE((single.at(1).key), (60));
    const auto allRest = BasePitchCurve::fillRestKeys({
        {40, 0, 0.1, true},
        {50, 0, 0.2, true},
        {60, 0, 0.3, true}
    });
    QVERIFY2((allRest.at(0).key == 40 && allRest.at(1).key == 50 && allRest.at(2).key == 60),
             "all-rest piece preserves drawn keys");

    BasePitchCurve empty(std::vector<InputNote>{});
    QVERIFY2((empty.isEmpty() && empty.GetPitchPoints(0.01).empty() &&
              empty.SemitoneValueAt(0.0) == 0.0),
             "empty base pitch curve is safe");
    BasePitchCurve zeroDuration(std::vector<InputNote>{
        {60, 0, 0.0, false}
    });
    QVERIFY2((zeroDuration.isEmpty()), "zero-duration base pitch curve is safe");
    BasePitchCurve withCents(std::vector<InputNote>{
        {60, 99, 0.2, false}
    });
    BasePitchCurve withoutCents(std::vector<InputNote>{
        {60, 0, 0.2, false}
    });
    QVERIFY(expectNear(withCents.SemitoneValueAt(0.1), withoutCents.SemitoneValueAt(0.1), 1e-12,
                       "base pitch continues to ignore cents"));
}
