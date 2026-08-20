#include "DrawCurveEditUtils.h"

#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/Utils/AppModelUtils.h>
#include <lite/Support/MathUtils.h>

#include <algorithm>

namespace {
    struct LineRun {
        int startTick = 0;
        QList<int> values;
    };

    int alignedTickAtOrAfter(const int tick, const int origin, const int step) {
        auto remainder = (tick - origin) % step;
        if (remainder < 0)
            remainder += step;
        return remainder == 0 ? tick : tick + step - remainder;
    }

    std::optional<int> valueAt(const DrawCurve &curve, const int tick) {
        if (curve.isEmpty() || curve.step <= 0 || tick < curve.localStart() ||
            tick > curve.localEndTick())
            return std::nullopt;

        const auto offset = tick - curve.localStart();
        const auto leftIndex = offset / curve.step;
        if (leftIndex >= curve.values().size() - 1)
            return curve.values().last();

        const auto remainder = offset % curve.step;
        if (remainder == 0)
            return curve.values().at(leftIndex);

        const auto leftValue = curve.values().at(leftIndex);
        const auto rightValue = curve.values().at(leftIndex + 1);
        return qRound(leftValue +
                      (rightValue - leftValue) * static_cast<double>(remainder) / curve.step);
    }

    bool canMerge(const DrawCurve *curve, const LineRun &line, const int step) {
        if (!curve || curve->isEmpty() || line.values.isEmpty())
            return false;
        if (curve->step != step || (line.startTick - curve->localStart()) % step != 0)
            return false;
        const auto lineEnd = line.startTick + line.values.size() * step;
        return lineEnd >= curve->localStart() && curve->localEndTick() >= line.startTick;
    }

    std::optional<DrawCurve> curveOnGrid(const DrawCurve &source, const int origin,
                                         const int step) {
        const auto startTick = alignedTickAtOrAfter(source.localStart(), origin, step);
        if (startTick >= source.localEndTick())
            return std::nullopt;

        DrawCurve result(-1);
        result.step = step;
        result.Curve::setLocalStart(startTick);
        for (auto tick = startTick; tick < source.localEndTick(); tick += step)
            result.appendValue(*valueAt(source, tick));
        return result;
    }
}

std::pair<int, int> DrawCurveEditUtils::strokeTickRange(const int previousTick,
                                                        const int currentTick) {
    return std::minmax(previousTick, currentTick);
}

DrawCurveEditUtils::StrokeState DrawCurveEditUtils::beginStroke(const QList<DrawCurve *> &curves,
                                                                const QPoint &mouseDownPosition) {
    StrokeState state;
    state.mouseDownPosition = mouseDownPosition;
    for (auto *curve : curves) {
        if (curve && curve->localStart() <= mouseDownPosition.x() &&
            curve->localEndTick() > mouseDownPosition.x()) {
            state.editingCurve = curve;
            break;
        }
    }
    state.drawOnInterval = state.editingCurve == nullptr;
    return state;
}

bool DrawCurveEditUtils::updateStroke(QList<DrawCurve *> &curves, StrokeState &state,
                                      const QPoint &previousPosition, const QPoint &currentPosition,
                                      const ValueProvider &valueAtTick) {
    if (previousPosition.x() == currentPosition.x())
        return false;

    const auto [startTick, endTick] = strokeTickRange(previousPosition.x(), currentPosition.x());
    const auto step = state.editingCurve ? state.editingCurve->step : DrawCurve().step;
    if (step <= 0)
        return false;

    const auto sampleStart =
        state.editingCurve ? alignedTickAtOrAfter(startTick, state.editingCurve->localStart(), step)
                           : startTick;
    QList<LineRun> runs;
    for (auto tick = sampleStart; tick < endTick; tick += step) {
        const auto value = valueAtTick(tick);
        if (!value)
            continue;
        if (runs.isEmpty() || runs.last().startTick + runs.last().values.size() * step != tick)
            runs.append(LineRun{tick, {}});
        runs.last().values.append(*value);
    }
    if (runs.isEmpty())
        return false;

    const auto forward = previousPosition.x() < currentPosition.x();
    bool changed = false;
    for (qsizetype i = 0; i < runs.size(); ++i) {
        const auto &run = forward ? runs.at(i) : runs.at(runs.size() - i - 1);
        const auto runEnd = run.startTick + run.values.size() * step;
        const auto overlapped = AppModelUtils::curvesIn(curves, run.startTick, runEnd);
        auto *editingCurve = canMerge(state.editingCurve, run, step) ? state.editingCurve : nullptr;
        if (!editingCurve) {
            for (auto *curve : overlapped) {
                if (canMerge(curve, run, step)) {
                    editingCurve = curve;
                    break;
                }
            }
        }
        if (!editingCurve) {
            auto seedTick = forward ? run.startTick : runEnd;
            auto seedValue = valueAtTick(seedTick);
            if (!state.newCurveCreated && state.drawOnInterval) {
                const auto mouseDownValue = valueAtTick(state.mouseDownPosition.x());
                const auto seedEnd = state.mouseDownPosition.x() + step;
                if (mouseDownValue && runEnd >= state.mouseDownPosition.x() &&
                    seedEnd >= run.startTick) {
                    seedTick = state.mouseDownPosition.x();
                    seedValue = mouseDownValue;
                }
            }
            if (!seedValue) {
                seedTick = run.startTick + (run.values.size() - 1) * step;
                seedValue = run.values.last();
            }

            editingCurve = new DrawCurve;
            editingCurve->step = step;
            editingCurve->setLocalStart(seedTick);
            editingCurve->appendValue(*seedValue);
            MathUtils::binaryInsert(curves, editingCurve);
            state.newCurveCreated = true;
        }

        DrawCurve line(-1);
        line.step = step;
        line.Curve::setLocalStart(run.startTick);
        line.setValues(run.values);
        editingCurve->mergeWithOtherPriority(line);

        for (auto *curve : overlapped) {
            if (curve == editingCurve)
                continue;
            bool merged = false;
            if (curve->step == editingCurve->step &&
                (curve->localStart() - editingCurve->localStart()) % editingCurve->step == 0) {
                editingCurve->mergeWithCurrentPriority(*curve);
                merged = true;
            } else if (auto normalized =
                           curveOnGrid(*curve, editingCurve->localStart(), editingCurve->step);
                       normalized && normalized->isOverlappedWith(editingCurve)) {
                editingCurve->mergeWithCurrentPriority(*normalized);
                merged = true;
            }
            const auto fullyCovered = curve->localStart() >= editingCurve->localStart() &&
                                      curve->localEndTick() <= editingCurve->localEndTick();
            if (merged || fullyCovered) {
                curves.removeOne(curve);
                delete curve;
            }
        }
        state.editingCurve = editingCurve;
        changed = true;
    }
    return changed;
}

std::optional<int> DrawCurveEditUtils::generatedValueAt(const QList<DrawCurve *> &curves,
                                                        const int tick) {
    for (const auto *curve : curves) {
        if (!curve || curve->isEmpty())
            continue;
        if (curve->localStart() <= tick && curve->localEndTick() > tick)
            return valueAt(*curve, tick);
        if (curve->localStart() > tick)
            break;
    }
    return std::nullopt;
}
