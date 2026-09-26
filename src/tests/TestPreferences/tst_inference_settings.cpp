#include "tst_preferences.h"

#include "Model/AppOptions/Options/InferenceOption.h"
#include "../TestSupport/InferenceOptionFixture.h"

#include <QtTest/QTest>
#include <QTemporaryDir>

using TestSupport::inferenceOptionConfig;

void PreferencesTests::inferenceSingerSessionCacheSettings() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto cacheDirectory = directory.path();
    InferenceOption option;
    option.load(inferenceOptionConfig(cacheDirectory, QStringLiteral("CPU")));
    QCOMPARE(option.singerSessionCacheCapacity,
             InferenceOption::kSingerSessionCacheCapacityDefault);
    QCOMPARE(option.singerSessionIdleTimeoutSeconds,
             InferenceOption::kSingerSessionIdleTimeoutDefaultSeconds);

    auto configured = inferenceOptionConfig(cacheDirectory, QStringLiteral("CPU"));
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
    QCOMPARE(option.singerSessionCacheCapacity, InferenceOption::kSingerSessionCacheCapacityMin);
    QCOMPARE(option.singerSessionIdleTimeoutSeconds, 120);

    configured.insert(QStringLiteral("singerSessionCacheCapacity"), 1000);
    configured.insert(QStringLiteral("singerSessionIdleTimeoutSeconds"), 10000);
    option.load(configured);
    QCOMPARE(option.singerSessionCacheCapacity, InferenceOption::kSingerSessionCacheCapacityMax);
    QCOMPARE(option.singerSessionIdleTimeoutSeconds,
             InferenceOption::kSingerSessionIdleTimeoutMaxSeconds);
}

void PreferencesTests::inferencePlaybackLookaheadPersistence() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto cacheDirectory = directory.path();
    InferenceOption option;
    option.load(inferenceOptionConfig(cacheDirectory, QStringLiteral("CPU")));
    QCOMPARE(option.playbackLookaheadSeconds, 20.0);

    auto configured = inferenceOptionConfig(cacheDirectory, QStringLiteral("CPU"));
    configured.insert(QStringLiteral("playbackLookaheadSeconds"), 12.0);
    option.load(configured);
    QCOMPARE(option.playbackLookaheadSeconds, 12.0);

    const auto saved = option.value();
    QCOMPARE(saved.value(QStringLiteral("playbackLookaheadSeconds")).toDouble(), 12.0);

    InferenceOption reloaded;
    reloaded.load(saved);
    QCOMPARE(reloaded.playbackLookaheadSeconds, 12.0);
}
