#include "Model/AppOptions/Options/AutomationOption.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
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
        success &= expect(option.controlPort == 0, QStringLiteral("control port should default to 0"));
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
        option.controlPort = 65535;
        option.selectedProfile = AutomationOption::Profile::Custom;
        option.setCustomPermissionEnabled(QStringLiteral("notes.get"), true);
        option.setCustomPermissionEnabled(QStringLiteral("documents.save"), false);
        option.readRoots = {QStringLiteral("D:/inputs"), QStringLiteral("D:/shared")};
        option.writeRoots = {QStringLiteral("D:/outputs")};

        AutomationOption reloaded;
        reloaded.load(option.value());
        bool success = expect(reloaded.mcpEnabled, QStringLiteral("MCP setting should round-trip"));
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
        success &= expect(option.controlPort == 0,
                          QStringLiteral("out-of-range port should use default 0"));
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

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    bool success = true;
    success &= testDefaults();
    success &= testRoundTrip();
    success &= testInvalidValuesUseSafeDefaults();
    success &= testProfileConversion();
    return success ? 0 : 1;
}
