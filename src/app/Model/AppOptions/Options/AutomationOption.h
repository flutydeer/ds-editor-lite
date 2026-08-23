#ifndef AUTOMATIONOPTION_H
#define AUTOMATIONOPTION_H

#include "Model/AppOptions/IOption.h"

#include <QMap>
#include <QStringList>

#include <optional>

class AutomationOption final : public IOption {
public:
    enum class Profile {
        L1,
        L2,
        L3,
        Custom,
    };

    static constexpr quint16 kDefaultControlPort = 0;

    explicit AutomationOption() : IOption("automation") {
    }

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;

    [[nodiscard]] bool customPermissionEnabled(const QString &operationId) const;
    void setCustomPermissionEnabled(const QString &operationId, bool enabled);

    [[nodiscard]] static QString profileToString(Profile profile);
    [[nodiscard]] static std::optional<Profile> profileFromString(const QString &value);

    bool mcpEnabled = false;
    quint16 controlPort = kDefaultControlPort;
    Profile selectedProfile = Profile::L1;
    QMap<QString, bool> customPermissions;
    QStringList readRoots;
    QStringList writeRoots;
};

#endif // AUTOMATIONOPTION_H
