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

void IcuWrapperTests::rejectsSiblingScriptRegionCrossMatch()
{
    // Regression: zh-TW and zh-Hant maximize to the same likely-subtag form
    // (zh_Hant_TW), so the LocaleMatcher-based uloc_acceptLanguage in ICU
    // >= 67 cross-matches them (verified at ICU source level for 67.1,
    // 68.2, 70.1 and 74.2). Script/region siblings must never cross-match
    // on any platform.
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
