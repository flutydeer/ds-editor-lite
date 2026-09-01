#include "Model/AppOptions/Options/AutomationOption.h"
#include "Automation/Mcp/McpClientConfiguration.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {

    bool expect(const bool condition, const QString &message) {
        if (condition)
            return true;
        qCritical().noquote() << message;
        return false;
    }

    bool testDefaults() {
        AutomationOption option;
        const auto documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        const QFileInfo documentsInfo(documents);
        const auto canonicalDocuments =
            documents.isEmpty() || !documentsInfo.isDir() ? QString{}
                                                          : documentsInfo.canonicalFilePath();
        const auto defaultRoots = canonicalDocuments.isEmpty()
                                      ? QStringList{}
                                      : QStringList{QDir::cleanPath(
                                            QDir::fromNativeSeparators(canonicalDocuments))};
        bool success = expect(!option.mcpEnabled, QStringLiteral("MCP should default to disabled"));
        success &= expect(option.controlPort >= AutomationOption::kRandomControlPortMinimum &&
                              option.controlPort <= AutomationOption::kRandomControlPortMaximum,
                          QStringLiteral("the initial configuration should have a concrete private port"));
        success &= expect(option.controlLevel == AutomationOption::ControlLevel::L1,
                          QStringLiteral("control level should default to L1"));
        success &= expect(!option.customPermissionEnabled(QStringLiteral("notes.list")),
                          QStringLiteral("unknown Custom permissions should default to disabled"));
        success &= expect(option.accessRoots == defaultRoots,
                          QStringLiteral("file roots should default to the user documents folder"));

        AutomationOption loaded;
        loaded.load({});
        success &= expect(loaded.accessRoots == defaultRoots,
                          QStringLiteral("missing file root settings should use defaults"));

        AutomationOption cleared;
        cleared.load(QJsonObject{{QStringLiteral("accessRoots"), QJsonArray{}}});
        success &= expect(cleared.accessRoots.isEmpty(),
                          QStringLiteral("explicitly empty file roots should remain empty"));
        return success;
    }

    bool testRoundTrip() {
        AutomationOption option;
        option.mcpEnabled = true;
        option.controlPort = 65535;
        option.controlLevel = AutomationOption::ControlLevel::Custom;
        option.setCustomPermissionEnabled(QStringLiteral("notes.list"), true);
        option.setCustomPermissionEnabled(QStringLiteral("documents.save"), false);
        option.accessRoots = {QStringLiteral("D:/inputs"), QStringLiteral("D:/outputs")};

        AutomationOption reloaded;
        reloaded.load(option.value());
        bool success = expect(reloaded.mcpEnabled, QStringLiteral("MCP setting should round-trip"));
        success &= expect(reloaded.controlPort == 65535,
                          QStringLiteral("maximum control port should round-trip"));
        success &= expect(reloaded.controlLevel == AutomationOption::ControlLevel::Custom,
                          QStringLiteral("Custom control level should round-trip"));
        success &= expect(reloaded.customPermissionEnabled(QStringLiteral("notes.list")),
                          QStringLiteral("enabled Custom permission should round-trip"));
        success &= expect(reloaded.customPermissions.contains(QStringLiteral("documents.save")) &&
                              !reloaded.customPermissionEnabled(QStringLiteral("documents.save")),
                          QStringLiteral("explicitly disabled Custom permission should round-trip"));
        success &= expect(!reloaded.customPermissionEnabled(QStringLiteral("new.operation")),
                          QStringLiteral("new operations should stay disabled in Custom"));
        success &= expect(reloaded.accessRoots == option.accessRoots,
                          QStringLiteral("file roots should round-trip"));
        return success;
    }

    bool testInvalidValuesUseSafeDefaults() {
        AutomationOption option;
        option.mcpEnabled = true;
        option.controlPort = 42;
        option.controlLevel = AutomationOption::ControlLevel::L2;
        option.customPermissions.insert(QStringLiteral("old.operation"), true);
        option.accessRoots = {QStringLiteral("D:/old")};

        option.load(QJsonObject{
            {QStringLiteral("mcpEnabled"),       QStringLiteral("true")                 },
            {QStringLiteral("controlPortMode"),  QStringLiteral("invalid")              },
            {QStringLiteral("controlPort"),      65536                                  },
            {QStringLiteral("controlLevel"),  QStringLiteral("L2")                  },
            {QStringLiteral("customPermissions"),
             QJsonObject{{QStringLiteral("valid.false"), false},
                         {QStringLiteral("invalid"), QStringLiteral("true")}}          },
            {QStringLiteral("accessRoots"),
             QJsonArray{QStringLiteral("D:/input"), 12, QString{}}                      },
        });

        bool success = expect(!option.mcpEnabled,
                              QStringLiteral("invalid MCP value should use disabled default"));
        success &= expect(option.controlPort >= AutomationOption::kRandomControlPortMinimum &&
                              option.controlPort <= AutomationOption::kRandomControlPortMaximum,
                          QStringLiteral("invalid port settings should generate a concrete port"));
        success &= expect(option.controlLevel == AutomationOption::ControlLevel::L1,
                          QStringLiteral("invalid control level should use L1"));
        success &= expect(option.customPermissions.size() == 1 &&
                              option.customPermissions.contains(QStringLiteral("valid.false")),
                          QStringLiteral("only boolean Custom permissions should load"));
        success &= expect(option.accessRoots == QStringList{QStringLiteral("D:/input")},
                          QStringLiteral("only non-empty roots in arrays should load"));
        return success;
    }

    bool testControlLevelConversion() {
        bool success = true;
        const QList<AutomationOption::ControlLevel> levels = {
            AutomationOption::ControlLevel::L1,
            AutomationOption::ControlLevel::L2,
            AutomationOption::ControlLevel::L3,
            AutomationOption::ControlLevel::Custom,
        };
        for (const auto level : levels) {
            const auto serialized = AutomationOption::controlLevelToString(level);
            success &= expect(AutomationOption::controlLevelFromString(serialized) == level,
                              QStringLiteral("control level conversion should round-trip: %1")
                                  .arg(serialized));
        }
        success &= expect(!AutomationOption::controlLevelFromString(QStringLiteral("l0")),
                          QStringLiteral(
                              "L0 is intrinsic rather than a selectable editor control level"));
        return success;
    }

    bool testStableGeneratedControlPort() {
        AutomationOption generated;
        generated.load(QJsonObject{{QStringLiteral("controlPort"), 0},
                                   {QStringLiteral("controlPortMode"),
                                    QStringLiteral("random")}});
        bool success = expect(
            generated.controlPort >= AutomationOption::kRandomControlPortMinimum &&
                generated.controlPort <= AutomationOption::kRandomControlPortMaximum,
            QStringLiteral("missing or legacy zero ports should generate a concrete port"));

        const auto saved = generated.value();
        success &= expect(!saved.contains(QStringLiteral("controlPortMode")),
                          QStringLiteral("the removed port mode must not be persisted"));
        AutomationOption reloaded;
        reloaded.load(saved);
        success &= expect(reloaded.controlPort == generated.controlPort,
                          QStringLiteral("the generated port should stay stable after persistence"));

        AutomationOption existing;
        existing.load(QJsonObject{{QStringLiteral("controlPort"), 18231}});
        success &= expect(existing.controlPort == 18231,
                          QStringLiteral("an existing non-zero port should remain unchanged"));

        const auto refreshed = AutomationOption::generateRandomControlPort(generated.controlPort);
        success &= expect(refreshed != generated.controlPort &&
                              refreshed >= AutomationOption::kRandomControlPortMinimum &&
                              refreshed <= AutomationOption::kRandomControlPortMaximum,
                          QStringLiteral("refresh should produce a different concrete port"));
        return success;
    }

    bool testMcpClientConfigurations() {
        using namespace Automation::McpClientConfiguration;

        bool success = true;
        success &= expect(connectorArguments(AutomationOption::ControlLevel::L2) ==
                              QStringList{QStringLiteral("--control-level"),
                                          QStringLiteral("l2")},
                          QStringLiteral(
                              "L2 connector arguments should match the editor control level"));
        success &= expect(
            connectorArguments(AutomationOption::ControlLevel::Custom,
                               {QStringLiteral("notes.list"), QStringLiteral("documents.save"),
                                QStringLiteral("notes.list")}) ==
                QStringList{QStringLiteral("--control-level"), QStringLiteral("l0"),
                            QStringLiteral("--include-tool=id:documents.save"),
                            QStringLiteral("--include-tool=id:notes.list")},
            QStringLiteral("Custom connector arguments should be sorted and deduplicated"));

        const auto command = connectorExecutablePath(QStringLiteral("C:/Program Files/DS Editor Lite"));
        auto connectorSuffix = QStringLiteral("/DsConnectorLite");
#ifdef Q_OS_WIN
        connectorSuffix += QStringLiteral(".exe");
#endif
        success &= expect(QDir::fromNativeSeparators(command).endsWith(connectorSuffix),
                          QStringLiteral("connector path should use product metadata"));

        const auto stdio = QJsonDocument::fromJson(
            stdioJson(command, connectorArguments(AutomationOption::ControlLevel::L1)).toUtf8());
        const auto stdioServer = stdio.object();
        const auto stdioCommand = stdioServer.value(QStringLiteral("command")).toString();
        success &= expect(stdioServer.value(QStringLiteral("type")).toString() ==
                                  QStringLiteral("stdio") &&
                              stdioCommand == QDir::fromNativeSeparators(command) &&
                              !stdioCommand.contains(QLatin1Char('\\')) &&
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
    success &= testStableGeneratedControlPort();
    success &= testControlLevelConversion();
    success &= testMcpClientConfigurations();
    return success ? 0 : 1;
}
