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

    [[nodiscard]] static constexpr bool directMlExecutionProviderAvailable() noexcept {
#if defined(Q_OS_WIN)
        return true;
#else
        // DirectML is a Windows-only ONNX Runtime payload.
        return false;
#endif
    }

    static constexpr int kSingerSessionCacheCapacityUnlimited = 0;
    static constexpr int kSingerSessionCacheCapacityMin = 1;
    static constexpr int kSingerSessionCacheCapacityMax = 8;
    static constexpr int kSingerSessionCacheCapacityDefault = 4;
    static constexpr int kSingerSessionIdleTimeoutUnlimitedSeconds = 0;
    static constexpr int kSingerSessionIdleTimeoutMinSeconds = 60;
    static constexpr int kSingerSessionIdleTimeoutMaxSeconds = 300;
    static constexpr int kSingerSessionIdleTimeoutStepSeconds = 60;
    static constexpr int kSingerSessionIdleTimeoutDefaultSeconds = 60;

    [[nodiscard]] static constexpr int normalizeSingerSessionCacheCapacity(int value) noexcept {
        if (value == kSingerSessionCacheCapacityUnlimited) {
            return value;
        }
        if (value < kSingerSessionCacheCapacityMin) {
            return kSingerSessionCacheCapacityMin;
        }
        return value > kSingerSessionCacheCapacityMax ? kSingerSessionCacheCapacityMax : value;
    }

    [[nodiscard]] static constexpr int
        normalizeSingerSessionIdleTimeoutSeconds(int value) noexcept {
        if (value == kSingerSessionIdleTimeoutUnlimitedSeconds) {
            return value;
        }
        if (value <= kSingerSessionIdleTimeoutMinSeconds) {
            return kSingerSessionIdleTimeoutMinSeconds;
        }
        if (value >= kSingerSessionIdleTimeoutMaxSeconds) {
            return kSingerSessionIdleTimeoutMaxSeconds;
        }

        const int lower =
            value / kSingerSessionIdleTimeoutStepSeconds * kSingerSessionIdleTimeoutStepSeconds;
        const int upper = lower + kSingerSessionIdleTimeoutStepSeconds;
        return value - lower < upper - value ? lower : upper;
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
    LITE_OPTION_ITEM(int, singerSessionCacheCapacity, kSingerSessionCacheCapacityDefault)
    LITE_OPTION_ITEM(int, singerSessionIdleTimeoutSeconds, kSingerSessionIdleTimeoutDefaultSeconds)

    LITE_OPTION_ITEM(int, pitch_smooth_kernel_size, 0)
};

#endif // INFERENCEOPTION_H
