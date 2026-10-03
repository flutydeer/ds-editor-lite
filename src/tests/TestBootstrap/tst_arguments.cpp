#include "tst_bootstrap.h"
#include "Bootstrap/StartupArguments.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

void BootstrapTests::invalidArguments_data() {
    using Error = StartupArguments::ParseErrorCode;
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<int>("error");
    QTest::newRow("random-port") << QStringList{"--control-port", "random"}
                                 << int(Error::InvalidValue);
    QTest::newRow("zero-port") << QStringList{"--control-port", "0"} << int(Error::InvalidValue);
    QTest::newRow("overflow-port")
        << QStringList{"--control-port", "65536"} << int(Error::InvalidValue);
    QTest::newRow("negative-port")
        << QStringList{"--control-port", "-1"} << int(Error::InvalidValue);
    QTest::newRow("uppercase-level")
        << QStringList{"--control-level", "L2"} << int(Error::InvalidValue);
    QTest::newRow("missing-port") << QStringList{"--control-port"} << int(Error::MissingValue);
    QTest::newRow("flag-as-value")
        << QStringList{"--control-level", "--mcp"} << int(Error::MissingValue);
    QTest::newRow("conflicting-mcp")
        << QStringList{"--mcp", "--no-mcp"} << int(Error::ConflictingOptions);
    QTest::newRow("conflicting-port")
        << QStringList{"--control-port=1", "--control-port=2"} << int(Error::ConflictingOptions);
    QTest::newRow("conflicting-level") << QStringList{"--control-level=l1", "--control-level=l2"}
                                       << int(Error::ConflictingOptions);
    QTest::newRow("unknown-option") << QStringList{"--unknown"} << int(Error::UnknownOption);
}

void BootstrapTests::invalidArguments() {
    QFETCH(QStringList, arguments);
    QFETCH(int, error);
    const auto parsed = StartupArguments::parseArguments(arguments);
    QVERIFY(!parsed.isValid());
    QVERIFY(parsed.error);
    QCOMPARE(int(parsed.error->code), error);
}

void BootstrapTests::validPort_data() {
    QTest::addColumn<int>("port");
    QTest::newRow("lower-bound") << 1;
    QTest::newRow("upper-bound") << 65535;
}

void BootstrapTests::validPort() {
    QFETCH(int, port);
    const auto parsed = StartupArguments::parseArguments(
        {QStringLiteral("--control-port=%1").arg(port), "--control-level=l3"});
    QVERIFY(parsed.isValid());
    QCOMPARE(parsed.automation.controlPort, std::optional<quint16>(port));
    QCOMPARE(parsed.automation.controlLevel, std::optional(AutomationOption::ControlLevel::L3));
}

void BootstrapTests::flagsDoNotBecomeProjectPaths() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto parsed = StartupArguments::parseArguments(
        {"--headless", "--mcp", "--control-port=65535", "--control-level", "custom", "song.dspx"},
        directory.path());
    QVERIFY(parsed.isValid());
    QCOMPARE(parsed.hostMode, AppHostMode::Headless);
    QCOMPARE(parsed.automation.mcpEnabled, std::optional(true));
    QCOMPARE(parsed.automation.controlLevel, std::optional(AutomationOption::ControlLevel::Custom));
    QCOMPARE(parsed.projectFilePaths, QStringList{directory.filePath("song.dspx")});
}

void BootstrapTests::delimiterAndPreparseAgree() {
    char executable[] = "editor";
    char headless[] = "--headless";
    char delimiter[] = "--";
    char *headlessArguments[] = {executable, headless};
    QCOMPARE(StartupArguments::preparseHostMode(2, headlessArguments), AppHostMode::Headless);
    char *positionalArguments[] = {executable, delimiter, headless};
    QCOMPARE(StartupArguments::preparseHostMode(3, positionalArguments), AppHostMode::Gui);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto parsed = StartupArguments::parseArguments({"--", "--headless"}, directory.path());
    QVERIFY(parsed.isValid());
    QVERIFY(parsed.automation.isEmpty());
    QCOMPARE(parsed.hostMode, AppHostMode::Gui);
    QCOMPARE(parsed.projectFilePaths, QStringList{directory.filePath("--headless")});
}

void BootstrapTests::identicalOptionsCanRepeat() {
    const auto parsed = StartupArguments::parseArguments(
        {"--headless", "--headless", "--control-port=18231", "--control-port=18231"});
    QVERIFY(parsed.isValid());
    QCOMPARE(parsed.hostMode, AppHostMode::Headless);
    QCOMPARE(parsed.automation.controlPort, std::optional<quint16>(18231));
}

void BootstrapTests::runtimeOverridesDoNotMutatePersistence() {
    AutomationOption persisted;
    persisted.mcpEnabled = false;
    persisted.controlPort = 1234;
    persisted.controlLevel = AutomationOption::ControlLevel::L1;
    persisted.setCustomPermissionEnabled(QStringLiteral("notes.list"), true);
    StartupArguments::AutomationOverrides overrides;
    overrides.mcpEnabled = true;
    overrides.controlPort = 4321;
    overrides.controlLevel = AutomationOption::ControlLevel::Custom;
    const auto effective = StartupArguments::effectiveAutomationConfig(persisted, overrides);
    QVERIFY(effective.mcpEnabled);
    QCOMPARE(effective.controlPort, quint16(4321));
    QCOMPARE(effective.controlLevel, AutomationOption::ControlLevel::Custom);
    QCOMPARE(effective.mcpEnabledSource, StartupArguments::ConfigSource::CommandLine);
    QCOMPARE(effective.controlPortSource, StartupArguments::ConfigSource::CommandLine);
    QCOMPARE(effective.controlLevelSource, StartupArguments::ConfigSource::CommandLine);
    QVERIFY(!persisted.mcpEnabled);
    QCOMPARE(persisted.controlPort, quint16(1234));
    QCOMPARE(persisted.controlLevel, AutomationOption::ControlLevel::L1);
    QVERIFY(persisted.customPermissionEnabled(QStringLiteral("notes.list")));
}
