#include "tst_inference_provider.h"

#include "Model/AppOptions/Options/InferenceOption.h"
#include "../TestSupport/InferenceOptionFixture.h"

#include <QtTest/QTest>
#include <QTemporaryDir>

using TestSupport::inferenceOptionConfig;

void InferenceProviderTests::supportedProviders() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto cacheDirectory = directory.path();
    InferenceOption option;
    option.load(inferenceOptionConfig(cacheDirectory, QStringLiteral("CPU")));
    QCOMPARE(option.executionProvider, QStringLiteral("CPU"));

#if defined(Q_OS_WIN)
    option.load(inferenceOptionConfig(cacheDirectory, QStringLiteral("DirectML")));
    QCOMPARE(option.executionProvider, QStringLiteral("DirectML"));
#endif
}

void InferenceProviderTests::cudaProvider() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto cacheDirectory = directory.path();
    InferenceOption option;
    option.load(
        inferenceOptionConfig(cacheDirectory, QStringLiteral("CUDA"), QStringLiteral("cuda-uuid")));

#if defined(ONNXRUNTIME_ENABLE_CUDA)
    QVERIFY2(InferenceOption::cudaExecutionProviderAvailable(),
             "CUDA builds should report CUDA availability");
    QCOMPARE(option.executionProvider, QStringLiteral("CUDA"));
    QCOMPARE(option.selectedGpuIndex, 3);
    QCOMPARE(option.selectedGpuId, QStringLiteral("cuda-uuid"));
#else
    QVERIFY2(!InferenceOption::cudaExecutionProviderAvailable(),
             "Builds without CUDA should report it as unavailable");
    QCOMPARE(option.executionProvider, InferenceOption::defaultExecutionProvider());
    QCOMPARE(option.selectedGpuIndex, -1);
    QVERIFY2(option.selectedGpuId.isEmpty(), "fallback should clear the incompatible CUDA device");
    const auto saved = option.value();
    QCOMPARE(saved.value(QStringLiteral("executionProvider")).toString(),
             InferenceOption::defaultExecutionProvider());
#endif
}
