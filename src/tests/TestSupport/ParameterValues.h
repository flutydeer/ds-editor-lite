#ifndef TESTSUPPORT_PARAMETERVALUES_H
#define TESTSUPPORT_PARAMETERVALUES_H

#include <lite/ProjectModel/AppModel/ParamProperties.h>

#include <algorithm>
#include <cmath>

namespace TestSupport {
    constexpr int ParameterMaximum = ParamProperties::NormalizedMaximum;

    inline int parameterValue(const double normalized) {
        return static_cast<int>(
            std::llround(std::clamp(normalized, 0.0, 1.0) * ParameterMaximum));
    }

    inline int pitchValue(const int cents) {
        return parameterValue(static_cast<double>(cents) / 12'800.0);
    }

    inline int decibelValue(const double decibels) {
        const auto normalized = 1.0 - std::pow(std::clamp(-decibels / 96.0, 0.0, 1.0), 1.25);
        return parameterValue(normalized);
    }

    inline int tensionValue(const double tension) {
        const auto axis = std::clamp(tension / 10.0, -1.0, 1.0);
        const auto normalized =
            0.5 + std::copysign(0.5 * (1.0 - std::pow(1.0 - std::abs(axis), 1.0 / 0.7)), axis);
        return parameterValue(normalized);
    }

    inline int genderValue(const double gender) {
        return parameterValue((gender + 1.0) / 2.0);
    }

    inline int velocityValue(const double ratio) {
        return parameterValue(ratio > 0.0 ? (std::log2(ratio) + 1.0) / 2.0 : 0.5);
    }

    inline int toneShiftValue(const int cents) {
        return parameterValue((cents + 1200.0) / 2400.0);
    }
}

#endif // TESTSUPPORT_PARAMETERVALUES_H
