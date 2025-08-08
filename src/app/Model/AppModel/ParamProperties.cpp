//
// Created by fluty on 24-10-14.
//

#include "ParamProperties.h"

#include "Utils/MathUtils.h"

ParamProperties::ParamProperties() = default;

bool ParamProperties::hasUnit() const {
    return !(unit.isNull() || unit.isEmpty());
}

QString ParamProperties::valueToString(const int value, const bool withUnit,
                                       const int precision) const {
    auto strValue = QString::number(value / 1000.0, 'f', precision);
    if (withUnit)
        return QString("%1 %2").arg(strValue, unit);
    return strValue;
}

int ParamProperties::valueFromNormalized(const double normalized) const {
    return static_cast<int>(normalized * (maximum - minimum)) + minimum;
}

double ParamProperties::valueToNormalized(const int value) const {
    return 1.0 * (value - minimum) / (maximum - minimum);
}

PitchParamProperties::PitchParamProperties() {
    showDivision = false;
    displayMode = DisplayMode::CurveOnly;
}

ExprParamProperties::ExprParamProperties() {
    valueType = ValueType::Relative;
    defaultValue = 1000;
    divisionValue = 100;
}

DecibelParamProperties::DecibelParamProperties() {
    unit = "dB";
    minimum = -96'000;
    maximum = 0;
    divisionValue = 12'000; // 12 dB
}

int DecibelParamProperties::valueFromNormalized(const double normalized) const {
    const auto value = (MathUtils::inPowerCurveValueAt(normalized, 0.8) - 1) * (maximum - minimum);
    return static_cast<int>(value);
}

double DecibelParamProperties::valueToNormalized(const int value) const {
    const auto normalizedValue = 1.0 * (value - minimum) / (maximum - minimum);
    return MathUtils::inPowerCurveXAt(normalizedValue, 0.8);
}

TensionParamProperties::TensionParamProperties() {
    minimum = -10'000;
    maximum = 10'000;
    divisionValue = 2'000;
}

int TensionParamProperties::valueFromNormalized(const double normalized) const {
    const auto base = (normalized - 0.5) * 2;
    const auto input = qAbs(base);
    const auto absValue = MathUtils::inPowerCurveValueAt(input, 0.7) * maximum;
    return static_cast<int>(base >= 0 ? absValue : -absValue);
}

double TensionParamProperties::valueToNormalized(const int value) const {
    const auto normalized = 1.0 * value / maximum;
    const auto inputValue = qAbs(normalized);
    const auto scaled = MathUtils::inPowerCurveXAt(inputValue, 0.7);
    return scaled / 2 * (normalized > 0 ? 1 : -1) + 0.5;
}

MouthOpeningParamProperties::MouthOpeningParamProperties() = default;

GenderParamProperties::GenderParamProperties() {
    valueType = ValueType::Relative;
    displayMode = DisplayMode::FillFromDefault;
    minimum = -1'000;
    defaultValue = 0;
    showDefaultValue = true;
}

VelocityParamProperties::VelocityParamProperties() {
    valueType = ValueType::Relative;
    displayMode = DisplayMode::FillFromDefault;
    minimum = 500;
    maximum = 2'000;
    defaultValue = 1'000;
    divisionValue = 250;
    showDefaultValue = true;
}

int VelocityParamProperties::valueFromNormalized(const double normalized) const {
    const auto power = 2 * normalized - 1;
    const auto value = std::pow(2, power) * 1000;
    return static_cast<int>(value);
}

double VelocityParamProperties::valueToNormalized(const int value) const {
    const auto num = std::log2(value / 1000.0) + 1;
    return num / 2;
}

ToneShiftParamProperties::ToneShiftParamProperties() {
    valueType = ValueType::Relative;
    displayMode = DisplayMode::FillFromDefault;
    minimum = -1'200;
    maximum = 1'200;
    defaultValue = 0;
    showDefaultValue = true;
    unit = "cent";
}

QString ToneShiftParamProperties::valueToString(const int value, const bool withUnit,
                                                const int precision) const {
    Q_UNUSED(precision);
    auto strValue = QString::number(value);
    if (withUnit)
        return QString("%1 %2").arg(strValue, unit);
    return strValue;
}