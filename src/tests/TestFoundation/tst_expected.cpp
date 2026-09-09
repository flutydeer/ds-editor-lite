#include "tst_foundation.h"

#include <lite/ADT/Expected.h>

#include <QtTest/QTest>

void FoundationTests::expectedSuccessKeepsValue() {
    const Expected<int, QString> result(42);
    QVERIFY(result.isPresent());
    QCOMPARE(result.get(), 42);
    QCOMPARE(result.orElse(-1), 42);
    QVERIFY_EXCEPTION_THROWN(result.getError(), std::runtime_error);
}

void FoundationTests::expectedFailureKeepsError() {
    const Expected<int, QString> result(QStringLiteral("missing file"));
    QVERIFY(!result);
    QCOMPARE(result.getError(), QStringLiteral("missing file"));
    QCOMPARE(result.orElse(-1), -1);
    QVERIFY_EXCEPTION_THROWN(result.get(), std::runtime_error);
}

void FoundationTests::expectedFallbackIsLazy() {
    int calls = 0;
    const auto fallback = [&] {
        ++calls;
        return 7;
    };
    QCOMPARE((Expected<int, QString>(42).orElseGet(fallback)), 42);
    QCOMPARE(calls, 0);
    QCOMPARE((Expected<int, QString>(QStringLiteral("error")).orElseGet(fallback)), 7);
    QCOMPARE(calls, 1);
}

void FoundationTests::expectedMappingPropagatesErrors() {
    int calls = 0;
    const auto mapper = [&](int value) {
        ++calls;
        return value * 2;
    };
    const auto success = Expected<int, QString>(21).map(mapper);
    QCOMPARE(success.get(), 42);
    const auto failure = Expected<int, QString>(QStringLiteral("error")).map(mapper);
    QVERIFY(!failure);
    QCOMPARE(failure.getError(), QStringLiteral("error"));
    QCOMPARE(calls, 1);
}
