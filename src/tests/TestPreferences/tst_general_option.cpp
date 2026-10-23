#include "tst_preferences.h"

#include "Model/AppOptions/Options/GeneralOption.h"

#include <QJsonObject>
#include <QtTest/QTest>

void PreferencesTests::generalOptionDefaultLyricPrecedence() {
    GeneralOption option;

    // A language the editor ships a value for resolves to that value.
    QCOMPARE(option.defaultLyricForLanguage(QStringLiteral("cmn")), QStringLiteral("啦"));
    QCOMPARE(option.defaultLyricForLanguage(QStringLiteral("eng")), QStringLiteral("la"));
    // A language nothing knows about falls back to "la".
    QCOMPARE(option.defaultLyricForLanguage(QStringLiteral("zzz")), QStringLiteral("la"));

    // The values the settings ship with are not user overrides, so the built-in value stands.
    QVERIFY(!option.hasDefaultLyricOverride(QStringLiteral("eng")));
    QCOMPARE(option.defaultLyricForLanguage(QStringLiteral("eng")), QStringLiteral("la"));

    // A changed value is an override and wins over the built-in value.
    option.defaultLyrics.insert(QStringLiteral("eng"), QStringLiteral("ah"));
    QVERIFY(option.hasDefaultLyricOverride(QStringLiteral("eng")));
    QCOMPARE(option.defaultLyricForLanguage(QStringLiteral("eng")), QStringLiteral("ah"));
    // Setting the value back to the built-in one asks for the default again.
    option.defaultLyrics.insert(QStringLiteral("eng"), QStringLiteral("la"));
    QVERIFY(!option.hasDefaultLyricOverride(QStringLiteral("eng")));
    QCOMPARE(option.defaultLyricForLanguage(QStringLiteral("eng")), QStringLiteral("la"));

    // An empty value asks for the default rather than for an empty lyric.
    option.defaultLyrics.insert(QStringLiteral("jpn"), QString());
    QVERIFY(!option.hasDefaultLyricOverride(QStringLiteral("jpn")));
    QCOMPARE(option.defaultLyricForLanguage(QStringLiteral("jpn")), QStringLiteral("ら"));
    // A language this build knows nothing about has no built-in value to fall back to.
    QVERIFY(GeneralOption::builtinDefaultLyricForLanguage(QStringLiteral("deu")).isEmpty());
    QCOMPARE(GeneralOption::builtinDefaultLyrics().size(), 4);
}

void PreferencesTests::generalOptionDefaultLyricStorage() {
    GeneralOption option;

    // A profile written by an older build keeps its shape: the built-in values are still stored.
    const auto stored = option.value().value(QStringLiteral("defaultLyrics")).toObject();
    QCOMPARE(stored.size(), 4);
    QCOMPARE(stored.value(QStringLiteral("cmn")).toString(), QStringLiteral("啦"));

    // Loading merges, so an override in the profile replaces one entry and keeps the others.
    QJsonObject profile;
    profile.insert(QStringLiteral("defaultLyrics"),
                   QJsonObject{{QStringLiteral("eng"), QStringLiteral("ah")}});
    option.load(profile);
    QCOMPARE(option.defaultLyricForLanguage(QStringLiteral("eng")), QStringLiteral("ah"));
    QCOMPARE(option.defaultLyricForLanguage(QStringLiteral("jpn")), QStringLiteral("ら"));
    const auto resaved = option.value().value(QStringLiteral("defaultLyrics")).toObject();
    QCOMPARE(resaved.size(), 4);
    QCOMPARE(resaved.value(QStringLiteral("eng")).toString(), QStringLiteral("ah"));
    QCOMPARE(resaved.value(QStringLiteral("jpn")).toString(), QStringLiteral("ら"));
}
