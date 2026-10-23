#ifndef INFERPARAMCURVE_H
#define INFERPARAMCURVE_H

#include <QList>

/// Spacing, in ticks, between the points of a parameter curve.
///
/// The project model samples every drawn curve at this spacing (the default step of DrawCurve).
/// The integration layer reads and writes curves on the same grid: the inference input and
/// output, and the placement of an extracted pitch curve. Keep this grid aligned with the
/// default step of DrawCurve.
inline constexpr int kParamCurveStepTicks = 5;

class InferParamCurve {

public:
    int localStartTick = 0;
    QList<double> values;

    friend bool operator==(const InferParamCurve &lhs, const InferParamCurve &rhs) {
        if (lhs.localStartTick != rhs.localStartTick || lhs.values.count() != rhs.values.count())
            return false;

        for (int i = 0; i < lhs.values.count(); ++i)
            if (!qFuzzyCompare(lhs.values[i], rhs.values[i]))
                return false;

        return true;
    }

    friend bool operator!=(const InferParamCurve &lhs, const InferParamCurve &rhs) {
        return !(lhs == rhs);
    }
};

#endif // INFERPARAMCURVE_H
