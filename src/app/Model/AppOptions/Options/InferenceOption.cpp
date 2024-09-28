//
// Created by fluty on 24-9-28.
//

#include "InferenceOption.h"

void InferenceOption::load(const QJsonObject &object) {
    if (object.contains(executionProviderKey))
        executionProvider = object[executionProviderKey].toString();
    if (object.contains(selectedGpuIndexKey))
        selectedGpuIndex = object[selectedGpuIndexKey].toInt();
    if (object.contains(samplingStepsKey))
        samplingSteps = object[samplingStepsKey].toInt();
    if (object.contains(depthKey))
        depth = object[depthKey].toDouble();
}

void InferenceOption::save(QJsonObject &object) {
    object[executionProviderKey] = executionProvider;
    object[selectedGpuIndexKey] = selectedGpuIndex;
    object[samplingStepsKey] = samplingSteps;
    object[depthKey] = depth;
}