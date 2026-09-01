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

    int alignedTickAtOrBefore(const int tick, const int origin, const int step) {
        auto remainder = (tick - origin) % step;
        if (remainder < 0)
            remainder += step;
        return tick - remainder;
    }

    std::optional<int> valueAt(const DrawCurve &curve, const int tick) {
        if (curve.isEmpty() || curve.step <= 0 || tick < curve.localStart() ||
            tick >= curve.localEndTick())
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

    bool hasStandardGrid(const DrawCurve &curve) {
        const auto step = DrawCurve().step;
        return curve.step == step && curve.localStart() % step == 0;
    }

    DrawCurve *curveOnStandardGrid(const DrawCurve &source) {
        if (source.isEmpty() || source.step <= 0)
            return nullptr;

        const auto step = DrawCurve().step;
        auto *result = new DrawCurve(source);
        result->step = step;
        const auto startTick = alignedTickAtOrBefore(source.localStart(), 0, step);
        result->setLocalStart(startTick);

        QList<int> values;
        for (auto tick = startTick; tick < source.localEndTick(); tick += step) {
            values.append(tick < source.localStart() ? source.values().first()
                                                     : *valueAt(source, tick));
        }
        result->setValues(values);
        return result;
    }

    DrawCurve *ensureStandardGrid(QList<DrawCurve *> &curves, DrawCurve *curve) {
        if (!curve || hasStandardGrid(*curve))
            return curve;
        auto *normalized = curveOnStandardGrid(*curve);
        if (!normalized)
            return curve;

        curves.removeOne(curve);
        MathUtils::binaryInsert(curves, normalized);
        delete curve;
        return normalized;
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
    const auto step = DrawCurve().step;

    const auto sampleStart = startTick;
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

    state.editingCurve = ensureStandardGrid(curves, state.editingCurve);
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
        line.setLocalStart(run.startTick);
        line.setValues(run.values);
        editingCurve->mergeWithOtherPriority(line);

        for (auto *curve : overlapped) {
            if (curve == editingCurve)
                continue;
            auto *mergeCurve = ensureStandardGrid(curves, curve);
            if (!mergeCurve->isOverlappedWith(editingCurve))
                continue;
            editingCurve->mergeWithCurrentPriority(*mergeCurve);
            curves.removeOne(mergeCurve);
            delete mergeCurve;
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

DrawCurveEditUtils::GeneratedCurveSnapshot::~GeneratedCurveSnapshot() {
    clear();
}

void DrawCurveEditUtils::GeneratedCurveSnapshot::capture(const QList<DrawCurve *> &curves) {
    AppModelUtils::copyCurves(curves, m_curves);
}

void DrawCurveEditUtils::GeneratedCurveSnapshot::clear() {
    qDeleteAll(m_curves);
    m_curves.clear();
}

std::optional<int> DrawCurveEditUtils::GeneratedCurveSnapshot::valueAt(const int tick) const {
    return generatedValueAt(m_curves, tick);
}
