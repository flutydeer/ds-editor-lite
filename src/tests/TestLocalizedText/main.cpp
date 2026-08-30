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
        {QStringLiteral("ja"),      QStringLiteral("ろこ音凝華")},
        {QStringLiteral("zh-Hans"), QStringLiteral("君凝华")                  },
        {QStringLiteral("zh-Hant"), QStringLiteral("君凝華")                  }
    };
    const QString defaultName(QStringLiteral("Jun Ninghua"));

    // Full Qt-style candidate chain must reach zh-Hans.
    expectText(sungNames, defaultName,
               {QStringLiteral("zh-Hans-CN"), QStringLiteral("zh-CN"), QStringLiteral("zh-Hans"),
                QStringLiteral("zh")},
               QStringLiteral("君凝华"), "zh-Hans-CN candidate hits zh-Hans key");
    // The first candidate alone (most complete uiLanguages entry) also works.
    expectText(sungNames, defaultName, {QStringLiteral("zh-Hans-CN")},
               QStringLiteral("君凝华"), "single zh-Hans-CN candidate hits zh-Hans");
    // English UI falls back to default.
    expectText(sungNames, defaultName,
               {QStringLiteral("en-US"), QStringLiteral("en-International"), QStringLiteral("en")},
               defaultName, "English UI falls back to default text");
    // Japanese candidate hits the ja key.
    expectText(sungNames, defaultName, {QStringLiteral("ja-JP"), QStringLiteral("ja")},
               QStringLiteral("ろこ音凝華"), "ja-JP hits ja key");
    // Script subtags do not cross-match via region truncation (zh-Hant request
    // must not hit a zh-TW key, and vice versa).
    const QMap<QString, QString> hantOnly{
        {QStringLiteral("zh-Hant"), QStringLiteral("華-name")}
    };
    expectText(hantOnly, QStringLiteral("def"), {QStringLiteral("zh-TW")}, QStringLiteral("def"),
               "zh-TW request does not hit zh-Hant key");
    const QMap<QString, QString> twOnly{
        {QStringLiteral("zh-TW"), QStringLiteral("tw-name")}
    };
    expectText(twOnly, QStringLiteral("def"), {QStringLiteral("zh-Hant")}, QStringLiteral("def"),
               "zh-Hant request does not hit zh-TW key");

    // Case-insensitive matching (ICU normalization).
    const QMap<QString, QString> mixed{
        {QStringLiteral("EN"),    QStringLiteral("EN-name")},
        {QStringLiteral("Zh-Cn"), QStringLiteral("cn-name")}
    };
    expectText(mixed, QString(), {QStringLiteral("zh-cn")}, QStringLiteral("cn-name"),
               "case-insensitive tag match");

    // POSIX-style keys match after normalization and keep their original
    // spelling for the exact map fetch (ds-spec 2.4: keys are opaque, the
    // frontend owns matching; legacy packages resume showing translations).
    const QMap<QString, QString> posix{
        {QStringLiteral("zh_CN"), QStringLiteral("绣萱")}
    };
    expectText(posix, QStringLiteral("Qixuan"), {QStringLiteral("zh-CN")}, QStringLiteral("绣萱"),
               "POSIX key zh_CN hits zh-CN request");
    expectText(posix, QStringLiteral("Qixuan"),
               {QStringLiteral("zh-Hans-CN"), QStringLiteral("zh-CN"), QStringLiteral("zh-Hans"),
                QStringLiteral("zh")},
               QStringLiteral("绣萱"), "candidate chain hits POSIX key");

    // Empty candidates / empty table.
    expectText(sungNames, defaultName, {}, defaultName, "empty candidates -> default");
    expectText({}, defaultName, {QStringLiteral("zh")}, defaultName, "empty table -> default");

    // Single-tag overload agrees with the list overload.
    expectTextSingle(sungNames, defaultName, QStringLiteral("zh-Hant"),
                     QStringLiteral("君凝華"), "single-tag overload zh-Hant");
    expectTextSingle(posix, QStringLiteral("Qixuan"), QStringLiteral("zh_CN"),
                     QStringLiteral("绣萱"), "single-tag overload hits POSIX key");

    if (g_failures == 0) {
        qInfo() << "TestLocalizedText: ALL PASSED";
        return 0;
    }
    qInfo() << "TestLocalizedText:" << g_failures << "FAILURE(S)";
    return 1;
}
