#include "tst_preferences.h"

#include "Model/AppOptions/Options/AppearanceOption.h"

#include <QtTest/QTest>

void PreferencesTests::appearanceNativeFrameOverride_data() {
    QTest::addColumn<bool>("savedFrame");
    QTest::addColumn<bool>("forcedFrame");
    QTest::newRow("native-to-frameless") << true << false;
    QTest::newRow("frameless-to-native") << false << true;
    QTest::newRow("native-to-native") << true << true;
    QTest::newRow("frameless-to-frameless") << false << false;
}

void PreferencesTests::appearanceNativeFrameOverride() {
    QFETCH(bool, savedFrame);
    QFETCH(bool, forcedFrame);

    AppearanceOption option;
    option.load(QJsonObject{{QStringLiteral("useNativeFrame"), savedFrame}});
    QCOMPARE(option.effectiveUseNativeFrame(), savedFrame);
    QVERIFY(!option.nativeFrameOverride.has_value());
    AppearanceOption withoutOverride;
    withoutOverride.load(option.value());
    QCOMPARE(withoutOverride.effectiveUseNativeFrame(), savedFrame);

    option.nativeFrameOverride = forcedFrame;
    QCOMPARE(option.effectiveUseNativeFrame(), forcedFrame);
    QCOMPARE(option.useNativeFrame, savedFrame);

    // Saving another option during capture must not persist the forced frame mode.
    option.animationEnabled = false;
    const auto saved = option.value();
    QCOMPARE(saved.value(QStringLiteral("useNativeFrame")).toBool(), savedFrame);
    QVERIFY(!saved.value(QStringLiteral("animationEnabled")).toBool());

    AppearanceOption reloaded;
    reloaded.load(saved);
    QCOMPARE(reloaded.effectiveUseNativeFrame(), savedFrame);
    QVERIFY(!reloaded.nativeFrameOverride.has_value());

    option.nativeFrameOverride.reset();
    QCOMPARE(option.effectiveUseNativeFrame(), savedFrame);
    QCOMPARE(option.value().value(QStringLiteral("useNativeFrame")).toBool(), savedFrame);
}
