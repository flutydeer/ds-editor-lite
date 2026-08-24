#ifndef MCPREQUESTDISPATCHER_H
#define MCPREQUESTDISPATCHER_H

#include <lite/AutomationWire/McpProtocol.h>
#include <lite/AutomationWire/OpaqueCursorCodec.h>

#include <QJsonObject>

namespace Automation {

    class PublicAutomationRegistry;

    class McpRequestDispatcher final {
    public:
        explicit McpRequestDispatcher(
            PublicAutomationRegistry &registry,
            AutomationWire::Mcp::ImplementationInfo serverInfo);

        [[nodiscard]] QJsonObject dispatch(
            const AutomationWire::Mcp::RequestEnvelope &request,
            const QString &clientId) const;

    private:
        [[nodiscard]] QJsonObject dispatchToolsList(
            const AutomationWire::Mcp::RequestEnvelope &request) const;
        [[nodiscard]] QJsonObject dispatchToolsCall(
            const AutomationWire::Mcp::RequestEnvelope &request,
            const QString &clientId) const;

        PublicAutomationRegistry &m_registry;
        AutomationWire::Mcp::ImplementationInfo m_serverInfo;
        AutomationWire::OpaqueCursorCodec m_toolsCursorCodec;
    };

} // namespace Automation

#endif // MCPREQUESTDISPATCHER_H
