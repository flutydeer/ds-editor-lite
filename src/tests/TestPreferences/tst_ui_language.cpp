#include "tst_preferences.h"

#include "Model/AppOptions/Options/GeneralOption.h"
#include "Utils/ApplicationLocale.h"
#include "Utils/UiLanguageManager.h"

#include <QtTest/QTest>
#include <QSignalSpy>
#include <QJsonObject>

void PreferencesTests::uiLanguageEffectiveLanguage_data() {
    QTest::addColumn<QString>("preference");
    QTest::addColumn<QLocale>("systemLocale");
    QTest::addColumn<QString>("expected");
    QTest::newRow("simplified-system")
        << UiLanguageManager::System << QLocale(QLocale::Chinese, QLocale::China)
        << UiLanguageManager::SimplifiedChinese;
    QTest::newRow("traditional-system")
        << UiLanguageManager::System << QLocale(QLocale::Chinese, QLocale::Taiwan)
        << UiLanguageManager::SimplifiedChinese;
    QTest::newRow("hong-kong-system")
        << UiLanguageManager::System << QLocale(QLocale::Chinese, QLocale::HongKong)
        << UiLanguageManager::SimplifiedChinese;
    QTest::newRow("unsupported-system")
        << UiLanguageManager::System << QLocale(QLocale::German, QLocale::Germany)
        << UiLanguageManager::English;
    QTest::newRow("explicit-english")
        << UiLanguageManager::English << QLocale(QLocale::Chinese, QLocale::China)
        << UiLanguageManager::English;
    QTest::newRow("explicit-chinese")
        << UiLanguageManager::SimplifiedChinese << QLocale(QLocale::English, QLocale::UnitedStates)
        << UiLanguageManager::SimplifiedChinese;
    QTest::newRow("invalid-preference")
        << QStringLiteral("invalid") << QLocale(QLocale::Chinese, QLocale::China)
        << UiLanguageManager::SimplifiedChinese;
}

void PreferencesTests::uiLanguageEffectiveLanguage() {
    QFETCH(QString, preference);
    QFETCH(QLocale, systemLocale);
    QFETCH(QString, expected);
    QCOMPARE(UiLanguageManager::resolveEffectiveLanguageId(preference, systemLocale), expected);
}

void PreferencesTests::uiLanguageNumberFormattingPreservesDecimalSeparator() {
    auto source = QLocale(QLocale::French, QLocale::France);
    source.setNumberOptions(QLocale::RejectGroupSeparator);
    const auto locale = ApplicationLocale::configuredLocale(source);
    QCOMPARE(locale.name(), source.name());
    QCOMPARE(locale.toString(1234), QStringLiteral("1234"));
    QCOMPARE(locale.toString(12.5, 'f', 1), QStringLiteral("12,5"));
    QVERIFY(locale.numberOptions().testFlag(QLocale::OmitGroupSeparator));
    QVERIFY(locale.numberOptions().testFlag(QLocale::RejectGroupSeparator));
}

void PreferencesTests::uiLanguagePreferencePersistence() {
    GeneralOption option;
    option.load(QJsonObject{
        {QStringLiteral("uiLanguage"), QStringLiteral("invalid")}
    });
    QCOMPARE(option.uiLanguage, UiLanguageManager::System);
    option.uiLanguage = UiLanguageManager::English;
    QCOMPARE(option.value().value(QStringLiteral("uiLanguage")).toString(),
             UiLanguageManager::English);
}

void PreferencesTests::uiLanguageEquivalentPreferenceDoesNotNotify() {
    UiLanguageManager manager;
    manager.setPreference(UiLanguageManager::English);
    QSignalSpy changed(&manager, &UiLanguageManager::languageChanged);
    manager.setPreference(UiLanguageManager::English);
    QCOMPARE(manager.preference(), UiLanguageManager::English);
    QCOMPARE(manager.effectiveLanguageId(), UiLanguageManager::English);
    QVERIFY(changed.isEmpty());
}
