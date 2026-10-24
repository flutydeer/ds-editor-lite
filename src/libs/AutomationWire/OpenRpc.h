#ifndef AUTOMATIONWIRE_OPENRPC_H
#define AUTOMATIONWIRE_OPENRPC_H

#include "PublicToolContract.h"

namespace AutomationWire {

    inline constexpr auto OpenRpcVersion = "1.4.1";
    inline constexpr auto OpenRpcDiscoverMethod = "rpc.discover";
    inline constexpr auto NativeRpcPath = "/automation/v1";
    inline constexpr auto OpenRpcDocumentPath = "/automation/v1/openrpc.json";

    struct OpenRpcDocumentInfo {
        QString title;
        QString version;
        QString endpoint;
    };

    std::optional<QJsonObject> makeOpenRpcDocument(const QList<ToolContract> &contracts,
                                                   const OpenRpcDocumentInfo &info,
                                                   QString *error = nullptr);

} // namespace AutomationWire

#endif // AUTOMATIONWIRE_OPENRPC_H
