#include "AutomationOption.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QStandardPaths>

#include <cmath>

namespace {

    constexpr auto kMcpEnabledKey = "mcpEnabled";
    constexpr auto kControlPortKey = "controlPort";
    constexpr auto kSelectedProfileKey = "selectedProfile";
    constexpr auto kCustomPermissionsKey = "customPermissions";
    constexpr auto kReadRootsKey = "readRoots";
    constexpr auto kWriteRootsKey = "writeRoots";

    QStringList defaultFileRoots() {
        const auto documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        const QFileInfo documentsInfo(documents);
        if (documents.isEmpty() || !documentsInfo.isDir())
            return {};
        const auto canonicalPath = documentsInfo.canonicalFilePath();
        if (canonicalPath.isEmpty())
            return {};
        return {QDir::cleanPath(QDir::fromNativeSeparators(canonicalPath))};
    }

    QStringList loadStringList(const QJsonValue &value) {
        QStringList result;
        if (!value.isArray())
            return result;

        const auto values = value.toArray();
        result.reserve(values.size());
        for (const auto &item : values) {
            if (!item.isString() || item.toString().isEmpty())
                continue;
            result.append(item.toString());
        }
        return result;
    }

} // namespace

AutomationOption::AutomationOption() : IOption("automation"), controlPort(generateRandomControlPort()) {
    readRoots = defaultFileRoots();
    writeRoots = readRoots;
}

void AutomationOption::load(const QJsonObject &object) {
    mcpEnabled = false;
    controlPort = generateRandomControlPort(controlPort);
    selectedProfile = Profile::L1;
    customPermissions.clear();
    const auto defaultRoots = defaultFileRoots();
    readRoots = object.contains(QLatin1String(kReadRootsKey))
                    ? loadStringList(object.value(QLatin1String(kReadRootsKey)))
                    : defaultRoots;
    writeRoots = object.contains(QLatin1String(kWriteRootsKey))
                     ? loadStringList(object.value(QLatin1String(kWriteRootsKey)))
                     : defaultRoots;

    const auto enabledValue = object.value(QLatin1String(kMcpEnabledKey));
    if (enabledValue.isBool())
        mcpEnabled = enabledValue.toBool();

    const auto portValue = object.value(QLatin1String(kControlPortKey));
    if (portValue.isDouble()) {
        const auto numericPort = portValue.toDouble();
        if (std::floor(numericPort) == numericPort && numericPort >= 1 && numericPort <= 65535)
            controlPort = static_cast<quint16>(numericPort);
    }
    if (const auto profile =
            profileFromString(object.value(QLatin1String(kSelectedProfileKey)).toString())) {
        selectedProfile = *profile;
    }

    const auto permissionsValue = object.value(QLatin1String(kCustomPermissionsKey));
    if (permissionsValue.isObject()) {
        const auto permissions = permissionsValue.toObject();
        for (auto it = permissions.constBegin(); it != permissions.constEnd(); ++it) {
            if (!it.key().isEmpty() && it.value().isBool())
                customPermissions.insert(it.key(), it.value().toBool());
        }
    }
}

void AutomationOption::save(QJsonObject &object) {
    QJsonObject permissions;
    for (auto it = customPermissions.constBegin(); it != customPermissions.constEnd(); ++it)
        permissions.insert(it.key(), it.value());

    object = {
        {QLatin1String(kMcpEnabledKey),        mcpEnabled                            },
        {QLatin1String(kControlPortKey),       static_cast<int>(controlPort)         },
        {QLatin1String(kSelectedProfileKey),   profileToString(selectedProfile)      },
        {QLatin1String(kCustomPermissionsKey), permissions                           },
        {QLatin1String(kReadRootsKey),         QJsonArray::fromStringList(readRoots) },
        {QLatin1String(kWriteRootsKey),        QJsonArray::fromStringList(writeRoots)},
    };
}

bool AutomationOption::customPermissionEnabled(const QString &operationId) const {
    return customPermissions.value(operationId, false);
}

void AutomationOption::setCustomPermissionEnabled(const QString &operationId, const bool enabled) {
    if (!operationId.isEmpty())
        customPermissions.insert(operationId, enabled);
}

QString AutomationOption::profileToString(const Profile profile) {
    switch (profile) {
        case Profile::L2:
            return QStringLiteral("l2");
        case Profile::L3:
            return QStringLiteral("l3");
        case Profile::Custom:
            return QStringLiteral("custom");
        case Profile::L1:
        default:
            return QStringLiteral("l1");
    }
}

std::optional<AutomationOption::Profile> AutomationOption::profileFromString(const QString &value) {
    if (value == QStringLiteral("l1"))
        return Profile::L1;
    if (value == QStringLiteral("l2"))
        return Profile::L2;
    if (value == QStringLiteral("l3"))
        return Profile::L3;
    if (value == QStringLiteral("custom"))
        return Profile::Custom;
    return std::nullopt;
}

quint16 AutomationOption::generateRandomControlPort(const quint16 previousPort) {
    quint16 port;
    do {
        port = static_cast<quint16>(QRandomGenerator::system()->bounded(
            static_cast<quint32>(kRandomControlPortMinimum),
            static_cast<quint32>(kRandomControlPortMaximum) + 1));
    } while (port == previousPort);
    return port;
}
