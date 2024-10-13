//
// Created by fluty on 24-10-14.
//

#include "ParamProperties.h"

#include "Utils/MathUtils.h"

bool ParamProperties::hasUnit() const {
    return !(unit.isNull() || unit.isEmpty());
}

QString ParamProperties::valueToString(int value, bool withUnit) const {
    auto strValue = valueToDisplayString(value);
    if (withUnit)
        return QString("%1 %2").arg(strValue, unit);
    return strValue;
}

int ParamProperties::valueFromNormalized(double normalized) {
    return static_cast<int>(normalized * (maximum - minimum)) + minimum;
}

double ParamProperties::valueToNormalized(int value) {
    return 1.0 * (value - minimum) / (maximum - minimum);
}

QString ParamProperties::valueToDisplayString(int value) const {
    return QString::number(value / 1000.0, 'f', 3);
}

ExprParamProperties::ExprParamProperties() {
    hasDefaultValue = true;
    defaultValue = 1000;
}

DecibelParamProperties::DecibelParamProperties() {
    unit = "dB";
    minimum = -96'000;
    maximum = 0;
    displayDivision = 12'000; // 12 dB
}

int DecibelParamProperties::valueFromNormalized(double normalized) {
    auto value = (MathUtils::inPowerCurveValueAt(normalized, 0.8) - 1) * (maximum - minimum);
    return static_cast<int>(value);
}

double DecibelParamProperties::valueToNormalized(int value) {
    auto normalizedValue = 1.0 * (value - minimum) / (maximum - minimum);
    return MathUtils::inPowerCurveXAt(normalizedValue, 0.8);
}

QString DecibelParamProperties::valueToDisplayString(int value) const {
    return QString::number(value / 1000.0, 'f', 1);
}

TensionParamProperties::TensionParamProperties() {
    minimum = -10'000;
    maximum = 10'000;
}

GenderParamProperties::GenderParamProperties() {
    displayMode = DisplayMode::FillFromDefault;
    minimum = -1'000;
    hasDefaultValue = true;
    defaultValue = 0;
}

VelocityParamProperties::VelocityParamProperties() {
    displayMode = DisplayMode::FillFromDefault;
    minimum = 500;
    maximum = 2'000;
    hasDefaultValue = true;
    defaultValue = 1'000;
    displayDivision = 250;
}

int VelocityParamProperties::valueFromNormalized(double normalized) {
    auto power = 2 * normalized - 1;
    auto value = std::pow(2, power) * 1000;
    return static_cast<int>(value);
}

double VelocityParamProperties::valueToNormalized(int value) {
    auto num = std::log2(value / 1000.0) + 1;
    return num / 2;
}