#include "Model/AppOptions/Options/AutomationOption.h"
#include "Automation/Mcp/McpClientConfiguration.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

    bool expect(const bool condition, const QString &message) {
        if (condition)
            return true;
        qCritical().noquote() << message;
        return false;
    }

    bool testDefaults() {
        AutomationOption option;
        bool success = expect(!option.mcpEnabled, QStringLiteral("MCP should default to disabled"));
        success &= expect(option.controlPortMode == AutomationOption::ControlPortMode::Random,
                          QStringLiteral("control port mode should default to Random"));
        success &= expect(option.controlPort >= AutomationOption::kRandomControlPortMinimum &&
                              option.controlPort <= AutomationOption::kRandomControlPortMaximum,
                          QStringLiteral("Random mode should have a concrete private port"));
        success &= expect(option.selectedProfile == AutomationOption::Profile::L1,
                          QStringLiteral("profile should default to L1"));
        success &= expect(!option.customPermissionEnabled(QStringLiteral("notes.get")),
                          QStringLiteral("unknown Custom permissions should default to disabled"));
        success &= expect(option.readRoots.isEmpty() && option.writeRoots.isEmpty(),
                          QStringLiteral("file roots should default to empty"));
        return success;
    }

    bool testRoundTrip() {
        AutomationOption option;
        option.mcpEnabled = true;
        option.controlPortMode = AutomationOption::ControlPortMode::Fixed;
        option.controlPort = 65535;
        option.selectedProfile = AutomationOption::Profile::Custom;
        option.setCustomPermissionEnabled(QStringLiteral("notes.get"), true);
        option.setCustomPermissionEnabled(QStringLiteral("documents.save"), false);
        option.readRoots = {QStringLiteral("D:/inputs"), QStringLiteral("D:/shared")};
        option.writeRoots = {QStringLiteral("D:/outputs")};

        AutomationOption reloaded;
        reloaded.load(option.value());
        bool success = expect(reloaded.mcpEnabled, QStringLiteral("MCP setting should round-trip"));
        success &= expect(reloaded.controlPortMode == AutomationOption::ControlPortMode::Fixed,
                          QStringLiteral("control port mode should round-trip"));
        success &= expect(reloaded.controlPort == 65535,
                          QStringLiteral("maximum control port should round-trip"));
        success &= expect(reloaded.selectedProfile == AutomationOption::Profile::Custom,
                          QStringLiteral("Custom profile should round-trip"));
        success &= expect(reloaded.customPermissionEnabled(QStringLiteral("notes.get")),
                          QStringLiteral("enabled Custom permission should round-trip"));
        success &= expect(reloaded.customPermissions.contains(QStringLiteral("documents.save")) &&
                              !reloaded.customPermissionEnabled(QStringLiteral("documents.save")),
                          QStringLiteral("explicitly disabled Custom permission should round-trip"));
        success &= expect(!reloaded.customPermissionEnabled(QStringLiteral("new.operation")),
                          QStringLiteral("new operations should stay disabled in Custom"));
        success &= expect(reloaded.readRoots == option.readRoots &&
                              reloaded.writeRoots == option.writeRoots,
                          QStringLiteral("file roots should round-trip"));
        return success;
    }

    bool testInvalidValuesUseSafeDefaults() {
        AutomationOption option;
        option.mcpEnabled = true;
        option.controlPort = 42;
        option.selectedProfile = AutomationOption::Profile::L2;
        option.customPermissions.insert(QStringLiteral("old.operation"), true);
        option.readRoots = {QStringLiteral("D:/old")};

        option.load(QJsonObject{
            {QStringLiteral("mcpEnabled"),       QStringLiteral("true")                 },
            {QStringLiteral("controlPortMode"),  QStringLiteral("invalid")              },
            {QStringLiteral("controlPort"),      65536                                  },
            {QStringLiteral("selectedProfile"),  QStringLiteral("L2")                  },
            {QStringLiteral("customPermissions"),
             QJsonObject{{QStringLiteral("valid.false"), false},
                         {QStringLiteral("invalid"), QStringLiteral("true")}}          },
            {QStringLiteral("readRoots"),
             QJsonArray{QStringLiteral("D:/input"), 12, QString{}}                      },
            {QStringLiteral("writeRoots"),       QStringLiteral("D:/not-an-array")     },
        });

        bool success = expect(!option.mcpEnabled,
                              QStringLiteral("invalid MCP value should use disabled default"));
        success &= expect(option.controlPortMode == AutomationOption::ControlPortMode::Random &&
                              option.controlPort >= AutomationOption::kRandomControlPortMinimum &&
                              option.controlPort <= AutomationOption::kRandomControlPortMaximum,
                          QStringLiteral("invalid port settings should use a concrete Random port"));
        success &= expect(option.selectedProfile == AutomationOption::Profile::L1,
                          QStringLiteral("invalid profile should use L1"));
        success &= expect(option.customPermissions.size() == 1 &&
                              option.customPermissions.contains(QStringLiteral("valid.false")),
                          QStringLiteral("only boolean Custom permissions should load"));
        success &= expect(option.readRoots == QStringList{QStringLiteral("D:/input")} &&
                              option.writeRoots.isEmpty(),
                          QStringLiteral("only non-empty roots in arrays should load"));
        return success;
    }

    bool testProfileConversion() {
        bool success = true;
        const QList<AutomationOption::Profile> profiles = {
            AutomationOption::Profile::L1,
            AutomationOption::Profile::L2,
            AutomationOption::Profile::L3,
            AutomationOption::Profile::Custom,
        };
        for (const auto profile : profiles) {
            const auto serialized = AutomationOption::profileToString(profile);
            success &= expect(AutomationOption::profileFromString(serialized) == profile,
                              QStringLiteral("profile conversion should round-trip: %1")
                                  .arg(serialized));
        }
        success &= expect(!AutomationOption::profileFromString(QStringLiteral("l0")),
                          QStringLiteral("L0 is not an editor profile"));
        return success;
    }

    bool testLegacyControlPortMigration() {
        AutomationOption random;
        random.load(QJsonObject{{QStringLiteral("controlPort"), 0}});
        bool success = expect(
            random.controlPortMode == AutomationOption::ControlPortMode::Random &&
                random.controlPort >= AutomationOption::kRandomControlPortMinimum,
            QStringLiteral("legacy port 0 should migrate to an explicit Random selection"));

        AutomationOption fixed;
        fixed.load(QJsonObject{{QStringLiteral("controlPort"), 18231}});
        success &= expect(fixed.controlPortMode == AutomationOption::ControlPortMode::Fixed &&
                              fixed.controlPort == 18231,
                          QStringLiteral("legacy non-zero ports should migrate to Fixed"));

        const auto refreshed = AutomationOption::generateRandomControlPort(random.controlPort);
        success &= expect(refreshed != random.controlPort &&
                              refreshed >= AutomationOption::kRandomControlPortMinimum &&
                              refreshed <= AutomationOption::kRandomControlPortMaximum,
                          QStringLiteral("refresh should produce a different concrete Random port"));
        return success;
    }

    bool testMcpClientConfigurations() {
        using namespace Automation::McpClientConfiguration;

        bool success = true;
        success &= expect(connectorArguments(AutomationOption::Profile::L2) ==
                              QStringList{QStringLiteral("--exposure-profile"),
                                          QStringLiteral("l2")},
                          QStringLiteral("L2 connector arguments should match the editor profile"));
        success &= expect(
            connectorArguments(AutomationOption::Profile::Custom,
                               {QStringLiteral("notes.get"), QStringLiteral("documents.save"),
                                QStringLiteral("notes.get")}) ==
                QStringList{QStringLiteral("--exposure-profile"), QStringLiteral("l0"),
                            QStringLiteral("--include-tool"),
                            QStringLiteral("id:documents.save"),
                            QStringLiteral("--include-tool"), QStringLiteral("id:notes.get")},
            QStringLiteral("Custom connector arguments should be sorted and deduplicated"));

        const auto command = connectorExecutablePath(QStringLiteral("C:/Program Files/DS Editor Lite"));
        auto connectorSuffix = QStringLiteral("/DsConnectorLite");
#ifdef Q_OS_WIN
        connectorSuffix += QStringLiteral(".exe");
#endif
        success &= expect(QDir::fromNativeSeparators(command).endsWith(connectorSuffix),
                          QStringLiteral("connector path should use product metadata"));

        const auto stdio = QJsonDocument::fromJson(
            stdioJson(command, connectorArguments(AutomationOption::Profile::L1)).toUtf8());
        const auto stdioServer = stdio.object();
        success &= expect(stdioServer.value(QStringLiteral("type")).toString() ==
                                  QStringLiteral("stdio") &&
                              stdioServer.value(QStringLiteral("command")).toString() == command &&
                              stdioServer.value(QStringLiteral("args")).toArray().size() == 2,
                          QStringLiteral("STDIO JSON should contain command and arguments"));

        const auto endpoint = QStringLiteral("http://127.0.0.1:18231/mcp");
        const auto http = QJsonDocument::fromJson(streamableHttpJson(endpoint).toUtf8());
        const auto httpServer = http.object();
        success &= expect(httpServer.value(QStringLiteral("type")).toString() ==
                                  QStringLiteral("streamable-http") &&
                              httpServer.value(QStringLiteral("url")).toString() == endpoint,
                          QStringLiteral("Streamable HTTP JSON should contain the current endpoint"));
        return success;
    }

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    bool success = true;
    success &= testDefaults();
    success &= testRoundTrip();
    success &= testInvalidValuesUseSafeDefaults();
    success &= testLegacyControlPortMigration();
    success &= testProfileConversion();
    success &= testMcpClientConfigurations();
    return success ? 0 : 1;
}
