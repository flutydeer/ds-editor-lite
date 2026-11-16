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
    void rejectsSiblingScriptRegionCrossMatch();
    void keepsParentFallbackBehindSibling();
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
    // The explicit chain handles zh-Hans-CN -> zh-Hans, a fallback missing
    // from uloc_acceptLanguage in the Windows SDK's ICU 64.2.
    const QStringList available{QStringLiteral("ja"), QStringLiteral("zh-Hans"),
                                QStringLiteral("zh-Hant")};
    QCOMPARE(IcuWrapper::bestMatch(QStringLiteral("zh-Hans-CN"), available),
             QStringLiteral("zh-Hans"));
    QCOMPARE(IcuWrapper::bestMatch(QStringLiteral("zh-Hans-CN"),
                                   {QStringLiteral("zh"), QStringLiteral("zh-Hans")}),
             QStringLiteral("zh-Hans"));
}

void IcuWrapperTests::rejectsSiblingScriptRegionCrossMatch()
{
    // The parent chain excludes script and region siblings. ICU's
    // LocaleMatcher (67+) can collapse zh-TW and zh-Hant to zh-Hant-TW.
    QVERIFY(IcuWrapper::bestMatch(QStringLiteral("zh-TW"),
                                  {QStringLiteral("zh-Hant")}).isEmpty());
    QVERIFY(IcuWrapper::bestMatch(QStringLiteral("zh-Hant"),
                                  {QStringLiteral("zh-TW")}).isEmpty());
    // Region siblings are off the parent chain too (pt-BR's chain is
    // pt-BR -> pt and never contains pt-PT).
    QVERIFY(IcuWrapper::bestMatch(QStringLiteral("pt-BR"),
                                  {QStringLiteral("pt-PT")}).isEmpty());
    QVERIFY(IcuWrapper::bestMatch(QStringLiteral("pt-PT"),
                                  {QStringLiteral("pt-BR")}).isEmpty());
    // Positive control: an exact sibling key still wins when it exists.
    QCOMPARE(IcuWrapper::bestMatch(QStringLiteral("zh-TW"),
                                   {QStringLiteral("zh-Hant"),
                                    QStringLiteral("zh-TW")}),
             QStringLiteral("zh-TW"));
}

void IcuWrapperTests::keepsParentFallbackBehindSibling()
{
    // An off-chain sibling must not suppress an available parent.
    QCOMPARE(IcuWrapper::bestMatch(QStringLiteral("zh-TW"),
                                   {QStringLiteral("zh-Hant"),
                                    QStringLiteral("zh")}),
             QStringLiteral("zh"));
    QCOMPARE(IcuWrapper::bestMatch(QStringLiteral("pt-BR"),
                                   {QStringLiteral("pt-PT"),
                                    QStringLiteral("pt")}),
             QStringLiteral("pt"));
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
