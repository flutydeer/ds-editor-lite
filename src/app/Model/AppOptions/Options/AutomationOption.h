#ifndef AUTOMATIONOPTION_H
#define AUTOMATIONOPTION_H

#include "Model/AppOptions/IOption.h"

#include <QMap>
#include <QStringList>

#include <optional>

class AutomationOption final : public IOption {
public:
    enum class ControlLevel {
        L1,
        L2,
        L3,
        Custom,
    };

    static constexpr quint16 kRandomControlPortMinimum = 49152;
    static constexpr quint16 kRandomControlPortMaximum = 65535;

    explicit AutomationOption();

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;

    [[nodiscard]] bool customPermissionEnabled(const QString &operationId) const;
    void setCustomPermissionEnabled(const QString &operationId, bool enabled);

    [[nodiscard]] static QString controlLevelToString(ControlLevel level);
    [[nodiscard]] static std::optional<ControlLevel> controlLevelFromString(const QString &value);
    [[nodiscard]] static quint16 generateRandomControlPort(quint16 previousPort = 0);

    bool mcpEnabled = false;
    quint16 controlPort;
    ControlLevel controlLevel = ControlLevel::L1;
    QMap<QString, bool> customPermissions;
    QStringList accessRoots;
};

#endif // AUTOMATIONOPTION_H
