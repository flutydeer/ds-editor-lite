#ifndef AUTOMATION_MCP_MCPCLIENTCONFIGURATION_H
#define AUTOMATION_MCP_MCPCLIENTCONFIGURATION_H

#include "Model/AppOptions/Options/AutomationOption.h"

#include <QString>
#include <QStringList>

namespace Automation::McpClientConfiguration {

    [[nodiscard]] QString connectorExecutablePath(const QString &applicationDirectory);
    [[nodiscard]] QStringList connectorArguments(AutomationOption::ControlLevel controlLevel,
                                                 QStringList enabledCustomOperations = {});
    [[nodiscard]] QString stdioJson(const QString &command, const QStringList &arguments);
    [[nodiscard]] QString streamableHttpJson(const QString &endpoint);

} // namespace Automation::McpClientConfiguration

#endif // AUTOMATION_MCP_MCPCLIENTCONFIGURATION_H
