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

void PreferencesTests::appearanceTimecodeFormatAndTimeLane() {
    // Defaults keep the pre-existing readout and hide the seconds time axis.
    AppearanceOption defaults;
    QCOMPARE(defaults.timecodeFormat, AppearanceOption::TimecodeFormat::BarBeatTick);
    QVERIFY(!defaults.showTimeLane);
    const auto savedDefaults = defaults.value();
    QCOMPARE(savedDefaults.value(QStringLiteral("timecodeFormat")).toString(),
             QStringLiteral("barBeatTick"));
    QVERIFY(!savedDefaults.value(QStringLiteral("showTimeLane")).toBool());

    AppearanceOption loaded;
    loaded.load(QJsonObject{{QStringLiteral("timecodeFormat"), QStringLiteral("both")},
                            {QStringLiteral("showTimeLane"), true}});
    QCOMPARE(loaded.timecodeFormat, AppearanceOption::TimecodeFormat::Both);
    QVERIFY(loaded.showTimeLane);

    // An unknown id falls back to the default instead of being written back.
    AppearanceOption unknown;
    unknown.load(QJsonObject{{QStringLiteral("timecodeFormat"), QStringLiteral("frames")}});
    QCOMPARE(unknown.timecodeFormat, AppearanceOption::TimecodeFormat::BarBeatTick);
    QCOMPARE(unknown.value().value(QStringLiteral("timecodeFormat")).toString(),
             QStringLiteral("barBeatTick"));

    // Non-bool values are ignored and leave the default in place.
    AppearanceOption malformed;
    malformed.load(QJsonObject{{QStringLiteral("showTimeLane"), QStringLiteral("yes")}});
    QVERIFY(!malformed.showTimeLane);

    for (const auto format : {AppearanceOption::TimecodeFormat::BarBeatTick,
                              AppearanceOption::TimecodeFormat::Time,
                              AppearanceOption::TimecodeFormat::Both}) {
        AppearanceOption source;
        source.timecodeFormat = format;
        AppearanceOption target;
        target.load(source.value());
        QCOMPARE(target.timecodeFormat, format);
    }
}
