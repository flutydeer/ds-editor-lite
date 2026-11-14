#ifndef PARAMPROPERTIES_H
#define PARAMPROPERTIES_H

#include <QString>

class ParamProperties {
public:
    enum class ValueType { Absolute, Relative };
    enum class DisplayMode { FillFromBottom, FillFromDefault, CurveOnly };

    static constexpr int NormalizedMaximum = 8'064'000;

    ValueType valueType = ValueType::Absolute;
    DisplayMode displayMode = DisplayMode::FillFromBottom;
    int minimum = 0;
    int maximum = NormalizedMaximum;
    QString unit;
    int defaultValue = 0;
    bool showDefaultValue = false;
    bool showDivision = true;
    int divisionValue = NormalizedMaximum / 10;
    int displayPrecision = 3;
    bool hasUnit() const;
    virtual QString valueToString(int value, bool withUnit, int precision = 3) const;
    virtual int valueFromNormalized(double normalized) const;
    virtual double valueFromNormalizedDouble(double normalized) const;
    virtual double valueToNormalized(int value) const;
    explicit ParamProperties();
    virtual ~ParamProperties() = default;
};

class PitchParamProperties final : public ParamProperties {
public:
    explicit PitchParamProperties();
    QString valueToString(int value, bool withUnit, int precision = 3) const override;
};

class ExprParamProperties final : public ParamProperties {
public:
    explicit ExprParamProperties();
};

class DecibelParamProperties final : public ParamProperties {
public:
    explicit DecibelParamProperties();
    QString valueToString(int value, bool withUnit, int precision = 3) const override;
};

class TensionParamProperties final : public ParamProperties {
public:
    explicit TensionParamProperties();
    QString valueToString(int value, bool withUnit, int precision = 3) const override;
};

class MouthOpeningParamProperties final : public ParamProperties {
public:
    explicit MouthOpeningParamProperties();
};

class GenderParamProperties final : public ParamProperties {
public:
    explicit GenderParamProperties();
    QString valueToString(int value, bool withUnit, int precision = 3) const override;
};

class VelocityParamProperties final : public ParamProperties {
public:
    explicit VelocityParamProperties();
    QString valueToString(int value, bool withUnit, int precision = 3) const override;
};

class ToneShiftParamProperties final : public ParamProperties {
public:
    explicit ToneShiftParamProperties();
    QString valueToString(int value, bool withUnit, int precision = 3) const override;
};

#endif // PARAMPROPERTIES_H
