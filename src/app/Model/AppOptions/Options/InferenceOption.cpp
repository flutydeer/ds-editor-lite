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
    // Normalize recognized providers that this platform/build cannot run (e.g.
    // DirectML settings carried over from a Windows installation): fall back to
    // the platform default and drop the now-meaningless GPU selection.
    if ((executionProvider == QStringLiteral("DirectML") &&
         !directMlExecutionProviderAvailable()) ||
        (executionProvider == QStringLiteral("CUDA") && !cudaExecutionProviderAvailable())) {
        const auto unavailableProvider = executionProvider;
        executionProvider = defaultExecutionProvider();
        selectedGpuIndex = -1;
        selectedGpuId.clear();
        qWarning().noquote() << QStringLiteral(
                                    "Execution provider '%1' is unavailable on this "
                                    "platform/build; falling back to '%2'.")
                                    .arg(unavailableProvider, executionProvider);
    }
    load_samplingSteps(object);
    load_depth(object);
    load_runVocoderOnCpu(object);
    load_autoStartInfer(object);
    load_playbackLookaheadSeconds(object);
    load_singerSessionCacheCapacity(object);
    singerSessionCacheCapacity = normalizeSingerSessionCacheCapacity(singerSessionCacheCapacity);
    load_singerSessionIdleTimeoutSeconds(object);
    singerSessionIdleTimeoutSeconds =
        normalizeSingerSessionIdleTimeoutSeconds(singerSessionIdleTimeoutSeconds);
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
              serialize_playbackLookaheadSeconds(),
              serialize_cacheDirectory(),
              serialize_singerSessionCacheCapacity(),
              serialize_singerSessionIdleTimeoutSeconds(),
              serialize_pitch_smooth_kernel_size()};
}
