#include "tst_parameters.h"

#include "Controller/Actions/AppModel/Param/ReplaceParamAction.h"
#include "UI/Views/ClipEditor/AnchorEditor/AnchorEditUtils.h"
#include "UI/Views/ClipEditor/DrawCurveEditUtils.h"

#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/Utils/AppModelUtils.h>
#include <lite/Support/MathUtils.h>

#include <QCoreApplication>
#include <QtTest/QTest>
#include "../TestSupport/TestAssertions.h"
#include <QScopeGuard>

#include <algorithm>

namespace {

    using TestSupport::expect;

    DrawCurve *curve(const int start, const QList<int> &values) {
        auto *result = new DrawCurve;
        result->setLocalStart(start);
        result->setValues(values);
        return result;
    }

    struct CurveSnapshot {
        int start = 0;
        int end = 0;
        int step = 0;
        QList<int> values;

        friend bool operator==(const CurveSnapshot &, const CurveSnapshot &) = default;
    };

    QList<CurveSnapshot> snapshot(const QList<DrawCurve *> &curves) {
        QList<CurveSnapshot> result;
        for (const auto *item : curves) {
            if (item)
                result.append(
                    {item->localStart(), item->localEndTick(), item->step, item->values()});
        }
        return result;
    }

    QList<CurveSnapshot> snapshot(const QList<Curve *> &curves) {
        QList<CurveSnapshot> result;
        for (const auto *item : curves) {
            if (item && item->type() == Curve::Draw) {
                const auto *drawCurve = static_cast<const DrawCurve *>(item);
                result.append({drawCurve->localStart(), drawCurve->localEndTick(), drawCurve->step,
                               drawCurve->values()});
            }
        }
        return result;
    }

    bool hasStandardGrid(const QList<CurveSnapshot> &curves) {
        return std::all_of(curves.cbegin(), curves.cend(),
                           [](const CurveSnapshot &item) { return item.step == DrawCurve().step; });
    }

    bool hasSameShape(const QList<CurveSnapshot> &left, const QList<CurveSnapshot> &right) {
        if (left.size() != right.size())
            return false;
        for (qsizetype i = 0; i < left.size(); ++i) {
            if (left.at(i).start != right.at(i).start || left.at(i).end != right.at(i).end ||
                left.at(i).step != right.at(i).step ||
                left.at(i).values.size() != right.at(i).values.size())
                return false;
        }
        return true;
    }

    enum class Tool { Pencil, Eraser, Trace };

    struct StrokeResult {
        bool changed = false;
        QList<CurveSnapshot> preview;
        QList<CurveSnapshot> replacement;
    };

    StrokeResult runStroke(const Tool tool, const QList<QPoint> &points,
                           const DrawCurveList &generated, const DrawCurveList &initial = {}) {
        if (!QTest::qVerify(!points.isEmpty(), "!points.isEmpty()", "a stroke needs a point",
                            __FILE__, __LINE__))
            return {};
        DrawCurveList preview;
        AppModelUtils::copyCurves(initial, preview);
        const auto cleanup = qScopeGuard([&] { qDeleteAll(preview); });
        const auto snapPoint = [](const QPoint &point) {
            return QPoint(MathUtils::round(point.x(), DrawCurve().step), point.y());
        };
        auto previous = snapPoint(points.first());
        auto state = DrawCurveEditUtils::beginStroke(preview, previous);
        DrawCurveEditUtils::GeneratedCurveSnapshot source;
        source.capture(generated);
        bool changed = false;
        for (qsizetype i = 1; i < points.size(); ++i) {
            const auto current = snapPoint(points.at(i));
            if (tool == Tool::Eraser) {
                const auto [start, end] =
                    DrawCurveEditUtils::strokeTickRange(previous.x(), current.x());
                changed |= AppModelUtils::eraseDrawCurveRange(preview, start, end);
            } else {
                const DrawCurveEditUtils::ValueProvider provider =
                    tool == Tool::Trace
                        ? DrawCurveEditUtils::ValueProvider(
                              [&](int tick) { return source.valueAt(tick); })
                        : DrawCurveEditUtils::ValueProvider([previous, current](int tick) {
                              return std::optional<int>(
                                  qRound(MathUtils::linearValueAt(previous, current, tick)));
                          });
                changed |=
                    DrawCurveEditUtils::updateStroke(preview, state, previous, current, provider);
            }
            previous = current;
        }
        const auto replacement =
            changed ? AnchorEditor::replaceDrawCurves({}, preview) : QList<Curve *>{};
        const auto result = StrokeResult{changed, snapshot(preview), snapshot(replacement)};
        qDeleteAll(replacement);
        return result;
    }

    void expectPencilTraceAlignment(const QList<QPoint> &eventPoints,
                                    const DrawCurveList &generated,
                                    const DrawCurveList &initial = {}) {
        const auto pencil = runStroke(Tool::Pencil, eventPoints, generated, initial);
        const auto trace = runStroke(Tool::Trace, eventPoints, generated, initial);

        expect(pencil.changed == trace.changed,
               "pencil and trace must make the same commit decision");
        expect(hasSameShape(pencil.preview, trace.preview),
               "pencil and trace previews must have identical curve shape");
        expect(hasSameShape(pencil.replacement, trace.replacement),
               "pencil and trace commits must have identical curve shape");
        expect(hasStandardGrid(trace.preview) && hasStandardGrid(trace.replacement),
               "new traced curves must stay on the standard five-tick grid");
    }

    void testSingleClickAndOneSampleInterval(const DrawCurveList &generated) {
        const auto click = runStroke(Tool::Trace,
                                     {
                                         {10, 700}
        },
                                     generated);
        expect(!click.changed && click.preview.isEmpty() && click.replacement.isEmpty(),
               "a single trace point must not be committed");
        expectPencilTraceAlignment(
            {
                {10, 700}
        },
            generated);

        const auto oneSample = runStroke(Tool::Trace,
                                         {
                                             {0, 700},
                                             {5, 710}
        },
                                         generated);
        expect(oneSample.changed && oneSample.preview.size() == 1 &&
                   oneSample.preview.first().values.size() == 1 && oneSample.replacement.isEmpty(),
               "a one-sample trace interval must be filtered by the normal pencil commit path");
        expectPencilTraceAlignment(
            {
                {0, 700},
                {5, 710}
        },
            generated);
    }

    void testShortestValidStrokes(const DrawCurveList &generated) {
        const QList<QList<QPoint>> strokes{
            {{0, 700},  {10, 720}},
            {{10, 700}, {5, 720} }
        };
        for (const auto &stroke : strokes) {
            const auto trace = runStroke(Tool::Trace, stroke, generated);
            expect(trace.replacement.size() == 1 && trace.replacement.first().values.size() == 2,
                   "the shortest valid trace stroke must persist exactly like pencil");
            expectPencilTraceAlignment(stroke, generated);
        }
    }

    void testSparseFastStroke(const DrawCurveList &generated) {
        const QList<QPoint> sparseStroke{
            {0,   700},
            {35,  760},
            {100, 820}
        };
        const auto trace = runStroke(Tool::Trace, sparseStroke, generated);
        expect(trace.replacement.size() == 1 && trace.replacement.first().start == 0 &&
                   trace.replacement.first().end == 100 &&
                   trace.replacement.first().values.size() == 20,
               "sparse trace mouse events must be interpolated without gaps");
        expectPencilTraceAlignment(sparseStroke, generated);

        QList<QPoint> denseStroke;
        for (int tick = 0; tick <= 100; tick += 5)
            denseStroke.append({tick, 700 + tick});
        const auto dense = runStroke(Tool::Trace, denseStroke, generated);
        expect(hasSameShape(trace.replacement, dense.replacement),
               "sparse and dense trace events must produce the same five-tick shape");

        DrawCurveList committed;
        for (const auto &item : dense.replacement) {
            auto *restored = curve(item.start, item.values);
            restored->step = item.step;
            committed.append(restored);
        }
        const auto inferenceInput = AppModelUtils::getResultCurve(*generated.first(), committed);
        expect(inferenceInput.step == DrawCurve().step &&
                   inferenceInput.values().size() == generated.first()->values().size(),
               "dense trace commits must merge into the generated curve without QList assertions");
        qDeleteAll(committed);
    }

    void testExistingCurveOverwrite(const DrawCurveList &generated) {
        DrawCurveList initial{curve(0, QList<int>(24, 321))};
        const QList<QPoint> stroke{
            {20, 700},
            {55, 760},
            {85, 820}
        };
        const auto trace = runStroke(Tool::Trace, stroke, generated, initial);
        const auto pencil = runStroke(Tool::Pencil, stroke, generated, initial);

        expect(hasSameShape(pencil.preview, trace.preview) &&
                   hasSameShape(pencil.replacement, trace.replacement),
               "overwriting an existing curve must share pencil range and merge semantics");
        expect(trace.replacement.size() == 1 && trace.replacement.first().start == 0 &&
                   trace.replacement.first().end == 120 && trace.replacement.first().step == 5,
               "trace overwrite must preserve the standard existing curve shape");
        expect(trace.replacement != pencil.replacement,
               "trace and pencil must differ only in the values supplied to the shared path");
        qDeleteAll(initial);
    }

    void testImportedCurveGridAlignment(const DrawCurveList &generated) {
        auto *imported = curve(0, QList<int>(10, 321));
        imported->step = 3;
        DrawCurveList initial{imported};
        const QList<QPoint> stroke{
            {5,  700},
            {10, 720}
        };
        const auto pencil = runStroke(Tool::Pencil, stroke, generated, initial);
        const auto trace = runStroke(Tool::Trace, stroke, generated, initial);

        expect(hasSameShape(pencil.replacement, trace.replacement) &&
                   trace.replacement.size() == 1 &&
                   trace.replacement.first().step == DrawCurve().step,
               "pencil and trace must normalize an imported curve to the standard grid");
        if (trace.replacement.size() == 1) {
            const auto &values = trace.replacement.first().values;
            const auto generatedAt5 = DrawCurveEditUtils::generatedValueAt(generated, 5);
            expect(values.size() == 6 && values.at(0) == 321 && values.at(2) == 321,
                   "editing an imported curve must preserve samples outside the stroke");
            expect(generatedAt5 && values.at(1) == *generatedAt5,
                   "trace samples must use the standard five-tick phase");
        }

        DrawCurveList committed;
        for (const auto &item : trace.replacement) {
            auto *restored = curve(item.start, item.values);
            restored->step = item.step;
            committed.append(restored);
        }
        const auto inferenceInput = AppModelUtils::getResultCurve(*generated.first(), committed);
        expect(inferenceInput.step == generated.first()->step &&
                   inferenceInput.values().size() == generated.first()->values().size(),
               "imported curve edits must merge into inference input without grid corruption");
        qDeleteAll(committed);
        qDeleteAll(initial);

        auto *crossedImported = curve(20, QList<int>(10, 321));
        crossedImported->step = 3;
        DrawCurveList crossedInitial{crossedImported};
        const QList<QPoint> crossingStroke{
            {0,  700},
            {30, 720}
        };
        const auto crossingPencil =
            runStroke(Tool::Pencil, crossingStroke, generated, crossedInitial);
        const auto crossingTrace =
            runStroke(Tool::Trace, crossingStroke, generated, crossedInitial);
        expect(hasSameShape(crossingPencil.replacement, crossingTrace.replacement) &&
                   crossingTrace.replacement.size() == 1 &&
                   crossingTrace.replacement.first().step == DrawCurve().step &&
                   crossingTrace.replacement.first().start == 0 &&
                   crossingTrace.replacement.first().end == 50,
               "crossing an imported grid must use the shared pencil normalization path");
        qDeleteAll(crossedInitial);

        auto *leftImported = curve(0, QList<int>(5, 111));
        leftImported->step = 4;
        auto *rightImported = curve(20, QList<int>(10, 321));
        rightImported->step = 3;
        DrawCurveList adjacentInitial{leftImported, rightImported};
        const QList<QPoint> backwardCrossingStroke{
            {25, 700},
            {10, 720}
        };
        const auto adjacentPencil =
            runStroke(Tool::Pencil, backwardCrossingStroke, generated, adjacentInitial);
        const auto adjacentTrace =
            runStroke(Tool::Trace, backwardCrossingStroke, generated, adjacentInitial);
        expect(hasSameShape(adjacentPencil.replacement, adjacentTrace.replacement) &&
                   adjacentTrace.replacement.size() == 1 &&
                   adjacentTrace.replacement.first().start == 0 &&
                   adjacentTrace.replacement.first().end == 50 &&
                   adjacentTrace.replacement.first().step == DrawCurve().step &&
                   adjacentTrace.replacement.first().values.first() == 111 &&
                   adjacentTrace.replacement.first().values.last() == 321,
               "crossing adjacent legacy grids must preserve untouched prefixes and tails");
        qDeleteAll(adjacentInitial);
    }

    void testEraserRangeRegression(const DrawCurveList &generated) {
        DrawCurveList initial{curve(0, QList<int>(24, 321))};
        const auto erased = runStroke(Tool::Eraser,
                                      {
                                          {10,  0},
                                          {55,  0},
                                          {100, 0}
        },
                                      generated, initial);
        expect(erased.replacement.size() == 2 && erased.replacement.first().start == 0 &&
                   erased.replacement.first().end == 10 && erased.replacement.last().start == 100 &&
                   erased.replacement.last().end == 120,
               "sparse eraser events must continue to cover every adjacent-event interval");
        qDeleteAll(initial);
    }

    void testGeneratedGapsStaySeparate() {
        DrawCurveList generated{curve(0, {100, 110}), curve(20, {200, 210})};
        for (const auto &stroke : {
                 QList<QPoint>{{0, 700},  {30, 720}},
                 QList<QPoint>{{30, 700}, {0, 720} }
        }) {
            const auto traced = runStroke(Tool::Trace, stroke, generated);
            expect(traced.replacement.size() == 2 && hasStandardGrid(traced.replacement) &&
                       traced.replacement.first().start == 0 &&
                       traced.replacement.first().end == 10 &&
                       traced.replacement.last().start == 20 && traced.replacement.last().end == 30,
                   "trace must use the shared pencil path separately across generated gaps");
        }

        const auto gapOnly = runStroke(Tool::Trace,
                                       {
                                           {10, 700},
                                           {20, 720}
        },
                                       generated);
        expect(!gapOnly.changed && gapOnly.preview.isEmpty() && gapOnly.replacement.isEmpty(),
               "a trace stroke entirely inside a generated gap must not commit");

        auto *legacyEdited = curve(10, QList<int>(4, 321));
        legacyEdited->step = 3;
        DrawCurveList legacyInitial{legacyEdited};
        const auto legacyGapOnly = runStroke(Tool::Trace,
                                             {
                                                 {10, 700},
                                                 {20, 720}
        },
                                             generated, legacyInitial);
        expect(!legacyGapOnly.changed && legacyGapOnly.preview == snapshot(legacyInitial),
               "a no-op trace stroke must not normalize or commit untouched legacy curves");
        qDeleteAll(legacyInitial);
        qDeleteAll(generated);
    }

    void testEmptyGeneratedCurveIsNoOp() {
        const DrawCurveList generated;
        const auto traced = runStroke(Tool::Trace,
                                      {
                                          {0,  700},
                                          {40, 720}
        },
                                      generated);
        expect(!traced.changed && traced.preview.isEmpty() && traced.replacement.isEmpty(),
               "tracing without generated curves must be a no-op");
    }

    void testGeneratedSegmentWaitsForNextStroke() {
        DrawCurveList generated;
        DrawCurveEditUtils::GeneratedCurveSnapshot source;
        source.capture(generated);
        generated.append(curve(0, QList<int>(20, 720)));
        QVERIFY(!source.valueAt(30).has_value());
        source.capture(generated);
        QCOMPARE(source.valueAt(30), std::optional<int>(720));
        qDeleteAll(generated);
    }

    void testUndoRedo(const ParamInfo::Name paramName, const DrawCurveList &generated) {
        const auto traced = runStroke(Tool::Trace,
                                      {
                                          {0,   700},
                                          {35,  760},
                                          {100, 820}
        },
                                      generated);
        expect(!traced.replacement.isEmpty(),
               "undo/redo setup must produce a replacement trace curve");

        SingingClip clip;
        auto *oldCurve = curve(0, QList<int>(20, 111));
        clip.params.getParamByName(paramName)->setCurves(Param::Edited, {oldCurve}, &clip);

        QList<Curve *> replacement;
        for (const auto &item : traced.replacement)
            replacement.append(curve(item.start, item.values));
        ReplaceParamAction action(paramName, Param::Edited, replacement, &clip);
        qDeleteAll(replacement);

        action.execute();
        const auto after = snapshot(clip.params.getParamByName(paramName)->curves(Param::Edited));
        expect(after == traced.replacement,
               "executing a trace action must store the traced curves");
        action.undo();
        const auto undone = snapshot(clip.params.getParamByName(paramName)->curves(Param::Edited));
        expect(undone.size() == 1 && undone.first().values == QList<int>(20, 111),
               "undo must restore the pre-trace edited curve");
        action.execute();
        expect(snapshot(clip.params.getParamByName(paramName)->curves(Param::Edited)) == after,
               "redo must restore the same five-tick traced curve");

        delete oldCurve;
    }

}

namespace {
    static void addCurveData() {
        QTest::addColumn<bool>("pitch");
        QTest::newRow("pitch") << true;
        QTest::newRow("parameter") << false;
    }

}

void ParametersTests::curveTraceSingleClickAndOneSampleInterval_data() {
    addCurveData();
}

void ParametersTests::curveTraceSingleClickAndOneSampleInterval() {
    QFETCH(bool, pitch);
    QList<int> values;
    for (int i = 0; i < 32; ++i)
        values.append(pitch ? 6000 + i * 7 : -50000 + i * 125);
    DrawCurveList generated{curve(0, values)};
    const auto cleanup = qScopeGuard([&] { qDeleteAll(generated); });
    testSingleClickAndOneSampleInterval(generated);
}

void ParametersTests::curveTraceShortestValidStrokes_data() {
    addCurveData();
}

void ParametersTests::curveTraceShortestValidStrokes() {
    QFETCH(bool, pitch);
    QList<int> values;
    for (int i = 0; i < 32; ++i)
        values.append(pitch ? 6000 + i * 7 : -50000 + i * 125);
    DrawCurveList generated{curve(0, values)};
    const auto cleanup = qScopeGuard([&] { qDeleteAll(generated); });
    testShortestValidStrokes(generated);
}

void ParametersTests::curveTraceSparseFastStroke_data() {
    addCurveData();
}

void ParametersTests::curveTraceSparseFastStroke() {
    QFETCH(bool, pitch);
    QList<int> values;
    for (int i = 0; i < 32; ++i)
        values.append(pitch ? 6000 + i * 7 : -50000 + i * 125);
    DrawCurveList generated{curve(0, values)};
    const auto cleanup = qScopeGuard([&] { qDeleteAll(generated); });
    testSparseFastStroke(generated);
}

void ParametersTests::curveTraceExistingCurveOverwrite_data() {
    addCurveData();
}

void ParametersTests::curveTraceExistingCurveOverwrite() {
    QFETCH(bool, pitch);
    QList<int> values;
    for (int i = 0; i < 32; ++i)
        values.append(pitch ? 6000 + i * 7 : -50000 + i * 125);
    DrawCurveList generated{curve(0, values)};
    const auto cleanup = qScopeGuard([&] { qDeleteAll(generated); });
    testExistingCurveOverwrite(generated);
}

void ParametersTests::curveTraceImportedCurveGridAlignment_data() {
    addCurveData();
}

void ParametersTests::curveTraceImportedCurveGridAlignment() {
    QFETCH(bool, pitch);
    QList<int> values;
    for (int i = 0; i < 32; ++i)
        values.append(pitch ? 6000 + i * 7 : -50000 + i * 125);
    DrawCurveList generated{curve(0, values)};
    const auto cleanup = qScopeGuard([&] { qDeleteAll(generated); });
    testImportedCurveGridAlignment(generated);
}

void ParametersTests::curveTraceEraserRangeRegression_data() {
    addCurveData();
}

void ParametersTests::curveTraceEraserRangeRegression() {
    QFETCH(bool, pitch);
    QList<int> values;
    for (int i = 0; i < 32; ++i)
        values.append(pitch ? 6000 + i * 7 : -50000 + i * 125);
    DrawCurveList generated{curve(0, values)};
    const auto cleanup = qScopeGuard([&] { qDeleteAll(generated); });
    testEraserRangeRegression(generated);
}

void ParametersTests::curveTraceGeneratedGapsStaySeparate() {
    testGeneratedGapsStaySeparate();
}

void ParametersTests::curveTraceEmptyGeneratedCurveIsNoOp() {
    testEmptyGeneratedCurveIsNoOp();
}

void ParametersTests::curveTraceGeneratedSegmentWaitsForNextStroke() {
    testGeneratedSegmentWaitsForNextStroke();
}

void ParametersTests::curveTraceUndoRedo_data() {
    addCurveData();
}

void ParametersTests::curveTraceUndoRedo() {
    QFETCH(bool, pitch);
    QList<int> values;
    for (int i = 0; i < 32; ++i)
        values.append(pitch ? 6000 + i * 7 : -50000 + i * 125);
    DrawCurveList generated{curve(0, values)};
    const auto cleanup = qScopeGuard([&] { qDeleteAll(generated); });
    testUndoRedo((pitch ? ParamInfo::Pitch : ParamInfo::Breathiness), generated);
}
