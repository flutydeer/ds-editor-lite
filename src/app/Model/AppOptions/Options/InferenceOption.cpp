//
// Created by fluty on 24-9-28.
//

#include "InferenceOption.h"

#include "Utils/Log.h"

#include <QDir>

void InferenceOption::load(const QJsonObject &object) {
    if (object.contains(cacheDirectoryKey))
        cacheDirectory = object[cacheDirectoryKey].toString();
    const QDir configDir(cacheDirectory);
    if (!configDir.exists()) {
        if (configDir.mkpath("."))
            qDebug() << "Successfully created cache directory";
        else
            qCritical() << "Failed to create cache directory";
    } else
        qDebug() << "Cache directory already exists";

    if (object.contains(executionProviderKey))
        executionProvider = object[executionProviderKey].toString();
    if (object.contains(selectedGpuIndexKey))
        selectedGpuIndex = object[selectedGpuIndexKey].toInt();
    if (object.contains(selectedGpuIdKey))
        selectedGpuId = object[selectedGpuIdKey].toString();
    if (object.contains(samplingStepsKey))
        samplingSteps = object[samplingStepsKey].toInt();
    if (object.contains(depthKey))
        depth = object[depthKey].toDouble();
    if (object.contains(runVocoderOnCpuKey))
        runVocoderOnCpu = object[runVocoderOnCpuKey].toBool();
    if (object.contains(autoStartInferKey))
        autoStartInfer = object[autoStartInferKey].toBool();
    if (object.contains(pitch_smooth_kernel_sizeKey))
        pitch_smooth_kernel_size = object[pitch_smooth_kernel_sizeKey].toInt();
}

void InferenceOption::save(QJsonObject &object) {
    object = {serialize_executionProvider(),
              serialize_selectedGpuIndex(),
              serialize_selectedGpuId(),
              serialize_samplingSteps(),
              serialize_depth(),
              serialize_runVocoderOnCpu(),
              serialize_autoStartInfer(),
              serialize_cacheDirectory(),
              serialize_pitch_smooth_kernel_size()
    };
}