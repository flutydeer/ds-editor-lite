#include <IcuWrapper/IcuWrapper.h>

#include <QtTest/QTest>

class IcuWrapperTests : public QObject
{
    Q_OBJECT

private slots:
    void rejectsEmptyInput();
    void preservesOriginalValue();
    void matchesLanguageFallback();
    void matchesScriptFallback();
    void rejectsUnrelatedLanguage();
    void skipsInvalidAvailableTags();
};

void IcuWrapperTests::rejectsEmptyInput()
{
    QVERIFY(IcuWrapper::bestMatch({}, {QStringLiteral("en")}).isEmpty());
    QVERIFY(IcuWrapper::bestMatch(QStringLiteral("en"), {}).isEmpty());
}

void IcuWrapperTests::preservesOriginalValue()
{
    const QStringList available{QStringLiteral("en_US.UTF-8"),
                                QStringLiteral("zh-Hans")};
    QCOMPARE(IcuWrapper::bestMatch(QStringLiteral(" en-US@calendar=gregorian "),
                                   available),
             QStringLiteral("en_US.UTF-8"));
}

void IcuWrapperTests::matchesLanguageFallback()
{
    const QStringList available{QStringLiteral("fr"), QStringLiteral("de")};
    QCOMPARE(IcuWrapper::bestMatch(QStringLiteral("fr-CA"), available),
             QStringLiteral("fr"));
}

void IcuWrapperTests::matchesScriptFallback()
{
    // Regression: the Windows SDK umbrella ICU (64.2) does not fall back
    // across script+region combos (zh_Hans_CN -> zh_Hans) inside
    // uloc_acceptLanguage; the explicit fallback chain must cover this.
    const QStringList available{QStringLiteral("ja"), QStringLiteral("zh-Hans"),
                                QStringLiteral("zh-Hant")};
    QCOMPARE(IcuWrapper::bestMatch(QStringLiteral("zh-Hans-CN"), available),
             QStringLiteral("zh-Hans"));
    QCOMPARE(IcuWrapper::bestMatch(QStringLiteral("zh-Hans-CN"),
                                   {QStringLiteral("zh"), QStringLiteral("zh-Hans")}),
             QStringLiteral("zh-Hans"));
}

void IcuWrapperTests::rejectsUnrelatedLanguage()
{
    QVERIFY(IcuWrapper::bestMatch(QStringLiteral("ja-JP"),
                                  {QStringLiteral("en-US"),
                                   QStringLiteral("de-DE")}).isEmpty());
}

void IcuWrapperTests::skipsInvalidAvailableTags()
{
    QCOMPARE(IcuWrapper::bestMatch(QStringLiteral("de-DE"),
                                   {QStringLiteral("not_a_tag!"),
                                    QStringLiteral("de")}),
             QStringLiteral("de"));
}

QTEST_APPLESS_MAIN(IcuWrapperTests)

#include "IcuWrapperTests.moc"
