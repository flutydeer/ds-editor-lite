#include "tst_foundation.h"

#include <lite/Support/LocalizedTextUtils.h>

#include <QtTest/QTest>

using LocalizedNames = QMap<QString, QString>;

void FoundationTests::localizedTextLookup_data() {
    QTest::addColumn<LocalizedNames>("names");
    QTest::addColumn<QStringList>("candidates");
    QTest::addColumn<QString>("expected");
    const LocalizedNames names{
        {QStringLiteral("ja"),      QStringLiteral("Japanese")   },
        {QStringLiteral("zh-Hans"), QStringLiteral("Simplified") },
        {QStringLiteral("zh-Hant"), QStringLiteral("Traditional")}
    };
    QTest::newRow("script-candidate-chain")
        << names << QStringList{"zh-Hans-CN", "zh-CN", "zh-Hans", "zh"}
        << QStringLiteral("Simplified");
    QTest::newRow("single-script-candidate")
        << names << QStringList{"zh-Hans-CN"} << QStringLiteral("Simplified");
    QTest::newRow("default-for-unsupported-language")
        << names << QStringList{"en-US", "en"} << QStringLiteral("Default");
    QTest::newRow("region-falls-back-to-language")
        << names << QStringList{"ja-JP", "ja"} << QStringLiteral("Japanese");
    QTest::newRow("region-does-not-imply-script")
        << LocalizedNames{{"zh-Hant", "Traditional"}} << QStringList{"zh-TW"}
        << QStringLiteral("Default");
    QTest::newRow("script-does-not-imply-region")
        << LocalizedNames{{"zh-TW", "Taiwan"}} << QStringList{"zh-Hant"}
        << QStringLiteral("Default");
    QTest::newRow("case-insensitive")
        << LocalizedNames{{"Zh-Cn", "Chinese"}} << QStringList{"zh-cn"}
        << QStringLiteral("Chinese");
    QTest::newRow("legacy-posix-tag")
        << LocalizedNames{{"zh_CN", "Chinese"}} << QStringList{"zh-CN"}
        << QStringLiteral("Chinese");
    QTest::newRow("legacy-posix-candidate-chain")
        << LocalizedNames{{"zh_CN", "Chinese"}}
        << QStringList{"zh-Hans-CN", "zh-CN", "zh-Hans", "zh"}
        << QStringLiteral("Chinese");
    QTest::newRow("empty-candidates") << names << QStringList{} << QStringLiteral("Default");
    QTest::newRow("empty-table") << LocalizedNames{} << QStringList{"zh"}
                                 << QStringLiteral("Default");
}

void FoundationTests::localizedTextLookup() {
    QFETCH(LocalizedNames, names);
    QFETCH(QStringList, candidates);
    QFETCH(QString, expected);
    QCOMPARE(lite::Support::lookupLocalizedText(names, QStringLiteral("Default"), candidates),
             expected);
}

void FoundationTests::localizedTextSingleTagOverload() {
    const LocalizedNames names{
        {QStringLiteral("zh_CN"), QStringLiteral("绮萱")}
    };
    QCOMPARE(lite::Support::lookupLocalizedText(names, QStringLiteral("Qixuan"),
                                                QStringLiteral("zh_CN")),
             QStringLiteral("绮萱"));
}
