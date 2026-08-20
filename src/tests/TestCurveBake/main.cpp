#include "Controller/Actions/AppModel/Param/ReplaceParamAction.h"
#include "UI/Views/ClipEditor/AnchorEditor/AnchorEditUtils.h"
#include "UI/Views/ClipEditor/DrawCurveEditUtils.h"

#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/Utils/AppModelUtils.h>
#include <lite/Support/MathUtils.h>

#include <QCoreApplication>
#include <QMouseEvent>
#include <QTextStream>

#include <algorithm>

namespace {
    int failures = 0;

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++failures;
    }

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

    enum class Backend { GraphicsView, Rhi };
    enum class Tool { Pencil, Eraser, Bake };

    struct StrokeResult {
        bool commitAttempted = false;
        QList<CurveSnapshot> preview;
        QList<CurveSnapshot> persisted;
    };

    class CurveStrokeEventProbe final : public QObject {
    public:
        CurveStrokeEventProbe(const Backend backend, const Tool tool,
                              const DrawCurveList &generated, const DrawCurveList &initial)
            : m_backend(backend), m_tool(tool), m_generated(generated) {
            AppModelUtils::copyCurves(initial, m_preview);
        }

        ~CurveStrokeEventProbe() override {
            qDeleteAll(m_preview);
            qDeleteAll(m_persisted);
        }

        StrokeResult result() const {
            return {m_commitAttempted, snapshot(m_preview), snapshot(m_persisted)};
        }

    protected:
        bool event(QEvent *event) override {
            if (event->type() == QEvent::MouseButtonPress) {
                const auto *mouseEvent = static_cast<QMouseEvent *>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    m_pressed = true;
                    m_mouseDown = pointAt(*mouseEvent);
                    m_previous = m_mouseDown;
                    m_moved = false;
                    if (m_tool != Tool::Eraser)
                        m_stroke = DrawCurveEditUtils::beginStroke(m_preview, m_mouseDown);
                }
                return true;
            }
            if (event->type() == QEvent::MouseMove) {
                const auto *mouseEvent = static_cast<QMouseEvent *>(event);
                if (m_pressed && mouseEvent->buttons().testFlag(Qt::LeftButton))
                    updateStroke(pointAt(*mouseEvent));
                return true;
            }
            if (event->type() == QEvent::MouseButtonRelease) {
                const auto *mouseEvent = static_cast<QMouseEvent *>(event);
                if (m_pressed && mouseEvent->button() == Qt::LeftButton) {
                    if (m_backend == Backend::Rhi)
                        updateStroke(pointAt(*mouseEvent));
                    if (m_moved)
                        commit();
                    m_pressed = false;
                }
                return true;
            }
            return QObject::event(event);
        }

    private:
        static QPoint pointAt(const QMouseEvent &event) {
            return {MathUtils::round(qRound(event.position().x()), DrawCurve().step),
                    qRound(event.position().y())};
        }

        void updateStroke(const QPoint &current) {
            if (current.x() == m_previous.x())
                return;

            const auto [startTick, endTick] =
                DrawCurveEditUtils::strokeTickRange(m_previous.x(), current.x());
            bool changed = false;
            if (m_tool == Tool::Eraser) {
                changed = AppModelUtils::eraseDrawCurveRange(m_preview, startTick, endTick);
            } else {
                const DrawCurveEditUtils::ValueProvider provider =
                    m_tool == Tool::Bake
                        ? DrawCurveEditUtils::ValueProvider([this](const int tick) {
                              return DrawCurveEditUtils::generatedValueAt(m_generated, tick);
                          })
                        : DrawCurveEditUtils::ValueProvider(
                              [previous = m_previous, current](const int tick) {
                                  return std::optional<int>(
                                      qRound(MathUtils::linearValueAt(previous, current, tick)));
                              });
                changed = DrawCurveEditUtils::updateStroke(m_preview, m_stroke, m_previous, current,
                                                           provider);
            }
            m_previous = current;
            m_moved = m_moved || changed;
        }

        void commit() {
            const auto replacement = AnchorEditor::replaceDrawCurves({}, m_preview);
            for (const auto *item : replacement) {
                if (item->type() == Curve::Draw)
                    m_persisted.append(new DrawCurve(*static_cast<const DrawCurve *>(item)));
            }
            qDeleteAll(replacement);
            m_commitAttempted = true;
        }

        Backend m_backend;
        Tool m_tool;
        const DrawCurveList &m_generated;
        DrawCurveList m_preview;
        DrawCurveList m_persisted;
        DrawCurveEditUtils::StrokeState m_stroke;
        QPoint m_mouseDown;
        QPoint m_previous;
        bool m_pressed = false;
        bool m_moved = false;
        bool m_commitAttempted = false;
    };

    void sendMouseEvent(QObject &target, const QEvent::Type type, const QPoint &point,
                        const Qt::MouseButton button, const Qt::MouseButtons buttons) {
        QMouseEvent event(type, QPointF(point), button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(&target, &event);
    }

    StrokeResult runStroke(const Backend backend, const Tool tool, const QList<QPoint> &eventPoints,
                           const DrawCurveList &generated, const DrawCurveList &initial = {}) {
        expect(!eventPoints.isEmpty(), "a test stroke must contain at least one event");
        if (eventPoints.isEmpty())
            return {};

        CurveStrokeEventProbe probe(backend, tool, generated, initial);
        sendMouseEvent(probe, QEvent::MouseButtonPress, eventPoints.first(), Qt::LeftButton,
                       Qt::LeftButton);
        for (qsizetype i = 1; i < eventPoints.size(); ++i)
            sendMouseEvent(probe, QEvent::MouseMove, eventPoints.at(i), Qt::NoButton,
                           Qt::LeftButton);
        sendMouseEvent(probe, QEvent::MouseButtonRelease, eventPoints.last(), Qt::LeftButton,
                       Qt::NoButton);
        return probe.result();
    }

    void expectPencilBakeAlignment(const Backend backend, const QList<QPoint> &eventPoints,
                                   const DrawCurveList &generated,
                                   const DrawCurveList &initial = {}) {
        const auto pencil = runStroke(backend, Tool::Pencil, eventPoints, generated, initial);
        const auto bake = runStroke(backend, Tool::Bake, eventPoints, generated, initial);

        expect(pencil.commitAttempted == bake.commitAttempted,
               "pencil and bake must make the same commit decision");
        expect(hasSameShape(pencil.preview, bake.preview),
               "pencil and bake previews must have identical curve shape");
        expect(hasSameShape(pencil.persisted, bake.persisted),
               "pencil and bake commits must have identical curve shape");
        expect(hasStandardGrid(bake.preview) && hasStandardGrid(bake.persisted),
               "new baked curves must stay on the standard five-tick grid");
    }

    void testSingleClickAndOneSampleInterval(const Backend backend,
                                             const DrawCurveList &generated) {
        const auto click = runStroke(backend, Tool::Bake,
                                     {
                                         {10, 700}
        },
                                     generated);
        expect(!click.commitAttempted && click.preview.isEmpty() && click.persisted.isEmpty(),
               "a single bake point must not be committed");
        expectPencilBakeAlignment(backend,
                                  {
                                      {10, 700}
        },
                                  generated);

        const auto oneSample = runStroke(backend, Tool::Bake,
                                         {
                                             {0, 700},
                                             {5, 710}
        },
                                         generated);
        expect(oneSample.commitAttempted && oneSample.preview.size() == 1 &&
                   oneSample.preview.first().values.size() == 1 && oneSample.persisted.isEmpty(),
               "a one-sample bake interval must be filtered by the normal pencil commit path");
        expectPencilBakeAlignment(backend,
                                  {
                                      {0, 700},
                                      {5, 710}
        },
                                  generated);
    }

    void testShortestValidStrokes(const Backend backend, const DrawCurveList &generated) {
        const QList<QList<QPoint>> strokes{
            {{0, 700},  {10, 720}},
            {{10, 700}, {5, 720} }
        };
        for (const auto &stroke : strokes) {
            const auto bake = runStroke(backend, Tool::Bake, stroke, generated);
            expect(bake.persisted.size() == 1 && bake.persisted.first().values.size() == 2,
                   "the shortest valid bake stroke must persist exactly like pencil");
            expectPencilBakeAlignment(backend, stroke, generated);
        }
    }

    void testSparseFastStroke(const Backend backend, const DrawCurveList &generated) {
        const QList<QPoint> sparseStroke{
            {0,   700},
            {35,  760},
            {100, 820}
        };
        const auto bake = runStroke(backend, Tool::Bake, sparseStroke, generated);
        expect(bake.persisted.size() == 1 && bake.persisted.first().start == 0 &&
                   bake.persisted.first().end == 100 && bake.persisted.first().values.size() == 20,
               "sparse bake mouse events must be interpolated without gaps");
        expectPencilBakeAlignment(backend, sparseStroke, generated);

        QList<QPoint> denseStroke;
        for (int tick = 0; tick <= 100; tick += 5)
            denseStroke.append({tick, 700 + tick});
        const auto dense = runStroke(backend, Tool::Bake, denseStroke, generated);
        expect(hasSameShape(bake.persisted, dense.persisted),
               "sparse and dense bake events must produce the same five-tick shape");

        DrawCurveList committed;
        for (const auto &item : dense.persisted) {
            auto *restored = curve(item.start, item.values);
            restored->step = item.step;
            committed.append(restored);
        }
        const auto inferenceInput = AppModelUtils::getResultCurve(*generated.first(), committed);
        expect(inferenceInput.step == DrawCurve().step &&
                   inferenceInput.values().size() == generated.first()->values().size(),
               "dense bake commits must merge into the generated curve without QList assertions");
        qDeleteAll(committed);
    }

    void testExistingCurveOverwrite(const Backend backend, const DrawCurveList &generated) {
        DrawCurveList initial{curve(0, QList<int>(24, 321))};
        const QList<QPoint> stroke{
            {20, 700},
            {55, 760},
            {85, 820}
        };
        const auto bake = runStroke(backend, Tool::Bake, stroke, generated, initial);
        const auto pencil = runStroke(backend, Tool::Pencil, stroke, generated, initial);

        expect(hasSameShape(pencil.preview, bake.preview) &&
                   hasSameShape(pencil.persisted, bake.persisted),
               "overwriting an existing curve must share pencil range and merge semantics");
        expect(bake.persisted.size() == 1 && bake.persisted.first().start == 0 &&
                   bake.persisted.first().end == 120 && bake.persisted.first().step == 5,
               "bake overwrite must preserve the standard existing curve shape");
        expect(bake.persisted != pencil.persisted,
               "bake and pencil must differ only in the values supplied to the shared path");
        qDeleteAll(initial);
    }

    void testImportedCurveGridAlignment(const Backend backend, const DrawCurveList &generated) {
        auto *imported = curve(0, QList<int>(10, 321));
        imported->step = 3;
        DrawCurveList initial{imported};
        const QList<QPoint> stroke{
            {5,  700},
            {10, 720}
        };
        const auto pencil = runStroke(backend, Tool::Pencil, stroke, generated, initial);
        const auto bake = runStroke(backend, Tool::Bake, stroke, generated, initial);

        expect(hasSameShape(pencil.persisted, bake.persisted) && bake.persisted.size() == 1 &&
                   bake.persisted.first().step == DrawCurve().step,
               "pencil and bake must normalize an imported curve to the standard grid");
        if (bake.persisted.size() == 1) {
            const auto &values = bake.persisted.first().values;
            const auto generatedAt5 = DrawCurveEditUtils::generatedValueAt(generated, 5);
            expect(values.size() == 6 && values.at(0) == 321 && values.at(2) == 321,
                   "editing an imported curve must preserve samples outside the stroke");
            expect(generatedAt5 && values.at(1) == *generatedAt5,
                   "bake samples must use the standard five-tick phase");
        }

        DrawCurveList committed;
        for (const auto &item : bake.persisted) {
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
            runStroke(backend, Tool::Pencil, crossingStroke, generated, crossedInitial);
        const auto crossingBake =
            runStroke(backend, Tool::Bake, crossingStroke, generated, crossedInitial);
        expect(hasSameShape(crossingPencil.persisted, crossingBake.persisted) &&
                   crossingBake.persisted.size() == 1 &&
                   crossingBake.persisted.first().step == DrawCurve().step &&
                   crossingBake.persisted.first().start == 0 &&
                   crossingBake.persisted.first().end == 50,
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
            runStroke(backend, Tool::Pencil, backwardCrossingStroke, generated, adjacentInitial);
        const auto adjacentBake =
            runStroke(backend, Tool::Bake, backwardCrossingStroke, generated, adjacentInitial);
        expect(hasSameShape(adjacentPencil.persisted, adjacentBake.persisted) &&
                   adjacentBake.persisted.size() == 1 &&
                   adjacentBake.persisted.first().start == 0 &&
                   adjacentBake.persisted.first().end == 50 &&
                   adjacentBake.persisted.first().step == DrawCurve().step &&
                   adjacentBake.persisted.first().values.first() == 111 &&
                   adjacentBake.persisted.first().values.last() == 321,
               "crossing adjacent legacy grids must preserve untouched prefixes and tails");
        qDeleteAll(adjacentInitial);
    }

    void testEraserRangeRegression(const Backend backend, const DrawCurveList &generated) {
        DrawCurveList initial{curve(0, QList<int>(24, 321))};
        const auto erased = runStroke(backend, Tool::Eraser,
                                      {
                                          {10,  0},
                                          {55,  0},
                                          {100, 0}
        },
                                      generated, initial);
        expect(erased.persisted.size() == 2 && erased.persisted.first().start == 0 &&
                   erased.persisted.first().end == 10 && erased.persisted.last().start == 100 &&
                   erased.persisted.last().end == 120,
               "sparse eraser events must continue to cover every adjacent-event interval");
        qDeleteAll(initial);
    }

    void testGeneratedGapsStaySeparate(const Backend backend) {
        DrawCurveList generated{curve(0, {100, 110}), curve(20, {200, 210})};
        for (const auto &stroke : {
                 QList<QPoint>{{0, 700},  {30, 720}},
                 QList<QPoint>{{30, 700}, {0, 720} }
        }) {
            const auto baked = runStroke(backend, Tool::Bake, stroke, generated);
            expect(baked.persisted.size() == 2 && hasStandardGrid(baked.persisted) &&
                       baked.persisted.first().start == 0 && baked.persisted.first().end == 10 &&
                       baked.persisted.last().start == 20 && baked.persisted.last().end == 30,
                   "bake must use the shared pencil path separately across generated gaps");
        }

        const auto gapOnly = runStroke(backend, Tool::Bake,
                                       {
                                           {10, 700},
                                           {20, 720}
        },
                                       generated);
        expect(!gapOnly.commitAttempted && gapOnly.preview.isEmpty() && gapOnly.persisted.isEmpty(),
               "a bake stroke entirely inside a generated gap must not commit");

        auto *legacyEdited = curve(10, QList<int>(4, 321));
        legacyEdited->step = 3;
        DrawCurveList legacyInitial{legacyEdited};
        const auto legacyGapOnly = runStroke(backend, Tool::Bake,
                                             {
                                                 {10, 700},
                                                 {20, 720}
        },
                                             generated, legacyInitial);
        expect(!legacyGapOnly.commitAttempted && legacyGapOnly.preview == snapshot(legacyInitial),
               "a no-op bake stroke must not normalize or commit untouched legacy curves");
        qDeleteAll(legacyInitial);
        qDeleteAll(generated);
    }

    void testUndoRedo(const Backend backend, const ParamInfo::Name paramName,
                      const DrawCurveList &generated) {
        const auto baked = runStroke(backend, Tool::Bake,
                                     {
                                         {0,   700},
                                         {35,  760},
                                         {100, 820}
        },
                                     generated);
        expect(!baked.persisted.isEmpty(), "undo/redo setup must produce a persisted bake curve");

        SingingClip clip;
        auto *oldCurve = curve(0, QList<int>(20, 111));
        clip.params.getParamByName(paramName)->setCurves(Param::Edited, {oldCurve}, &clip);

        QList<Curve *> replacement;
        for (const auto &item : baked.persisted)
            replacement.append(curve(item.start, item.values));
        ReplaceParamAction action(paramName, Param::Edited, replacement, &clip);
        qDeleteAll(replacement);

        action.execute();
        const auto after = snapshot(clip.params.getParamByName(paramName)->curves(Param::Edited));
        expect(after == baked.persisted, "executing a bake action must store the baked curves");
        action.undo();
        const auto undone = snapshot(clip.params.getParamByName(paramName)->curves(Param::Edited));
        expect(undone.size() == 1 && undone.first().values == QList<int>(20, 111),
               "undo must restore the pre-bake edited curve");
        action.execute();
        expect(snapshot(clip.params.getParamByName(paramName)->curves(Param::Edited)) == after,
               "redo must restore the same five-tick baked curve");

        delete oldCurve;
    }

    void runAlignmentSuite(const DrawCurveList &generated) {
        for (const auto backend : {Backend::GraphicsView, Backend::Rhi}) {
            testSingleClickAndOneSampleInterval(backend, generated);
            testShortestValidStrokes(backend, generated);
            testSparseFastStroke(backend, generated);
            testExistingCurveOverwrite(backend, generated);
            testImportedCurveGridAlignment(backend, generated);
            testEraserRangeRegression(backend, generated);
            testGeneratedGapsStaySeparate(backend);
        }
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QList<int> pitchValues;
    QList<int> parameterValues;
    for (int i = 0; i < 32; ++i) {
        pitchValues.append(6000 + i * 7);
        parameterValues.append(-50000 + i * 125);
    }
    DrawCurveList pitchGenerated{curve(0, pitchValues)};
    DrawCurveList parameterGenerated{curve(0, parameterValues)};

    runAlignmentSuite(pitchGenerated);
    runAlignmentSuite(parameterGenerated);
    testUndoRedo(Backend::Rhi, ParamInfo::Pitch, pitchGenerated);
    testUndoRedo(Backend::GraphicsView, ParamInfo::Breathiness, parameterGenerated);

    qDeleteAll(pitchGenerated);
    qDeleteAll(parameterGenerated);
    return failures == 0 ? 0 : 1;
}
