//
// Created by fluty on 24-9-28.
//

#include "InferenceOption.h"

#include <lite/Support/Log.h>

#include <QDir>

void InferenceOption::load(const QJsonObject &object) {
    load_cacheDirectory(object);
    const QDir configDir(cacheDirectory);
    if (!configDir.exists()) {
        if (configDir.mkpath("."))
            qDebug() << "Successfully created cache directory";
        else
            qCritical() << "Failed to create cache directory";
    } else
        qDebug() << "Cache directory already exists";

    load_executionProvider(object);
    load_selectedGpuIndex(object);
    load_selectedGpuId(object);
    load_samplingSteps(object);
    load_depth(object);
    load_runVocoderOnCpu(object);
    load_autoStartInfer(object);
    load_pitch_smooth_kernel_size(object);
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