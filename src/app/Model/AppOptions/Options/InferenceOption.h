//
// Created by fluty on 24-9-28.
//

#ifndef INFERENCEOPTION_H
#define INFERENCEOPTION_H

#define OPTION_FIELD(FieldType, FieldName, DefaultValue)                                           \
public:                                                                                            \
    FieldType FieldName = DefaultValue;                                                            \
                                                                                                   \
private:                                                                                           \
    const QString FieldName##Key = #FieldName;\

#include "Model/AppOptions/IOption.h"

class InferenceOption final : public IOption {
public:
    explicit InferenceOption() : IOption("inference"){};

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;

    OPTION_FIELD(QString, executionProvider, "DirectML")
    OPTION_FIELD(int, selectedGpuIndex, 0)
    OPTION_FIELD(int, samplingSteps, 20)
    OPTION_FIELD(double, depth, 1.0)
};



#endif // INFERENCEOPTION_H
