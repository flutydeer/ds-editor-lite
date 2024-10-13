//
// Created by fluty on 24-10-14.
//

#ifndef PARAMPROPERTIES_H
#define PARAMPROPERTIES_H

#include <QString>

class ParamProperties {
public:
    enum class DisplayMode { FillFromBottom, FillFromDefault, CurveOnly };

    DisplayMode displayMode = DisplayMode::FillFromBottom;
    int minimum = 0;
    int maximum = 1'000;
    QString unit;
    bool hasDefaultValue = false;
    int defaultValue = 0;
    int displayDivision = 200;
    [[nodiscard]] bool hasUnit() const;
    [[nodiscard]] virtual QString valueToString(int value, bool withUnit) const;
    [[nodiscard]] virtual int valueFromNormalized(double normalized);
    [[nodiscard]] virtual double valueToNormalized(int value);
    virtual ~ParamProperties() = default;

protected:
    [[nodiscard]] virtual QString valueToDisplayString(int value) const;
};

class ExprParamProperties final : public ParamProperties {
public:
    explicit ExprParamProperties();
};

class DecibelParamProperties final : public ParamProperties {
public:
    explicit DecibelParamProperties();
    int valueFromNormalized(double normalized) override;
    double valueToNormalized(int value) override;

private:
    [[nodiscard]] QString valueToDisplayString(int value) const override;
};

class TensionParamProperties final : public ParamProperties {
public:
    explicit TensionParamProperties();
};

class GenderParamProperties final : public ParamProperties {
public:
    explicit GenderParamProperties();
};

class VelocityParamProperties final : public ParamProperties {
public:
    explicit VelocityParamProperties();
    int valueFromNormalized(double normalized) override;
    double valueToNormalized(int value) override;
};

#endif // PARAMPROPERTIES_H
