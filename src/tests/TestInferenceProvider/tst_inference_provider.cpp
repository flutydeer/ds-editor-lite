#include "tst_inference_provider.h"

#include "Model/AppOptions/Options/InferenceOption.h"
#include "Modules/Inference/ExecutionProvider.h"
#include "../TestSupport/InferenceOptionFixture.h"

#include <QtTest/QTest>
#include <QTemporaryDir>
#include <QScopeGuard>

Q_DECLARE_METATYPE(ExecutionProvider)

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

void InferenceProviderTests::providerResolution_data() {
    QTest::addColumn<QString>("persisted");
    QTest::addColumn<bool>("gpuAvailable");
    QTest::addColumn<ExecutionProvider>("expected");
    QTest::addColumn<bool>("changed");
    QTest::newRow("cpu") << QStringLiteral("CPU") << false << ExecutionProvider::Cpu << false;
#if defined(Q_OS_WIN)
    QTest::newRow("directml-device")
        << QStringLiteral("DirectML") << true << ExecutionProvider::DirectML << false;
#else
    QTest::newRow("directml-unavailable-build")
        << QStringLiteral("DirectML") << true << ExecutionProvider::Cpu << true;
#endif
    QTest::newRow("directml-no-device")
        << QStringLiteral("DirectML") << false << ExecutionProvider::Cpu << true;
#if defined(ONNXRUNTIME_ENABLE_CUDA)
    QTest::newRow("cuda-device") << QStringLiteral("CUDA") << true << ExecutionProvider::Cuda
                                 << false;
#else
    QTest::newRow("cuda-unavailable-build")
        << QStringLiteral("CUDA") << true << ExecutionProvider::Cpu << true;
#endif
    QTest::newRow("cuda-no-device")
        << QStringLiteral("CUDA") << false << ExecutionProvider::Cpu << true;
    QTest::newRow("unknown") << QStringLiteral("BogusEP") << true << ExecutionProvider::Cpu << true;
    QTest::newRow("empty") << QString() << true << ExecutionProvider::Cpu << true;
}

void InferenceProviderTests::providerResolution() {
    QFETCH(QString, persisted);
    QFETCH(bool, gpuAvailable);
    QFETCH(ExecutionProvider, expected);
    QFETCH(bool, changed);
    const auto result = ExecutionProviderUtils::resolve(persisted, {.gpuFound = gpuAvailable});
    QCOMPARE(result.provider, expected);
    QCOMPARE(result.changed, changed);
    QCOMPARE(result.reason.isEmpty(), !changed);
    const auto previous = ExecutionProviderUtils::effective();
    const auto restore =
        qScopeGuard([previous] { ExecutionProviderUtils::setEffective(previous); });
    ExecutionProviderUtils::setEffective(result.provider);
    QCOMPARE(ExecutionProviderUtils::effective(), expected);
}

void InferenceProviderTests::providerNamesRoundTrip() {
    for (const auto provider :
         {ExecutionProvider::Cpu, ExecutionProvider::DirectML, ExecutionProvider::Cuda}) {
        QCOMPARE(ExecutionProviderUtils::fromString(ExecutionProviderUtils::toString(provider)),
                 std::optional<ExecutionProvider>(provider));
    }
    QVERIFY(!ExecutionProviderUtils::fromString(QStringLiteral("gpu")));
}
