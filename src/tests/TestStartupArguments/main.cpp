#include "Bootstrap/StartupArguments.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QTemporaryDir>

namespace {

    bool expect(const bool condition, const QString &message) {
        if (condition)
            return true;
        qCritical().noquote() << message;
        return false;
    }

    bool testValidArguments(const QString &workingDirectory) {
        const auto parsed = StartupArguments::parseArguments(
            {QStringLiteral("--mcp"), QStringLiteral("--control-port=65535"),
             QStringLiteral("--automation-profile"), QStringLiteral("custom"),
             QStringLiteral("song.dspx")},
            workingDirectory);
        bool success = expect(parsed.isValid(), QStringLiteral("valid arguments should parse"));
        success &= expect(parsed.automation.mcpEnabled == true,
                          QStringLiteral("--mcp should enable the runtime override"));
        success &= expect(parsed.automation.controlPort == 65535,
                          QStringLiteral("control port should parse at its upper bound"));
        success &= expect(parsed.automation.profile == AutomationOption::Profile::Custom,
                          QStringLiteral("Custom profile should parse"));
        success &= expect(
            parsed.projectFilePaths ==
                QStringList{QDir::cleanPath(QDir(workingDirectory).absoluteFilePath("song.dspx"))},
            QStringLiteral("automation flags and their values must not become project paths"));
        return success;
    }

    bool testPortAndProfileBounds() {
        bool success = true;
        auto parsed = StartupArguments::parseArguments(
            {QStringLiteral("--control-port"), QStringLiteral("1"),
             QStringLiteral("--automation-profile=l3")});
        success &= expect(parsed.isValid() && parsed.automation.controlPort == 1 &&
                              parsed.automation.profile == AutomationOption::Profile::L3,
                          QStringLiteral("the minimum port and L3 should be accepted"));

        parsed = StartupArguments::parseArguments(
            {QStringLiteral("--control-port"), QStringLiteral("random")});
        success &= expect(!parsed.isValid() &&
                              parsed.error->code == StartupArguments::ParseErrorCode::InvalidValue,
                          QStringLiteral("the removed Random mode should fail clearly"));

        parsed = StartupArguments::parseArguments(
            {QStringLiteral("--control-port"), QStringLiteral("0")});
        success &= expect(!parsed.isValid() &&
                              parsed.error->code == StartupArguments::ParseErrorCode::InvalidValue,
                          QStringLiteral("port 0 should no longer represent Random mode"));

        parsed = StartupArguments::parseArguments(
            {QStringLiteral("--control-port"), QStringLiteral("65536")});
        success &= expect(!parsed.isValid() &&
                              parsed.error->code == StartupArguments::ParseErrorCode::InvalidValue,
                          QStringLiteral("ports above 65535 should fail clearly"));

        parsed = StartupArguments::parseArguments(
            {QStringLiteral("--control-port"), QStringLiteral("-1")});
        success &= expect(!parsed.isValid() &&
                              parsed.error->code == StartupArguments::ParseErrorCode::InvalidValue,
                          QStringLiteral("negative ports should fail clearly"));

        parsed = StartupArguments::parseArguments(
            {QStringLiteral("--automation-profile"), QStringLiteral("L2")});
        success &= expect(!parsed.isValid() &&
                              parsed.error->code == StartupArguments::ParseErrorCode::InvalidValue,
                          QStringLiteral("profile values should use the documented lowercase form"));
        return success;
    }

    bool testMissingAndConflictingOptions() {
        bool success = true;
        auto parsed = StartupArguments::parseArguments({QStringLiteral("--control-port")});
        success &= expect(!parsed.isValid() &&
                              parsed.error->code == StartupArguments::ParseErrorCode::MissingValue &&
                              parsed.error->option == QStringLiteral("--control-port"),
                          QStringLiteral("missing control port should identify its option"));

        parsed = StartupArguments::parseArguments(
            {QStringLiteral("--automation-profile"), QStringLiteral("--mcp")});
        success &= expect(!parsed.isValid() &&
                              parsed.error->code == StartupArguments::ParseErrorCode::MissingValue,
                          QStringLiteral("a following flag is not a profile value"));

        parsed = StartupArguments::parseArguments(
            {QStringLiteral("--mcp"), QStringLiteral("--no-mcp")});
        success &= expect(
            !parsed.isValid() &&
                parsed.error->code == StartupArguments::ParseErrorCode::ConflictingOptions,
            QStringLiteral("--mcp and --no-mcp should conflict"));

        parsed = StartupArguments::parseArguments(
            {QStringLiteral("--control-port=1"), QStringLiteral("--control-port=2")});
        success &= expect(
            !parsed.isValid() &&
                parsed.error->code == StartupArguments::ParseErrorCode::ConflictingOptions,
            QStringLiteral("different repeated ports should conflict"));

        parsed = StartupArguments::parseArguments(
            {QStringLiteral("--control-port=18231"),
             QStringLiteral("--control-port=18231")});
        success &= expect(parsed.isValid() && parsed.automation.controlPort == 18231,
                          QStringLiteral("repeating the same concrete port should be accepted"));

        parsed = StartupArguments::parseArguments(
            {QStringLiteral("--automation-profile=l1"),
             QStringLiteral("--automation-profile=l2")});
        success &= expect(
            !parsed.isValid() &&
                parsed.error->code == StartupArguments::ParseErrorCode::ConflictingOptions,
            QStringLiteral("different repeated profiles should conflict"));
        return success;
    }

    bool testUnknownOptionAndDelimiter(const QString &workingDirectory) {
        auto parsed = StartupArguments::parseArguments({QStringLiteral("--unknown")});
        bool success = expect(!parsed.isValid() &&
                                  parsed.error->code ==
                                      StartupArguments::ParseErrorCode::UnknownOption,
                              QStringLiteral("unknown options should fail clearly"));

        parsed = StartupArguments::parseArguments(
            {QStringLiteral("--"), QStringLiteral("--mcp")}, workingDirectory);
        success &= expect(parsed.isValid() && parsed.automation.isEmpty() &&
                              parsed.projectFilePaths.size() == 1 &&
                              parsed.projectFilePaths.constFirst() ==
                                  QDir::cleanPath(
                                      QDir(workingDirectory).absoluteFilePath("--mcp")),
                          QStringLiteral("the delimiter should allow dash-prefixed project paths"));
        return success;
    }

    bool testEffectiveConfigDoesNotMutatePersistence() {
        AutomationOption persisted;
        persisted.mcpEnabled = false;
        persisted.controlPort = 1234;
        persisted.selectedProfile = AutomationOption::Profile::L1;
        persisted.setCustomPermissionEnabled(QStringLiteral("notes.get"), true);

        StartupArguments::AutomationOverrides overrides;
        overrides.mcpEnabled = true;
        overrides.controlPort = 4321;
        overrides.profile = AutomationOption::Profile::Custom;
        const auto effective =
            StartupArguments::effectiveAutomationConfig(persisted, overrides);

        bool success = expect(effective.mcpEnabled && overrides.controlPort &&
                                  effective.controlPort == *overrides.controlPort &&
                                  effective.profile == AutomationOption::Profile::Custom,
                              QStringLiteral("CLI values should win in the effective config"));
        success &= expect(
            effective.mcpEnabledSource == StartupArguments::ConfigSource::CommandLine &&
                effective.controlPortSource == StartupArguments::ConfigSource::CommandLine &&
                effective.profileSource == StartupArguments::ConfigSource::CommandLine,
            QStringLiteral("effective config should expose command-line sources"));
        success &= expect(!persisted.mcpEnabled && persisted.controlPort == 1234 &&
                              persisted.selectedProfile == AutomationOption::Profile::L1 &&
                              persisted.customPermissionEnabled(QStringLiteral("notes.get")),
                          QStringLiteral("resolving CLI overrides must not modify saved settings"));
        return success;
    }

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    QTemporaryDir workingDirectory;
    if (!expect(workingDirectory.isValid(), QStringLiteral("temporary directory should exist")))
        return 1;

    bool success = true;
    success &= testValidArguments(workingDirectory.path());
    success &= testPortAndProfileBounds();
    success &= testMissingAndConflictingOptions();
    success &= testUnknownOptionAndDelimiter(workingDirectory.path());
    success &= testEffectiveConfigDoesNotMutatePersistence();
    return success ? 0 : 1;
}
