#include "Model/AppOptions/Options/InferenceOption.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {

    bool expect(const bool condition, const QString &message) {
        if (condition) {
            return true;
        }
        qCritical().noquote() << message;
        return false;
    }

    QJsonObject config(const QString &cacheDirectory, const QString &provider,
                       const QString &gpuId = QStringLiteral("saved-gpu")) {
        return {
            {QStringLiteral("cacheDirectory"),    cacheDirectory},
            {QStringLiteral("executionProvider"), provider      },
            {QStringLiteral("selectedGpuIndex"),  3             },
            {QStringLiteral("selectedGpuId"),     gpuId         },
        };
    }

    bool testSupportedProviders(const QString &cacheDirectory) {
        InferenceOption option;
        option.load(config(cacheDirectory, QStringLiteral("CPU")));
        bool success = expect(option.executionProvider == QStringLiteral("CPU"),
                              QStringLiteral("CPU should remain selected"));

#if defined(Q_OS_WIN)
        option.load(config(cacheDirectory, QStringLiteral("DirectML")));
        success &= expect(option.executionProvider == QStringLiteral("DirectML"),
                          QStringLiteral("DirectML should remain selected on Windows"));
#endif
        return success;
    }

    bool testCudaProvider(const QString &cacheDirectory) {
        InferenceOption option;
        option.load(config(cacheDirectory, QStringLiteral("CUDA"), QStringLiteral("cuda-uuid")));

#if defined(ONNXRUNTIME_ENABLE_CUDA)
        bool success = expect(InferenceOption::cudaExecutionProviderAvailable(),
                              QStringLiteral("CUDA builds should report CUDA availability"));
        success &= expect(option.executionProvider == QStringLiteral("CUDA"),
                          QStringLiteral("CUDA builds should preserve the CUDA provider"));
        success &= expect(option.selectedGpuIndex == 3 &&
                              option.selectedGpuId == QStringLiteral("cuda-uuid"),
                          QStringLiteral("CUDA builds should preserve the selected CUDA device"));
#else
        bool success = expect(!InferenceOption::cudaExecutionProviderAvailable(),
                              QStringLiteral("DML-only builds should report CUDA as unavailable"));
        success &= expect(
            option.executionProvider == InferenceOption::defaultExecutionProvider(),
            QStringLiteral("DML-only builds should fall back from CUDA to the default provider"));
        success &= expect(option.selectedGpuIndex == -1 && option.selectedGpuId.isEmpty(),
                          QStringLiteral("fallback should clear the incompatible CUDA device"));
        const auto saved = option.value();
        success &= expect(saved.value(QStringLiteral("executionProvider")).toString() ==
                              InferenceOption::defaultExecutionProvider(),
                          QStringLiteral("the fallback provider should be persisted"));
#endif
        return success;
    }

    bool testSingerSessionCacheSettings(const QString &cacheDirectory) {
        InferenceOption option;
        option.load(config(cacheDirectory, QStringLiteral("CPU")));
        bool success =
            expect(option.singerSessionCacheCapacity ==
                       InferenceOption::kSingerSessionCacheCapacityDefault,
                   QStringLiteral("session cache capacity should use its default when absent"));
        success &=
            expect(option.singerSessionIdleTimeoutSeconds ==
                       InferenceOption::kSingerSessionIdleTimeoutDefaultSeconds,
                   QStringLiteral("session idle timeout should use its default when absent"));

        auto configured = config(cacheDirectory, QStringLiteral("CPU"));
        configured.insert(QStringLiteral("singerSessionCacheCapacity"), 7);
        configured.insert(QStringLiteral("singerSessionIdleTimeoutSeconds"), 120);
        option.load(configured);
        success &= expect(option.singerSessionCacheCapacity == 7 &&
                              option.singerSessionIdleTimeoutSeconds == 120,
                          QStringLiteral("valid session cache settings should be loaded"));
        const auto saved = option.value();
        success &= expect(
            saved.value(QStringLiteral("singerSessionCacheCapacity")).toInt() == 7 &&
                saved.value(QStringLiteral("singerSessionIdleTimeoutSeconds")).toInt() == 120,
            QStringLiteral("session cache settings should be persisted"));

        configured.insert(QStringLiteral("singerSessionCacheCapacity"), 0);
        configured.insert(QStringLiteral("singerSessionIdleTimeoutSeconds"), 0);
        option.load(configured);
        success &= expect(option.singerSessionCacheCapacity ==
                                  InferenceOption::kSingerSessionCacheCapacityUnlimited &&
                              option.singerSessionIdleTimeoutSeconds ==
                                  InferenceOption::kSingerSessionIdleTimeoutUnlimitedSeconds,
                          QStringLiteral("unlimited session cache settings should be preserved"));

        configured.insert(QStringLiteral("singerSessionCacheCapacity"), 0);
        configured.insert(QStringLiteral("singerSessionIdleTimeoutSeconds"), -1);
        option.load(configured);
        success &= expect(option.singerSessionCacheCapacity ==
                                  InferenceOption::kSingerSessionCacheCapacityUnlimited &&
                              option.singerSessionIdleTimeoutSeconds ==
                                  InferenceOption::kSingerSessionIdleTimeoutMinSeconds,
                          QStringLiteral("invalid session cache settings should be normalized"));

        configured.insert(QStringLiteral("singerSessionCacheCapacity"), -1);
        configured.insert(QStringLiteral("singerSessionIdleTimeoutSeconds"), 90);
        option.load(configured);
        success &= expect(
            option.singerSessionCacheCapacity == InferenceOption::kSingerSessionCacheCapacityMin &&
                option.singerSessionIdleTimeoutSeconds == 120,
            QStringLiteral("session cache settings should snap to available choices"));

        configured.insert(QStringLiteral("singerSessionCacheCapacity"), 1000);
        configured.insert(QStringLiteral("singerSessionIdleTimeoutSeconds"), 10000);
        option.load(configured);
        success &= expect(
            option.singerSessionCacheCapacity == InferenceOption::kSingerSessionCacheCapacityMax &&
                option.singerSessionIdleTimeoutSeconds ==
                    InferenceOption::kSingerSessionIdleTimeoutMaxSeconds,
            QStringLiteral("session cache settings should be clamped to their maximums"));
        return success;
    }

    bool testPlaybackLookaheadPersistence(const QString &cacheDirectory) {
        InferenceOption option;
        option.load(config(cacheDirectory, QStringLiteral("CPU")));
        bool success = expect(option.playbackLookaheadSeconds == 20.0,
                              QStringLiteral("playback lookahead should default to 20 seconds"));

        auto configured = config(cacheDirectory, QStringLiteral("CPU"));
        configured.insert(QStringLiteral("playbackLookaheadSeconds"), 12.0);
        option.load(configured);
        success &= expect(option.playbackLookaheadSeconds == 12.0,
                          QStringLiteral("playback lookahead should be loaded"));

        const auto saved = option.value();
        success &= expect(
            saved.value(QStringLiteral("playbackLookaheadSeconds")).toDouble() == 12.0,
            QStringLiteral("playback lookahead should be persisted"));

        InferenceOption reloaded;
        reloaded.load(saved);
        success &= expect(
            reloaded.playbackLookaheadSeconds == 12.0,
            QStringLiteral("playback lookahead should survive a save-load round trip"));
        return success;
    }

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    QTemporaryDir cacheDirectory;
    if (!expect(cacheDirectory.isValid(),
                QStringLiteral("temporary cache directory should exist"))) {
        return 1;
    }

    bool success = true;
    success &= testSupportedProviders(cacheDirectory.path());
    success &= testCudaProvider(cacheDirectory.path());
    success &= testSingerSessionCacheSettings(cacheDirectory.path());
    success &= testPlaybackLookaheadPersistence(cacheDirectory.path());
    return success ? 0 : 1;
}
