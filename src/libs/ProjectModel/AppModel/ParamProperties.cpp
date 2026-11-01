#include <lite/ProjectModel/AppModel/ParamProperties.h>

#include <QLocale>

#include <algorithm>
#include <cmath>

namespace {
    constexpr double PitchDisplayMaximum = 12'800.0;

    QString formatValue(const double value, const QString &unit, const bool withUnit,
                        const int precision) {
        const auto strValue = QLocale().toString(value, 'f', precision);
        if (withUnit)
            return QString("%1 %2").arg(strValue, unit);
        return strValue;
    }
}

ParamProperties::ParamProperties() = default;

bool ParamProperties::hasUnit() const {
    return !(unit.isNull() || unit.isEmpty());
}

QString ParamProperties::valueToString(const int value, const bool withUnit,
                                       const int precision) const {
    return formatValue(valueToNormalized(value), unit, withUnit, precision);
}

int ParamProperties::valueFromNormalized(const double normalized) const {
    return static_cast<int>(std::lround(valueFromNormalizedDouble(normalized)));
}

double ParamProperties::valueFromNormalizedDouble(const double normalized) const {
    return minimum +
           std::clamp(normalized, 0.0, 1.0) * static_cast<double>(maximum - minimum);
}

double ParamProperties::valueToNormalized(const int value) const {
    const auto bounded = std::clamp(value, minimum, maximum);
    return static_cast<double>(bounded - minimum) / (maximum - minimum);
}

PitchParamProperties::PitchParamProperties() {
    unit = "cent";
    showDivision = false;
    displayMode = DisplayMode::CurveOnly;
}

QString PitchParamProperties::valueToString(const int value, const bool withUnit,
                                            const int precision) const {
    return formatValue(valueToNormalized(value) * PitchDisplayMaximum, unit, withUnit, precision);
}

ExprParamProperties::ExprParamProperties() {
    valueType = ValueType::Relative;
    defaultValue = NormalizedMaximum;
    divisionValue = NormalizedMaximum / 10;
}

DecibelParamProperties::DecibelParamProperties() {
    unit = "dB";
    defaultValue = NormalizedMaximum;
    divisionValue = NormalizedMaximum / 8;
}

QString DecibelParamProperties::valueToString(const int value, const bool withUnit,
                                              const int precision) const {
    const auto decibel = -96.0 * std::pow(1.0 - valueToNormalized(value), 0.8);
    return formatValue(decibel, unit, withUnit, precision);
}

TensionParamProperties::TensionParamProperties() {
    defaultValue = NormalizedMaximum / 2;
    divisionValue = NormalizedMaximum / 10;
}

QString TensionParamProperties::valueToString(const int value, const bool withUnit,
                                              const int precision) const {
    const auto axis = valueToNormalized(value) * 2.0 - 1.0;
    const auto tension = std::copysign(10.0 * (1.0 - std::pow(1.0 - std::abs(axis), 0.7)), axis);
    return formatValue(tension, unit, withUnit, precision);
}

MouthOpeningParamProperties::MouthOpeningParamProperties() = default;

GenderParamProperties::GenderParamProperties() {
    valueType = ValueType::Relative;
    displayMode = DisplayMode::FillFromDefault;
    defaultValue = NormalizedMaximum / 2;
    showDefaultValue = true;
}

QString GenderParamProperties::valueToString(const int value, const bool withUnit,
                                             const int precision) const {
    return formatValue(valueToNormalized(value) * 2.0 - 1.0, unit, withUnit, precision);
}

VelocityParamProperties::VelocityParamProperties() {
    valueType = ValueType::Relative;
    displayMode = DisplayMode::FillFromDefault;
    defaultValue = NormalizedMaximum / 2;
    divisionValue = NormalizedMaximum / 8;
    showDefaultValue = true;
}

QString VelocityParamProperties::valueToString(const int value, const bool withUnit,
                                               const int precision) const {
    return formatValue(std::exp2(valueToNormalized(value) * 2.0 - 1.0), unit, withUnit, precision);
}

ToneShiftParamProperties::ToneShiftParamProperties() {
    valueType = ValueType::Relative;
    displayMode = DisplayMode::FillFromDefault;
    defaultValue = NormalizedMaximum / 2;
    showDefaultValue = true;
    unit = "cent";
}

QString ToneShiftParamProperties::valueToString(const int value, const bool withUnit,
                                                const int precision) const {
    Q_UNUSED(precision);
    const auto strValue = QLocale().toString(std::lround(valueToNormalized(value) * 2400.0 - 1200.0));
    if (withUnit)
        return QString("%1 %2").arg(strValue, unit);
    return strValue;
}
