#include <lite/Support/LocalizedTextUtils.h>

#include <QMap>
#include <QString>
#include <QStringList>
#include <QDebug>

namespace {
    int g_failures = 0;

    void expect(bool ok, const char *what) {
        if (!ok) {
            ++g_failures;
            qWarning() << "FAIL:" << what;
        } else {
            qInfo() << "ok:" << what;
        }
    }

    void expectText(const QMap<QString, QString> &localized, const QString &defaultText,
                    const QStringList &candidates, const QString &expected, const char *what) {
        const auto actual = lite::Support::lookupLocalizedText(localized, defaultText, candidates);
        expect(actual == expected, what);
        if (actual != expected)
            qWarning() << "   expected" << expected << "got" << actual;
    }

    void expectTextSingle(const QMap<QString, QString> &localized, const QString &defaultText,
                          const QString &lazyTag, const QString &expected, const char *what) {
        const auto actual = lite::Support::lookupLocalizedText(localized, defaultText, lazyTag);
        expect(actual == expected, what);
        if (actual != expected)
            qWarning() << "   expected" << expected << "got" << actual;
    }
}

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    // junninghua-style data (valid BCP 47 keys).
    const QMap<QString, QString> sungNames{
        {QStringLiteral("ja"),      QStringLiteral("\u3046\u308d\u3053\u97f3\u51dd\u83ef")},
        {QStringLiteral("zh-Hans"), QStringLiteral("\u541b\u51dd\u534e")                  },
        {QStringLiteral("zh-Hant"), QStringLiteral("\u541b\u51dd\u83ef")                  }
    };
    const QString defaultName(QStringLiteral("Jun Ninghua"));

    // Full Qt-style candidate chain must reach zh-Hans.
    expectText(sungNames, defaultName,
               {QStringLiteral("zh-Hans-CN"), QStringLiteral("zh-CN"), QStringLiteral("zh-Hans"),
                QStringLiteral("zh")},
               QStringLiteral("\u541b\u51dd\u534e"), "zh-Hans-CN candidate hits zh-Hans key");
    // The first candidate alone (most complete uiLanguages entry) also works.
    expectText(sungNames, defaultName, {QStringLiteral("zh-Hans-CN")},
               QStringLiteral("\u541b\u51dd\u534e"), "single zh-Hans-CN candidate hits zh-Hans");
    // English UI falls back to default.
    expectText(sungNames, defaultName,
               {QStringLiteral("en-US"), QStringLiteral("en-International"), QStringLiteral("en")},
               defaultName, "English UI falls back to default text");
    // Japanese candidate hits the ja key.
    expectText(sungNames, defaultName, {QStringLiteral("ja-JP"), QStringLiteral("ja")},
               QStringLiteral("\u3046\u308d\u3053\u97f3\u51dd\u83ef"), "ja-JP hits ja key");

    // Case-insensitive matching.
    const QMap<QString, QString> mixed{
        {QStringLiteral("EN"),    QStringLiteral("EN-name")},
        {QStringLiteral("Zh-Cn"), QStringLiteral("cn-name")}
    };
    expectText(mixed, QString(), {QStringLiteral("zh-cn")}, QStringLiteral("cn-name"),
               "case-insensitive tag match");

    // POSIX-style keys never match, even under an equivalent request.
    const QMap<QString, QString> posix{
        {QStringLiteral("zh_CN"), QStringLiteral("\u7ee3\u8431")}
    };
    expectText(posix, QStringLiteral("Qixuan"), {QStringLiteral("zh-CN")}, QStringLiteral("Qixuan"),
               "POSIX key zh_CN is skipped, falls back to default");
    expectText(posix, QStringLiteral("Qixuan"),
               {QStringLiteral("zh-Hans-CN"), QStringLiteral("zh-CN"), QStringLiteral("zh-Hans"),
                QStringLiteral("zh")},
               QStringLiteral("Qixuan"), "candidate chain does not match POSIX key");

    // Empty candidates / empty table.
    expectText(sungNames, defaultName, {}, defaultName, "empty candidates -> default");
    expectText({}, defaultName, {QStringLiteral("zh")}, defaultName, "empty table -> default");

    // Single-tag overload agrees with the list overload.
    expectTextSingle(sungNames, defaultName, QStringLiteral("zh-Hant"),
                     QStringLiteral("\u541b\u51dd\u83ef"), "single-tag overload zh-Hant");
    expectTextSingle(posix, QStringLiteral("Qixuan"), QStringLiteral("zh_CN"),
                     QStringLiteral("Qixuan"), "single-tag overload never matches POSIX key");

    // hasLocalizedTexts: POSIX-only keys do not count as usable translations.
    expect(lite::Support::hasLocalizedTexts(sungNames), "hasLocalizedTexts true for BCP 47 keys");
    expect(!lite::Support::hasLocalizedTexts(posix), "hasLocalizedTexts false for POSIX-only keys");
    expect(!lite::Support::hasLocalizedTexts({}), "hasLocalizedTexts false for empty table");

    if (g_failures == 0) {
        qInfo() << "TestLocalizedText: ALL PASSED";
        return 0;
    }
    qInfo() << "TestLocalizedText:" << g_failures << "FAILURE(S)";
    return 1;
}
