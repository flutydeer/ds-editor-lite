#include "tst_preferences.h"

#include "Model/AppOptions/Options/WindowOption.h"

#include <QtTest/QTest>

void PreferencesTests::windowOptionRoundTrip() {
    const QByteArray mainGeometry = QByteArrayLiteral("main-window-geometry");
    const QByteArray detachedGeometry = QByteArrayLiteral("detached-panel-geometry");

    WindowOption option;
    option.setMainWindowGeometry(mainGeometry);
    option.setDetachedPanelGeometry(detachedGeometry);

    const auto saved = option.value();
    QCOMPARE(saved.value(QStringLiteral("mainWindowGeometry")).toString(),
             QString::fromLatin1(mainGeometry.toBase64()));
    QCOMPARE(saved.value(QStringLiteral("detachedPanelGeometry")).toString(),
             QString::fromLatin1(detachedGeometry.toBase64()));

    WindowOption reopened;
    reopened.load(saved);
    QCOMPARE(reopened.mainWindowGeometry(), mainGeometry);
    QCOMPARE(reopened.detachedPanelGeometry(), detachedGeometry);

    // An absent key must clear the field instead of keeping a stale value.
    reopened.load({});
    QVERIFY(reopened.mainWindowGeometry().isEmpty());
    QVERIFY(reopened.detachedPanelGeometry().isEmpty());
}

void PreferencesTests::windowOptionRejectsDamagedGeometry() {
    WindowOption option;
    option.load(QJsonObject{
        {QStringLiteral("mainWindowGeometry"), QStringLiteral("not-base64")},
        {QStringLiteral("detachedPanelGeometry"), QStringLiteral("AA!!")},
    });
    QVERIFY(option.mainWindowGeometry().isEmpty());
    QVERIFY(option.detachedPanelGeometry().isEmpty());
}
