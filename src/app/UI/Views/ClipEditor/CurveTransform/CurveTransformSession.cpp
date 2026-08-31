#include "CurveTransformSession.h"

#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/ParamProperties.h>
#include <lite/ProjectModel/Utils/AppModelUtils.h>

#include <QtAlgorithms>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
    int alignedAtOrAfter(const int tick) {
        const auto remainder = ((tick % CurveTransform::SampleStep) + CurveTransform::SampleStep) %
                               CurveTransform::SampleStep;
        return remainder == 0 ? tick : tick + CurveTransform::SampleStep - remainder;
    }

    int alignedAtOrBefore(const int tick) {
        const auto remainder = ((tick % CurveTransform::SampleStep) + CurveTransform::SampleStep) %
                               CurveTransform::SampleStep;
        return tick - remainder;
    }

    int alignedNearest(const int tick) {
        const auto before = alignedAtOrBefore(tick);
        return tick - before < CurveTransform::SampleStep / 2.0
                   ? before
                   : before + CurveTransform::SampleStep;
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
        const auto left = curve.values().at(leftIndex);
        const auto right = curve.values().at(leftIndex + 1);
        return qRound(left + (right - left) * static_cast<double>(remainder) / curve.step);
    }

    void copyNormalizedCurves(const QList<DrawCurve *> &source, QList<DrawCurve *> &target) {
        AppModelUtils::copyCurves(source, target);
        for (auto *curve : target) {
            if (!curve || curve->isEmpty() || curve->step <= 0)
                continue;
            const auto startTick = alignedAtOrAfter(curve->localStart());
            if (curve->step == CurveTransform::SampleStep && curve->localStart() == startTick)
                continue;
            QList<int> values;
            const auto endTick = curve->localEndTick();
            values.reserve(std::max((endTick - startTick) / CurveTransform::SampleStep, 0));
            for (auto tick = startTick; tick + CurveTransform::SampleStep <= endTick;
                 tick += CurveTransform::SampleStep) {
                if (const auto sample = valueAt(*curve, tick))
                    values.append(*sample);
            }
            curve->step = CurveTransform::SampleStep;
            curve->setLocalStart(startTick);
            curve->setValues(values);
        }
    }
}

namespace CurveTransform {
    bool Bounds::isValid() const {
        return componentStart <= c && c <= a && a + 2 * SampleStep <= b && b <= d &&
               d <= componentEnd;
    }

    double smoothWeight(const int tick, const Bounds &bounds) {
        if (tick < bounds.c || tick >= bounds.d)
            return 0.0;
        if (tick < bounds.a) {
            if (bounds.c == bounds.a)
                return 0.0;
            const auto z = static_cast<double>(tick - bounds.c) / (bounds.a - bounds.c);
            return z * z * (3.0 - 2.0 * z);
        }
        if (tick < bounds.b)
            return 1.0;
        if (bounds.b == bounds.d)
            return 0.0;
        const auto z = static_cast<double>(bounds.d - tick) / (bounds.d - bounds.b);
        return z * z * (3.0 - 2.0 * z);
    }

    double factorAt(const int tick, const Bounds &bounds, const double factor) {
        return 1.0 + (std::clamp(factor, 0.0, 2.0) - 1.0) * smoothWeight(tick, bounds);
    }

    std::optional<Interval> completeSampleInterval(const int startTick, const int endTick) {
        const auto start = alignedAtOrAfter(startTick);
        const auto end = alignedAtOrBefore(endTick - SampleStep);
        if (end < start)
            return std::nullopt;
        return Interval{start, end};
    }

    Session::Session() = default;

    Session::~Session() {
        qDeleteAll(m_editedSnapshot);
    }

    void Session::setSource(const QList<DrawCurve *> &original, const QList<DrawCurve *> &edited,
                            Config config) {
        clear();
        m_config = std::move(config);
        AppModelUtils::copyCurves(edited, m_editedSnapshot);

        QList<DrawCurve *> originalSnapshot;
        QList<DrawCurve *> editedSourceSnapshot;
        copyNormalizedCurves(original, originalSnapshot);
        copyNormalizedCurves(edited, editedSourceSnapshot);
        const auto merged = AppModelUtils::mergeCurves(originalSnapshot, editedSourceSnapshot);
        qDeleteAll(originalSnapshot);
        qDeleteAll(editedSourceSnapshot);
        int previousPartition = -1;
        for (const auto *curve : merged) {
            if (!curve || curve->isEmpty() || curve->step <= 0)
                continue;
            const auto start = alignedAtOrAfter(curve->localStart());
            const auto end = alignedAtOrBefore(curve->localEndTick() - 1);
            for (auto tick = start; tick <= end; tick += SampleStep) {
                const auto sample = valueAt(*curve, tick);
                if (!sample)
                    continue;
                const auto partition = partitionAt(tick);
                if (!m_config.partitions.isEmpty() && partition < 0) {
                    previousPartition = -1;
                    continue;
                }
                if (m_components.isEmpty() || previousPartition != partition ||
                    m_components.last().endTick() != tick) {
                    m_components.append(Component{tick, {}});
                }
                m_components.last().values.append(*sample);
                previousPartition = partition;
            }
        }
        qDeleteAll(merged);
    }

    void Session::clear() {
        qDeleteAll(m_editedSnapshot);
        m_editedSnapshot.clear();
        m_components.clear();
        m_config = {};
        resetInteraction();
    }

    void Session::cancel() {
        resetInteraction();
    }

    bool Session::hasSource() const {
        return !m_components.isEmpty();
    }

    Phase Session::phase() const {
        return m_phase;
    }

    const Bounds &Session::bounds() const {
        return m_bounds;
    }

    double Session::factor() const {
        return m_factor;
    }

    void Session::beginSelection(const int tick) {
        resetInteraction();
        m_phase = Phase::Selecting;
        m_selectionStartTick = tick;
    }

    bool Session::updateSelection(const int tick) {
        if (m_phase != Phase::Selecting)
            return false;
        return updateSelectionCandidate(tick);
    }

    bool Session::finishSelection(const int tick) {
        if (m_phase != Phase::Selecting)
            return false;
        if (!updateSelectionCandidate(tick)) {
            resetInteraction();
            return false;
        }
        initializeShoulders();
        m_phase = Phase::Adjusting;
        return true;
    }

    bool Session::setBoundary(const Boundary boundary, const int tick) {
        if (m_phase != Phase::Adjusting || m_selectedComponent < 0)
            return false;
        const auto aligned = alignedNearest(tick);
        const auto before = m_bounds;
        switch (boundary) {
            case Boundary::C:
                m_bounds.c = std::clamp(aligned, m_bounds.componentStart, m_bounds.a);
                break;
            case Boundary::A:
                m_bounds.a = std::clamp(aligned, m_bounds.c, m_bounds.b - 2 * SampleStep);
                break;
            case Boundary::B:
                m_bounds.b = std::clamp(aligned, m_bounds.a + 2 * SampleStep, m_bounds.d);
                break;
            case Boundary::D:
                m_bounds.d = std::clamp(aligned, m_bounds.b, m_bounds.componentEnd);
                break;
            case Boundary::None:
                return false;
        }
        return before.c != m_bounds.c || before.a != m_bounds.a || before.b != m_bounds.b ||
               before.d != m_bounds.d;
    }

    bool Session::beginTransform() {
        if (m_phase != Phase::Adjusting || !m_bounds.isValid())
            return false;
        m_phase = Phase::Transforming;
        m_factor = 1.0;
        return true;
    }

    void Session::updateTransform(const double verticalLogicalPixelDelta) {
        if (m_phase != Phase::Transforming)
            return;
        m_factor = std::clamp(1.0 - verticalLogicalPixelDelta / 100.0, 0.0, 2.0);
    }

    QList<DrawCurve *> Session::buildEditedPreview() const {
        if (m_phase == Phase::Idle || !m_bounds.isValid()) {
            QList<DrawCurve *> result;
            AppModelUtils::copyCurves(m_editedSnapshot, result);
            return result;
        }
        auto transformed = transformedCurve();
        QList<DrawCurve *> result;
        AppModelUtils::copyCurves(m_editedSnapshot, result);
        AppModelUtils::eraseDrawCurveRange(result, transformed.localStart(),
                                           transformed.localEndTick());
        result.append(new DrawCurve(transformed));
        std::sort(result.begin(), result.end(), [](const auto *left, const auto *right) {
            return left->localStart() < right->localStart();
        });
        return result;
    }

    bool Session::hasEffectiveChange() const {
        if (m_phase != Phase::Transforming || !m_bounds.isValid() || m_selectedComponent < 0)
            return false;
        const auto &component = m_components.at(m_selectedComponent);
        for (auto tick = m_bounds.c; tick < m_bounds.d; tick += SampleStep) {
            if (transformedValueAt(tick) != component.valueAt(tick))
                return true;
        }
        return false;
    }

    int Session::Component::endTick() const {
        return startTick + values.size() * SampleStep;
    }

    int Session::Component::valueAt(const int tick) const {
        return values.at((tick - startTick) / SampleStep);
    }

    int Session::partitionAt(const int tick) const {
        if (m_config.partitions.isEmpty())
            return 0;
        for (int i = 0; i < m_config.partitions.size(); ++i) {
            const auto &partition = m_config.partitions.at(i);
            if (partition.startTick <= tick && tick <= partition.endTick)
                return i;
            if (partition.startTick > tick)
                break;
        }
        return -1;
    }

    int Session::firstTouchedComponent(const int fromTick, const int toTick) const {
        if (toTick > fromTick) {
            for (int i = 0; i < m_components.size(); ++i) {
                const auto &component = m_components.at(i);
                if (component.endTick() > fromTick && component.startTick < toTick)
                    return i;
            }
        } else if (toTick < fromTick) {
            for (int i = m_components.size() - 1; i >= 0; --i) {
                const auto &component = m_components.at(i);
                if (component.startTick < fromTick && component.endTick() > toTick)
                    return i;
            }
        }
        return -1;
    }

    bool Session::updateSelectionCandidate(const int tick) {
        m_selectedComponent = firstTouchedComponent(m_selectionStartTick, tick);
        if (m_selectedComponent < 0) {
            m_bounds = {};
            return false;
        }
        const auto &component = m_components.at(m_selectedComponent);
        const auto [rawStart, rawEnd] = std::minmax(m_selectionStartTick, tick);
        const auto a = std::max(component.startTick, alignedAtOrAfter(rawStart));
        const auto b = std::min(component.endTick(), alignedAtOrAfter(rawEnd));
        if (b - a < 2 * SampleStep) {
            m_bounds = {};
            m_selectedComponent = -1;
            return false;
        }
        m_bounds = {component.startTick, a, a, b, b, component.endTick()};
        return true;
    }

    void Session::initializeShoulders() {
        const auto leftAvailable = m_bounds.a - m_bounds.componentStart;
        const auto rightAvailable = m_bounds.componentEnd - m_bounds.b;
        const auto left = defaultShoulderWidth(m_bounds.a, leftAvailable, true);
        const auto right = defaultShoulderWidth(m_bounds.b, rightAvailable, false);
        m_bounds.c = m_bounds.a - left;
        m_bounds.d = m_bounds.b + right;
    }

    int Session::defaultShoulderWidth(const int boundaryTick, const int availableTicks,
                                      const bool left) const {
        const auto available = alignedAtOrBefore(std::max(availableTicks, 0));
        if (!m_config.tickToMilliseconds || available == 0)
            return 0;
        const auto boundaryMs = m_config.tickToMilliseconds(boundaryTick);
        int low = 0;
        int high = available / SampleStep;
        while (low < high) {
            const auto middle = (low + high + 1) / 2;
            const auto tick = boundaryTick + (left ? -1 : 1) * middle * SampleStep;
            const auto elapsed = std::abs(m_config.tickToMilliseconds(tick) - boundaryMs);
            if (elapsed <= DefaultShoulderLengthMs + 1e-9)
                low = middle;
            else
                high = middle - 1;
        }
        return low * SampleStep;
    }

    DrawCurve Session::transformedCurve() const {
        DrawCurve result(-1);
        result.step = SampleStep;
        result.setLocalStart(m_bounds.c);
        for (auto tick = m_bounds.c; tick < m_bounds.d; tick += SampleStep)
            result.appendValue(transformedValueAt(tick));
        return result;
    }

    int Session::transformedValueAt(const int tick) const {
        const auto &component = m_components.at(m_selectedComponent);
        const auto source = component.valueAt(tick);
        const auto lambda = factorAt(tick, m_bounds, m_factor);
        if (m_config.kind == Kind::ModulatePitch) {
            const auto baseline = m_config.pitchBaselineAtTick
                                      ? m_config.pitchBaselineAtTick(tick).value_or(source)
                                      : source;
            const auto value = std::clamp(baseline + lambda * (source - baseline), 0.0, 12700.0);
            return qRound(value);
        }

        if (!m_config.properties)
            return source;
        const auto normalized = m_config.properties->valueToNormalized(source);
        double result = normalized;
        if (m_config.kind == Kind::Scale) {
            result = lambda * normalized;
        } else {
            const auto targetEndTick = m_bounds.b - SampleStep;
            const auto aValue =
                m_config.properties->valueToNormalized(component.valueAt(m_bounds.a));
            const auto bValue =
                m_config.properties->valueToNormalized(component.valueAt(targetEndTick));
            const auto line = aValue + (bValue - aValue) * static_cast<double>(tick - m_bounds.a) /
                                           (targetEndTick - m_bounds.a);
            result = line + lambda * (normalized - line);
        }
        result = std::clamp(result, 0.0, 1.0);
        return std::clamp(qRound(m_config.properties->valueFromNormalizedDouble(result)),
                          m_config.properties->minimum, m_config.properties->maximum);
    }

    void Session::resetInteraction() {
        m_phase = Phase::Idle;
        m_bounds = {};
        m_selectionStartTick = 0;
        m_selectedComponent = -1;
        m_factor = 1.0;
    }
}
