//
// Created by fluty on 24-10-14.
//

#include "ParamProperties.h"

#include "Utils/MathUtils.h"

bool ParamProperties::hasUnit() const {
    return !(unit.isNull() || unit.isEmpty());
}

QString ParamProperties::valueToString(int value, bool withUnit, int precision) const {
    auto strValue = QString::number(value / 1000.0, 'f', precision);
    if (withUnit)
        return QString("%1 %2").arg(strValue, unit);
    return strValue;
}

int ParamProperties::valueFromNormalized(double normalized) const {
    return static_cast<int>(normalized * (maximum - minimum)) + minimum;
}

double ParamProperties::valueToNormalized(int value) const {
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

int DecibelParamProperties::valueFromNormalized(double normalized) const {
    auto value = (MathUtils::inPowerCurveValueAt(normalized, 0.8) - 1) * (maximum - minimum);
    return static_cast<int>(value);
}

double DecibelParamProperties::valueToNormalized(int value) const {
    auto normalizedValue = 1.0 * (value - minimum) / (maximum - minimum);
    return MathUtils::inPowerCurveXAt(normalizedValue, 0.8);
}

TensionParamProperties::TensionParamProperties() {
    minimum = -10'000;
    maximum = 10'000;
    divisionValue = 2'000;
}

GenderParamProperties::GenderParamProperties() {
    valueType = ValueType::Relative;
    displayMode = DisplayMode::FillFromDefault;
    minimum = -1'000;
    defaultValue = 0;
}

VelocityParamProperties::VelocityParamProperties() {
    valueType = ValueType::Relative;
    displayMode = DisplayMode::FillFromDefault;
    minimum = 500;
    maximum = 2'000;
    defaultValue = 1'000;
    divisionValue = 250;
}

int VelocityParamProperties::valueFromNormalized(double normalized) const {
    auto power = 2 * normalized - 1;
    auto value = std::pow(2, power) * 1000;
    return static_cast<int>(value);
}

double VelocityParamProperties::valueToNormalized(int value) const {
    auto num = std::log2(value / 1000.0) + 1;
    return num / 2;
}