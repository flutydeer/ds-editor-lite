#include <QtTest/QTest>
#include "Model/AppOptions/Options/InferenceOption.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {
    QJsonObject config(const QString &cacheDirectory, const QString &provider,
                       const QString &gpuId = QStringLiteral("saved-gpu")) {
        return {
            {QStringLiteral("cacheDirectory"),    cacheDirectory},
            {QStringLiteral("executionProvider"), provider      },
            {QStringLiteral("selectedGpuIndex"),  3             },
            {QStringLiteral("selectedGpuId"),     gpuId         },
        };
    }

} // namespace

class InferenceOptionTests final : public QObject {
    Q_OBJECT

private slots:

    void supportedProviders() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto cacheDirectory = directory.path();
        InferenceOption option;
        option.load(config(cacheDirectory, QStringLiteral("CPU")));
        QCOMPARE(option.executionProvider, QStringLiteral("CPU"));

#if defined(Q_OS_WIN)
        option.load(config(cacheDirectory, QStringLiteral("DirectML")));
        QCOMPARE(option.executionProvider, QStringLiteral("DirectML"));
#endif
    }

    void cudaProvider() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto cacheDirectory = directory.path();
        InferenceOption option;
        option.load(config(cacheDirectory, QStringLiteral("CUDA"), QStringLiteral("cuda-uuid")));

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
        QVERIFY2(option.selectedGpuId.isEmpty(),
                 "fallback should clear the incompatible CUDA device");
        const auto saved = option.value();
        QCOMPARE(saved.value(QStringLiteral("executionProvider")).toString(),
                 InferenceOption::defaultExecutionProvider());
#endif
    }

    void singerSessionCacheSettings() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto cacheDirectory = directory.path();
        InferenceOption option;
        option.load(config(cacheDirectory, QStringLiteral("CPU")));
        QCOMPARE(option.singerSessionCacheCapacity,
                 InferenceOption::kSingerSessionCacheCapacityDefault);
        QCOMPARE(option.singerSessionIdleTimeoutSeconds,
                 InferenceOption::kSingerSessionIdleTimeoutDefaultSeconds);

        auto configured = config(cacheDirectory, QStringLiteral("CPU"));
        configured.insert(QStringLiteral("singerSessionCacheCapacity"), 7);
        configured.insert(QStringLiteral("singerSessionIdleTimeoutSeconds"), 120);
        option.load(configured);
        QCOMPARE(option.singerSessionCacheCapacity, 7);
        QCOMPARE(option.singerSessionIdleTimeoutSeconds, 120);
        const auto saved = option.value();
        QCOMPARE(saved.value(QStringLiteral("singerSessionCacheCapacity")).toInt(), 7);
        QCOMPARE(saved.value(QStringLiteral("singerSessionIdleTimeoutSeconds")).toInt(), 120);

        configured.insert(QStringLiteral("singerSessionCacheCapacity"), 0);
        configured.insert(QStringLiteral("singerSessionIdleTimeoutSeconds"), 0);
        option.load(configured);
        QCOMPARE(option.singerSessionCacheCapacity,
                 InferenceOption::kSingerSessionCacheCapacityUnlimited);
        QCOMPARE(option.singerSessionIdleTimeoutSeconds,
                 InferenceOption::kSingerSessionIdleTimeoutUnlimitedSeconds);

        configured.insert(QStringLiteral("singerSessionCacheCapacity"), 0);
        configured.insert(QStringLiteral("singerSessionIdleTimeoutSeconds"), -1);
        option.load(configured);
        QCOMPARE(option.singerSessionCacheCapacity,
                 InferenceOption::kSingerSessionCacheCapacityUnlimited);
        QCOMPARE(option.singerSessionIdleTimeoutSeconds,
                 InferenceOption::kSingerSessionIdleTimeoutMinSeconds);

        configured.insert(QStringLiteral("singerSessionCacheCapacity"), -1);
        configured.insert(QStringLiteral("singerSessionIdleTimeoutSeconds"), 90);
        option.load(configured);
        QCOMPARE(option.singerSessionCacheCapacity,
                 InferenceOption::kSingerSessionCacheCapacityMin);
        QCOMPARE(option.singerSessionIdleTimeoutSeconds, 120);

        configured.insert(QStringLiteral("singerSessionCacheCapacity"), 1000);
        configured.insert(QStringLiteral("singerSessionIdleTimeoutSeconds"), 10000);
        option.load(configured);
        QCOMPARE(option.singerSessionCacheCapacity,
                 InferenceOption::kSingerSessionCacheCapacityMax);
        QCOMPARE(option.singerSessionIdleTimeoutSeconds,
                 InferenceOption::kSingerSessionIdleTimeoutMaxSeconds);
    }

    void playbackLookaheadPersistence() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto cacheDirectory = directory.path();
        InferenceOption option;
        option.load(config(cacheDirectory, QStringLiteral("CPU")));
        QCOMPARE(option.playbackLookaheadSeconds, 20.0);

        auto configured = config(cacheDirectory, QStringLiteral("CPU"));
        configured.insert(QStringLiteral("playbackLookaheadSeconds"), 12.0);
        option.load(configured);
        QCOMPARE(option.playbackLookaheadSeconds, 12.0);

        const auto saved = option.value();
        QCOMPARE(saved.value(QStringLiteral("playbackLookaheadSeconds")).toDouble(), 12.0);

        InferenceOption reloaded;
        reloaded.load(saved);
        QCOMPARE(reloaded.playbackLookaheadSeconds, 12.0);
    }
};

QTEST_GUILESS_MAIN(InferenceOptionTests)
#include "main.moc"
