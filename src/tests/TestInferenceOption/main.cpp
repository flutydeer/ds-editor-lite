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
    return success ? 0 : 1;
}
