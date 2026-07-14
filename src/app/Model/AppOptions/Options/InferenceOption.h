//
// Created by fluty on 24-9-28.
//

#ifndef INFERENCEOPTION_H
#define INFERENCEOPTION_H

#include "Model/AppOptions/IOption.h"

#include <QStandardPaths>

#if defined(Q_OS_WIN)
#  define LITE_DEFAULT_EXECUTION_PROVIDER "DirectML"
#else
#  define LITE_DEFAULT_EXECUTION_PROVIDER "CPU"
#endif

class InferenceOption final : public IOption {
public:
    explicit InferenceOption() : IOption("inference") {
    }

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;

    LITE_OPTION_ITEM(QString, executionProvider, LITE_DEFAULT_EXECUTION_PROVIDER)
    LITE_OPTION_ITEM(int, selectedGpuIndex, 0)
    LITE_OPTION_ITEM(QString, selectedGpuId, "")
    LITE_OPTION_ITEM(int, samplingSteps, 20)
    LITE_OPTION_ITEM(double, depth, 1.0)
    LITE_OPTION_ITEM(bool, runVocoderOnCpu, false)
    LITE_OPTION_ITEM(bool, autoStartInfer, true) // TODO: Rename to lazy acoustic inference?
    LITE_OPTION_ITEM(QString, cacheDirectory,
                     QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first() +
                         "/Cache")

    LITE_OPTION_ITEM(int, pitch_smooth_kernel_size, 0)
};


#undef LITE_DEFAULT_EXECUTION_PROVIDER

#endif // INFERENCEOPTION_H
