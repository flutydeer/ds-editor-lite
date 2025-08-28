//
// Created by fluty on 24-9-28.
//

#ifndef INFERENCEOPTION_H
#define INFERENCEOPTION_H

#include "Model/AppOptions/IOption.h"

#include <QStandardPaths>

class InferenceOption final : public IOption {
public:
    explicit InferenceOption() : IOption("inference") {
    }

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;

    LITE_OPTION_ITEM(QString, executionProvider, "DirectML")
    LITE_OPTION_ITEM(int, selectedGpuIndex, 0)
    LITE_OPTION_ITEM(QString, selectedGpuId, "")
    LITE_OPTION_ITEM(int, samplingSteps, 20)
    LITE_OPTION_ITEM(double, depth, 1.0)
    LITE_OPTION_ITEM(bool, runVocoderOnCpu, false)
    LITE_OPTION_ITEM(bool, autoStartInfer, true)
    LITE_OPTION_ITEM(QString, cacheDirectory,
                     QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first() +
                         "/Cache")

    LITE_OPTION_ITEM(int, pitch_smooth_kernel_size, 0)
};


#endif // INFERENCEOPTION_H