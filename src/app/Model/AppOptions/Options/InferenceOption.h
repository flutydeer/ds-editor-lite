#ifndef INFERENCEOPTION_H
#define INFERENCEOPTION_H

#include "Model/AppOptions/IOption.h"

#include <QStandardPaths>

class InferenceOption final : public IOption {
public:
    explicit InferenceOption() : IOption("inference") {
    }

    [[nodiscard]] static QString defaultExecutionProvider() {
#if defined(Q_OS_WIN)
        return QStringLiteral("DirectML");
#else
        return QStringLiteral("CPU");
#endif
    }

    [[nodiscard]] static constexpr bool cudaExecutionProviderAvailable() noexcept {
#if defined(ONNXRUNTIME_ENABLE_CUDA)
        return true;
#else
        return false;
#endif
    }

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;

    LITE_OPTION_ITEM(QString, executionProvider, defaultExecutionProvider())
    LITE_OPTION_ITEM(int, selectedGpuIndex, 0)
    LITE_OPTION_ITEM(QString, selectedGpuId, "")
    LITE_OPTION_ITEM(int, samplingSteps, 20)
    LITE_OPTION_ITEM(double, depth, 1.0)
    LITE_OPTION_ITEM(bool, runVocoderOnCpu, false)
    LITE_OPTION_ITEM(bool, autoStartInfer, true) // TODO: Rename to lazy acoustic inference?
    // Lookahead window for playback-driven inference, in seconds. Converted to ticks via the
    // timeline at runtime because inference engine operates on wall-clock time, not ticks.
    LITE_OPTION_ITEM(double, playbackLookaheadSeconds, 20.0)
    LITE_OPTION_ITEM(QString, cacheDirectory,
                     QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first() +
                         "/Cache")

    LITE_OPTION_ITEM(int, pitch_smooth_kernel_size, 0)
};

#endif // INFERENCEOPTION_H
