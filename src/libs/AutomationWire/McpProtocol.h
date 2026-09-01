#ifndef AUTOMATIONWIRE_MCPPROTOCOL_H
#define AUTOMATIONWIRE_MCPPROTOCOL_H

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include <optional>

namespace AutomationWire::Mcp {

    inline constexpr auto CompatibilityProtocolVersion = "2025-06-18";
    inline constexpr auto LegacyProtocolVersion = "2025-11-25";
    inline constexpr auto ProtocolVersion = "2026-07-28";
    inline constexpr auto JsonRpcVersion = "2.0";
    inline constexpr qsizetype MaximumRequestIdCodeUnits = 1024;
    inline constexpr qsizetype MaximumMethodCodeUnits = 256;
    inline constexpr qsizetype MaximumToolNameCodeUnits = 256;
    inline constexpr qsizetype MaximumProtocolVersionCodeUnits = 64;
    inline constexpr qsizetype MaximumImplementationFieldCodeUnits = 4096;

    inline constexpr auto ProtocolVersionMetaKey = "io.modelcontextprotocol/protocolVersion";
    inline constexpr auto ClientCapabilitiesMetaKey = "io.modelcontextprotocol/clientCapabilities";
    inline constexpr auto ClientInfoMetaKey = "io.modelcontextprotocol/clientInfo";
    inline constexpr auto ServerInfoMetaKey = "io.modelcontextprotocol/serverInfo";
    inline constexpr auto ConnectorInstanceIdMetaKey =
        "org.openvpi.ds-editor-lite/connectorInstanceId";

    inline constexpr auto InitializeMethod = "initialize";
    inline constexpr auto InitializedNotification = "notifications/initialized";
    inline constexpr auto CancelledNotification = "notifications/cancelled";
    inline constexpr auto PingMethod = "ping";
    inline constexpr auto DiscoverMethod = "server/discover";
    inline constexpr auto ToolsListMethod = "tools/list";
    inline constexpr auto ToolsCallMethod = "tools/call";

    enum ErrorCode {
        ParseError = -32700,
        InvalidRequest = -32600,
        MethodNotFound = -32601,
        InvalidParams = -32602,
        InternalError = -32603,
        ServerNotInitialized = -32002,
        HeaderMismatch = -32020,
        MissingRequiredClientCapability = -32021,
        UnsupportedProtocolVersion = -32022,
    };

    struct ImplementationInfo {
        QString name;
        QString version;
        QString description;
        QString websiteUrl;

        bool valid() const;
        QJsonObject toJson() const;
        static std::optional<ImplementationInfo> fromJson(const QJsonValue &value,
                                                          QString *errorMessage = nullptr);
    };

    struct RequestContext {
        QString protocolVersion = QString::fromLatin1(ProtocolVersion);
        QJsonObject clientCapabilities;
        std::optional<ImplementationInfo> clientInfo;
    };

    struct ProtocolError {
        int code = InvalidRequest;
        QString message;
        QJsonValue data = QJsonValue(QJsonValue::Undefined);

        QJsonObject toJson() const;
    };

    struct RequestEnvelope {
        QJsonValue id = QJsonValue(QJsonValue::Undefined);
        bool notification = false;
        QString method;
        QJsonObject params;
        QJsonObject meta;
        QString protocolVersion;
        QJsonObject clientCapabilities;
        std::optional<ImplementationInfo> clientInfo;
        QString name;
    };

    struct RequestValidationResult {
        std::optional<RequestEnvelope> request;
        ProtocolError error;

        bool valid() const {
            return request.has_value();
        }
    };

    struct ResponseEnvelope {
        QJsonValue id = QJsonValue(QJsonValue::Undefined);
        QJsonObject result;
        std::optional<ProtocolError> error;
    };

    struct ResponseValidationResult {
        std::optional<ResponseEnvelope> response;
        ProtocolError error;

        bool valid() const {
            return response.has_value();
        }
    };

    struct TransportMetadata {
        std::optional<QString> protocolVersion;
        std::optional<QString> method;
        std::optional<QString> name;
    };

    struct MetadataValidationResult {
        std::optional<ProtocolError> error;

        bool valid() const {
            return !error.has_value();
        }
    };

    bool isValidRequestId(const QJsonValue &id);
    bool isSupportedCoreMethod(const QString &method);
    bool isLegacyProtocolVersion(const QString &version);
    bool isModernProtocolVersion(const QString &version);
    QStringList supportedProtocolVersions();
    QStringList modernProtocolVersions();

    QJsonObject makeRequest(const QString &method, QJsonObject params,
                            const RequestContext &context,
                            const QJsonValue &id = QJsonValue(QJsonValue::Undefined));
    QJsonObject makeInitializeRequest(const RequestContext &context,
                                      const QJsonValue &id = QJsonValue(QJsonValue::Undefined));
    RequestValidationResult parseRequest(const QJsonValue &message,
                                         const QString &impliedProtocolVersion = {});
    RequestValidationResult
        validateRequest(const QJsonValue &message,
                        const QStringList &supportedVersions = supportedProtocolVersions(),
                        const QString &impliedProtocolVersion = {});
    ResponseValidationResult
        validateResponse(const QJsonValue &message,
                         const QJsonValue &expectedId = QJsonValue(QJsonValue::Undefined),
                         const QString &protocolVersion = QString::fromLatin1(ProtocolVersion));

    QString encodeHeaderValue(const QString &value);
    std::optional<QString> decodeHeaderValue(const QString &value, QString *errorMessage = nullptr);
    MetadataValidationResult validateTransportMetadata(
        const TransportMetadata &metadata, const RequestEnvelope &request,
        const QStringList &supportedVersions = supportedProtocolVersions());

    QJsonObject
        makeResultResponse(const QJsonValue &id, QJsonObject result,
                           const std::optional<ImplementationInfo> &serverInfo = {},
                           const QString &protocolVersion = QString::fromLatin1(ProtocolVersion));
    QJsonObject makeErrorResponse(const QJsonValue &id, const ProtocolError &error);
    QJsonObject makeInitializeResult(const QString &protocolVersion,
                                     const ImplementationInfo &serverInfo,
                                     const QString &instructions = {});
    QJsonObject makeDiscoverResult(const ImplementationInfo &serverInfo, qint64 ttlMs = 0,
                                   const QString &cacheScope = QStringLiteral("private"),
                                   const QStringList &supportedVersions = modernProtocolVersions());
    QJsonObject
        makeToolsListResult(const QJsonArray &tools, const QString &nextCursor = {},
                            qint64 ttlMs = 0, const QString &cacheScope = QStringLiteral("private"),
                            const std::optional<ImplementationInfo> &serverInfo = {},
                            const QString &protocolVersion = QString::fromLatin1(ProtocolVersion));
    QJsonObject
        makeToolCallResult(const QJsonValue &structuredContent, bool isError = false,
                           const QString &text = {},
                           const std::optional<ImplementationInfo> &serverInfo = {},
                           const QString &protocolVersion = QString::fromLatin1(ProtocolVersion));

}

#endif // AUTOMATIONWIRE_MCPPROTOCOL_H
