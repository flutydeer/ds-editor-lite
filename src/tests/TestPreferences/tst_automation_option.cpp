#include "tst_preferences.h"

#include "Model/AppOptions/Options/AutomationOption.h"
#include "Automation/Mcp/McpClientConfiguration.h"

#include <QCoreApplication>
#include <QtTest>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

void PreferencesTests::automationOptionDefaults() {
    AutomationOption option;
    const auto documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QFileInfo documentsInfo(documents);
    const auto canonicalDocuments = documents.isEmpty() || !documentsInfo.isDir()
                                        ? QString{}
                                        : documentsInfo.canonicalFilePath();
    const auto defaultRoots =
        canonicalDocuments.isEmpty()
            ? QStringList{}
            : QStringList{QDir::cleanPath(QDir::fromNativeSeparators(canonicalDocuments))};
    QVERIFY2((!option.mcpEnabled), qPrintable(QStringLiteral("MCP should default to disabled")));
    QVERIFY2((option.controlPort >= AutomationOption::kRandomControlPortMinimum &&
              option.controlPort <= AutomationOption::kRandomControlPortMaximum),
             qPrintable(
                 QStringLiteral("the initial configuration should have a concrete private port")));
    QVERIFY2((option.controlLevel == AutomationOption::ControlLevel::L1),
             qPrintable(QStringLiteral("control level should default to L1")));
    QVERIFY2((!option.customPermissionEnabled(QStringLiteral("notes.list"))),
             qPrintable(QStringLiteral("unknown Custom permissions should default to disabled")));
    QVERIFY2((option.accessRoots == defaultRoots),
             qPrintable(QStringLiteral("file roots should default to the user documents folder")));

    AutomationOption loaded;
    loaded.load({});
    QVERIFY2((loaded.accessRoots == defaultRoots),
             qPrintable(QStringLiteral("missing file root settings should use defaults")));

    AutomationOption cleared;
    cleared.load(QJsonObject{
        {QStringLiteral("accessRoots"), QJsonArray{}}
    });
    QVERIFY2((cleared.accessRoots.isEmpty()),
             qPrintable(QStringLiteral("explicitly empty file roots should remain empty")));
}

void PreferencesTests::automationOptionRoundTrip() {
    AutomationOption option;
    option.mcpEnabled = true;
    option.controlPort = 65535;
    option.controlLevel = AutomationOption::ControlLevel::Custom;
    option.setCustomPermissionEnabled(QStringLiteral("notes.list"), true);
    option.setCustomPermissionEnabled(QStringLiteral("documents.save"), false);
    option.accessRoots = {QStringLiteral("D:/inputs"), QStringLiteral("D:/outputs")};

    AutomationOption reloaded;
    reloaded.load(option.value());
    QVERIFY2((reloaded.mcpEnabled), qPrintable(QStringLiteral("MCP setting should round-trip")));
    QVERIFY2((reloaded.controlPort == 65535),
             qPrintable(QStringLiteral("maximum control port should round-trip")));
    QVERIFY2((reloaded.controlLevel == AutomationOption::ControlLevel::Custom),
             qPrintable(QStringLiteral("Custom control level should round-trip")));
    QVERIFY2((reloaded.customPermissionEnabled(QStringLiteral("notes.list"))),
             qPrintable(QStringLiteral("enabled Custom permission should round-trip")));
    QVERIFY2((reloaded.customPermissions.contains(QStringLiteral("documents.save")) &&
              !reloaded.customPermissionEnabled(QStringLiteral("documents.save"))),
             qPrintable(QStringLiteral("explicitly disabled Custom permission should round-trip")));
    QVERIFY2((!reloaded.customPermissionEnabled(QStringLiteral("new.operation"))),
             qPrintable(QStringLiteral("new operations should stay disabled in Custom")));
    QVERIFY2((reloaded.accessRoots == option.accessRoots),
             qPrintable(QStringLiteral("file roots should round-trip")));
}

void PreferencesTests::automationOptionInvalidValuesUseSafeDefaults() {
    AutomationOption option;
    option.mcpEnabled = true;
    option.controlPort = 42;
    option.controlLevel = AutomationOption::ControlLevel::L2;
    option.customPermissions.insert(QStringLiteral("old.operation"), true);
    option.accessRoots = {QStringLiteral("D:/old")};

    option.load(QJsonObject{
        {QStringLiteral("mcpEnabled"),        QStringLiteral("true")                               },
        {QStringLiteral("controlPortMode"),   QStringLiteral("invalid")                            },
        {QStringLiteral("controlPort"),       65536                                                },
        {QStringLiteral("controlLevel"),      QStringLiteral("L2")                                 },
        {QStringLiteral("customPermissions"),
         QJsonObject{{QStringLiteral("valid.false"), false},
                     {QStringLiteral("invalid"), QStringLiteral("true")}}                          },
        {QStringLiteral("accessRoots"),       QJsonArray{QStringLiteral("D:/input"), 12, QString{}}},
    });

    QVERIFY2((!option.mcpEnabled),
             qPrintable(QStringLiteral("invalid MCP value should use disabled default")));
    QVERIFY2((option.controlPort >= AutomationOption::kRandomControlPortMinimum &&
              option.controlPort <= AutomationOption::kRandomControlPortMaximum),
             qPrintable(QStringLiteral("invalid port settings should generate a concrete port")));
    QVERIFY2((option.controlLevel == AutomationOption::ControlLevel::L1),
             qPrintable(QStringLiteral("invalid control level should use L1")));
    QVERIFY2((option.customPermissions.size() == 1 &&
              option.customPermissions.contains(QStringLiteral("valid.false"))),
             qPrintable(QStringLiteral("only boolean Custom permissions should load")));
    QVERIFY2((option.accessRoots == QStringList{QStringLiteral("D:/input")}),
             qPrintable(QStringLiteral("only non-empty roots in arrays should load")));
}

void PreferencesTests::automationOptionControlLevelConversion_data() {
    QTest::addColumn<int>("level");
    QTest::addColumn<QString>("serialized");
    QTest::newRow("l1") << int(AutomationOption::ControlLevel::L1) << QStringLiteral("l1");
    QTest::newRow("l2") << int(AutomationOption::ControlLevel::L2) << QStringLiteral("l2");
    QTest::newRow("l3") << int(AutomationOption::ControlLevel::L3) << QStringLiteral("l3");
    QTest::newRow("custom") << int(AutomationOption::ControlLevel::Custom)
                            << QStringLiteral("custom");
}

void PreferencesTests::automationOptionControlLevelConversion() {
    QFETCH(int, level);
    QFETCH(QString, serialized);
    const auto value = static_cast<AutomationOption::ControlLevel>(level);
    QCOMPARE(AutomationOption::controlLevelToString(value), serialized);
    QVERIFY(AutomationOption::controlLevelFromString(serialized) == value);
}

void PreferencesTests::automationOptionControlPortInput_data() {
    QTest::addColumn<QJsonValue>("input");
    QTest::addColumn<int>("expectedPort");
    QTest::newRow("minimum") << QJsonValue(1) << 1;
    QTest::newRow("maximum") << QJsonValue(65535) << 65535;
    QTest::newRow("zero") << QJsonValue(0) << 0;
    QTest::newRow("negative") << QJsonValue(-1) << 0;
    QTest::newRow("overflow") << QJsonValue(65536) << 0;
    QTest::newRow("fraction") << QJsonValue(18231.5) << 0;
    QTest::newRow("text") << QJsonValue(QStringLiteral("18231")) << 0;
    QTest::newRow("boolean") << QJsonValue(true) << 0;
    QTest::newRow("null") << QJsonValue(QJsonValue::Null) << 0;
}

void PreferencesTests::automationOptionControlPortInput() {
    QFETCH(QJsonValue, input);
    QFETCH(int, expectedPort);
    AutomationOption option;
    option.load({
        {QStringLiteral("controlPort"), input}
    });
    if (expectedPort) {
        QCOMPARE(int(option.controlPort), expectedPort);
    } else {
        QVERIFY(option.controlPort >= AutomationOption::kRandomControlPortMinimum);
        QVERIFY(option.controlPort <= AutomationOption::kRandomControlPortMaximum);
    }
    AutomationOption restored;
    restored.load(option.value());
    QCOMPARE(restored.controlPort, option.controlPort);
}

void PreferencesTests::automationOptionInvalidPermissionAndLevelCannotBeEnabled() {
    AutomationOption option;
    option.setCustomPermissionEnabled(QString{}, true);
    QVERIFY(option.customPermissions.isEmpty());
    option.load({
        {QStringLiteral("controlLevel"),      QStringLiteral("l0")                             },
        {QStringLiteral("customPermissions"),
         QJsonObject{{QString{}, true}, {QStringLiteral("notes.list"), QStringLiteral("true")}}}
    });
    QCOMPARE(option.controlLevel, AutomationOption::ControlLevel::L1);
    QVERIFY(option.customPermissions.isEmpty());
    QVERIFY(!option.customPermissionEnabled(QStringLiteral("notes.list")));
}

void PreferencesTests::automationOptionStableGeneratedControlPort() {
    AutomationOption generated;
    generated.load(QJsonObject{
        {QStringLiteral("controlPort"),     0                       },
        {QStringLiteral("controlPortMode"), QStringLiteral("random")}
    });
    QVERIFY2(
        (generated.controlPort >= AutomationOption::kRandomControlPortMinimum &&
         generated.controlPort <= AutomationOption::kRandomControlPortMaximum),
        qPrintable(QStringLiteral("missing or legacy zero ports should generate a concrete port")));

    const auto saved = generated.value();
    QVERIFY2((!saved.contains(QStringLiteral("controlPortMode"))),
             qPrintable(QStringLiteral("the removed port mode must not be persisted")));
    AutomationOption reloaded;
    reloaded.load(saved);
    QVERIFY2((reloaded.controlPort == generated.controlPort),
             qPrintable(QStringLiteral("the generated port should stay stable after persistence")));

    AutomationOption existing;
    existing.load(QJsonObject{
        {QStringLiteral("controlPort"), 18231}
    });
    QVERIFY2((existing.controlPort == 18231),
             qPrintable(QStringLiteral("an existing non-zero port should remain unchanged")));

    const auto refreshed = AutomationOption::generateRandomControlPort(generated.controlPort);
    QVERIFY2((refreshed != generated.controlPort &&
              refreshed >= AutomationOption::kRandomControlPortMinimum &&
              refreshed <= AutomationOption::kRandomControlPortMaximum),
             qPrintable(QStringLiteral("refresh should produce a different concrete port")));
}

void PreferencesTests::automationOptionMcpClientConfigurations() {
    using namespace Automation::McpClientConfiguration;

    QVERIFY2(
        (connectorArguments(AutomationOption::ControlLevel::L2) ==
         QStringList{QStringLiteral("--control-level"), QStringLiteral("l2")}),
        qPrintable(QStringLiteral("L2 connector arguments should match the editor control level")));
    QVERIFY2(
        (connectorArguments(AutomationOption::ControlLevel::Custom,
                            {QStringLiteral("notes.list"), QStringLiteral("documents.save"),
                             QStringLiteral("notes.list")}) ==
         QStringList{QStringLiteral("--control-level"), QStringLiteral("l0"),
                     QStringLiteral("--include-tool=id:documents.save"),
                     QStringLiteral("--include-tool=id:notes.list")}),
        qPrintable(QStringLiteral("Custom connector arguments should be sorted and deduplicated")));

    const auto command = connectorExecutablePath(QStringLiteral("C:/Program Files/DS Editor Lite"));
    auto connectorSuffix = QStringLiteral("/DsConnectorLite");
#ifdef Q_OS_WIN
    connectorSuffix += QStringLiteral(".exe");
#endif
    QVERIFY2((QDir::fromNativeSeparators(command).endsWith(connectorSuffix)),
             qPrintable(QStringLiteral("connector path should use product metadata")));

    const auto stdio = QJsonDocument::fromJson(
        stdioJson(command, connectorArguments(AutomationOption::ControlLevel::L1)).toUtf8());
    const auto stdioServer = stdio.object();
    const auto stdioCommand = stdioServer.value(QStringLiteral("command")).toString();
    QVERIFY2((stdioServer.value(QStringLiteral("type")).toString() == QStringLiteral("stdio") &&
              stdioCommand == QDir::fromNativeSeparators(command) &&
              !stdioCommand.contains(QLatin1Char('\\')) &&
              stdioServer.value(QStringLiteral("args")).toArray().size() == 2),
             qPrintable(QStringLiteral("STDIO JSON should contain command and arguments")));

    const auto endpoint = QStringLiteral("http://127.0.0.1:18231/mcp");
    const auto http = QJsonDocument::fromJson(streamableHttpJson(endpoint).toUtf8());
    const auto httpServer = http.object();
    QVERIFY2(
        (httpServer.value(QStringLiteral("type")).toString() == QStringLiteral("streamable-http") &&
         httpServer.value(QStringLiteral("url")).toString() == endpoint),
        qPrintable(QStringLiteral("Streamable HTTP JSON should contain the current endpoint")));
}
