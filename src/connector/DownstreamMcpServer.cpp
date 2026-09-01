#include "DownstreamMcpServer.h"

#include <lite/AutomationWire/JsonSchema.h>
#include <lite/ProductMetadata.h>

#include <QJsonDocument>
#include <QJsonParseError>
#include <QScopeGuard>

namespace DsConnector {
    namespace {
        constexpr qsizetype MaxStdioMessageBytes = 16 * 1024 * 1024;
        constexpr qsizetype MaxConcurrentToolCalls = 32;

        AutomationWire::Mcp::ImplementationInfo serverInfo() {
            return {
                .name = QString::fromLatin1(LiteProductMetadata::ConnectorProductName),
                .version = QString::fromLatin1(LiteProductMetadata::Version),
                .description = QStringLiteral("%1 MCP stdio connector")
                                   .arg(QString::fromLatin1(LiteProductMetadata::ProductName)),
                .websiteUrl = QString::fromLatin1(LiteProductMetadata::ProductUrl),
            };
        }

        bool hasOnlyKeys(const QJsonObject &object, const QStringList &allowed) {
            for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
                if (!allowed.contains(it.key()))
                    return false;
            }
            return true;
        }

    }

    DownstreamMcpServer::DownstreamMcpServer(ConnectorRuntime *runtime, QObject *parent)
        : QObject(parent), m_runtime(runtime) {
    }

    void DownstreamMcpServer::processLine(const QByteArray &line) {
        const auto payload = line.trimmed();
        if (payload.isEmpty())
            return;
        if (payload.size() > MaxStdioMessageBytes) {
            sendError({}, AutomationWire::Mcp::InvalidRequest,
                      QStringLiteral("MCP stdio message is too large"));
            return;
        }
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            sendError({}, AutomationWire::Mcp::ParseError, QStringLiteral("Invalid JSON"));
            return;
        }
        const auto validation = AutomationWire::Mcp::validateRequest(
            document.isObject() ? QJsonValue(document.object()) : QJsonValue(),
            AutomationWire::Mcp::supportedProtocolVersions(), m_stdioProtocolVersion);
        if (!validation.valid()) {
            const auto object = document.isObject() ? document.object() : QJsonObject{};
            const auto recognizableNotification =
                !object.contains(QStringLiteral("id")) &&
                object.value(QStringLiteral("jsonrpc")) ==
                    QString::fromLatin1(AutomationWire::Mcp::JsonRpcVersion) &&
                object.value(QStringLiteral("method")).isString();
            if (recognizableNotification)
                return;
            const auto id = document.isObject() ? document.object().value(QStringLiteral("id"))
                                                : QJsonValue(QJsonValue::Undefined);
            sendError(id, validation.error.code, validation.error.message, validation.error.data);
            return;
        }
        const auto &request = *validation.request;
        if (request.notification && !m_stdioProtocolVersion.isEmpty() &&
            request.protocolVersion != m_stdioProtocolVersion) {
            return;
        }
        if (!request.notification &&
            AutomationWire::Mcp::isModernProtocolVersion(request.protocolVersion)) {
            m_stdioProtocolVersion = request.protocolVersion;
            m_legacyInitializeResponded = false;
            m_legacyInitialized = false;
        }
        if (request.method == QString::fromLatin1(AutomationWire::Mcp::InitializeMethod)) {
            if (!hasOnlyKeys(request.params,
                             {QStringLiteral("_meta"), QStringLiteral("protocolVersion"),
                              QStringLiteral("capabilities"), QStringLiteral("clientInfo")})) {
                sendError(request.id, AutomationWire::Mcp::InvalidParams,
                          QStringLiteral("Invalid initialize params"));
                return;
            }
            m_stdioProtocolVersion = request.protocolVersion;
            m_legacyInitializeResponded = true;
            m_legacyInitialized = false;
            sendJson(AutomationWire::Mcp::makeResultResponse(
                request.id,
                AutomationWire::Mcp::makeInitializeResult(
                    m_stdioProtocolVersion, serverInfo(),
                    QStringLiteral("Use the typed %1 tools when their schemas match the requested "
                                   "operation. Connector status and discovery tools remain "
                                   "available while the editor is offline.")
                        .arg(QString::fromLatin1(LiteProductMetadata::ProductName))),
                {}, m_stdioProtocolVersion));
            return;
        }
        if (request.method == QString::fromLatin1(AutomationWire::Mcp::InitializedNotification)) {
            if (!request.notification) {
                sendError(request.id, AutomationWire::Mcp::InvalidRequest,
                          QStringLiteral("notifications/initialized must be a notification"));
                return;
            }
            if (m_legacyInitializeResponded)
                m_legacyInitialized = true;
            return;
        }
        if (request.method == QString::fromLatin1(AutomationWire::Mcp::PingMethod)) {
            if (!request.notification) {
                sendJson(AutomationWire::Mcp::makeResultResponse(request.id, {}, serverInfo(),
                                                                 request.protocolVersion));
            }
            return;
        }
        if (AutomationWire::Mcp::isLegacyProtocolVersion(request.protocolVersion) &&
            !m_legacyInitialized) {
            if (!request.notification) {
                sendError(request.id, AutomationWire::Mcp::ServerNotInitialized,
                          QStringLiteral("MCP server is not initialized"));
            }
            return;
        }
        if (request.method == QString::fromLatin1(AutomationWire::Mcp::CancelledNotification)) {
            const auto requestId = request.params.value(QStringLiteral("requestId"));
            const auto validParams =
                hasOnlyKeys(request.params, {QStringLiteral("_meta"), QStringLiteral("requestId"),
                                             QStringLiteral("reason")}) &&
                AutomationWire::Mcp::isValidRequestId(requestId) &&
                (!request.params.contains(QStringLiteral("reason")) ||
                 request.params.value(QStringLiteral("reason")).isString());
            if (!request.notification) {
                sendError(request.id, AutomationWire::Mcp::InvalidRequest,
                          QStringLiteral("notifications/cancelled must be a notification"));
                return;
            }
            if (validParams)
                handleCancellation(request);
            return;
        }
        if (request.method == QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod)) {
            if (request.notification)
                return;
            m_stdioProtocolVersion = QString::fromLatin1(AutomationWire::Mcp::ProtocolVersion);
            m_legacyInitializeResponded = false;
            m_legacyInitialized = false;
            if (!hasOnlyKeys(request.params, {QStringLiteral("_meta")})) {
                sendError(request.id, AutomationWire::Mcp::InvalidParams,
                          QStringLiteral("Invalid server/discover params"));
                return;
            }
            sendJson(AutomationWire::Mcp::makeResultResponse(
                request.id, AutomationWire::Mcp::makeDiscoverResult(serverInfo()), serverInfo(),
                request.protocolVersion));
            return;
        }
        if (request.method == QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod)) {
            handleToolsList(request);
            return;
        }
        if (request.method == QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod)) {
            handleToolsCall(request);
            return;
        }
        if (!request.notification) {
            sendError(request.id, AutomationWire::Mcp::MethodNotFound,
                      QStringLiteral("MCP method not found"));
        }
    }

    void DownstreamMcpServer::sendJson(const QJsonObject &message) {
        auto encoded = QJsonDocument(message).toJson(QJsonDocument::Compact);
        encoded.append('\n');
        emit responseLine(encoded);
    }

    void DownstreamMcpServer::sendError(const QJsonValue &id, const int code,
                                        const QString &message, const QJsonValue &data) {
        sendJson(AutomationWire::Mcp::makeErrorResponse(
            id, AutomationWire::Mcp::ProtocolError{code, message, data}));
    }

    void DownstreamMcpServer::handleCancellation(
        const AutomationWire::Mcp::RequestEnvelope &request) {
        const auto cancelledId = request.params.value(QStringLiteral("requestId"));
        const auto key = idKey(cancelledId);
        const auto token = m_pending.take(key);
        if (token) {
            m_cancelled.insert(key);
            m_runtime->cancel(token, request.params.value(QStringLiteral("reason"))
                                         .toString(QStringLiteral("request_cancelled")));
        }
    }

    void DownstreamMcpServer::handleToolsList(const AutomationWire::Mcp::RequestEnvelope &request) {
        if (request.notification)
            return;
        if (!hasOnlyKeys(request.params, {QStringLiteral("_meta"), QStringLiteral("cursor")}) ||
            (request.params.contains(QStringLiteral("cursor")) &&
             !request.params.value(QStringLiteral("cursor")).isString())) {
            sendError(request.id, AutomationWire::Mcp::InvalidParams,
                      QStringLiteral("Invalid tools/list params"));
            return;
        }
        const auto tools = m_runtime->downstreamTools();
        const auto snapshot = m_runtime->instanceId();
        const auto cursorText = request.params.value(QStringLiteral("cursor")).toString();
        qint64 offset = 0;
        if (!cursorText.isEmpty()) {
            const auto parsed = m_toolsCursorCodec.parse(
                cursorText, QStringLiteral("connector-downstream-tools-list/v1"), snapshot);
            if (!parsed.valid()) {
                sendError(request.id, AutomationWire::Mcp::InvalidParams,
                          QStringLiteral("Invalid tools/list cursor"));
                return;
            }
            offset = *parsed.offset;
        }
        constexpr auto PageSize = 256;
        if (offset < 0 || offset > tools.size()) {
            sendError(request.id, AutomationWire::Mcp::InvalidParams,
                      QStringLiteral("Invalid tools/list cursor"));
            return;
        }
        QJsonArray page;
        for (auto index = offset; index < tools.size() && index < offset + PageSize; ++index)
            page.append(tools.at(index));
        QString next;
        if (offset + page.size() < tools.size()) {
            next = m_toolsCursorCodec.issue(QStringLiteral("connector-downstream-tools-list/v1"),
                                            snapshot, offset + page.size());
            if (next.isEmpty()) {
                sendError(request.id, AutomationWire::Mcp::InternalError,
                          QStringLiteral("Unable to create tools/list cursor"));
                return;
            }
        }
        sendJson(AutomationWire::Mcp::makeResultResponse(
            request.id,
            AutomationWire::Mcp::makeToolsListResult(page, next, 0, QStringLiteral("private"),
                                                     serverInfo(), request.protocolVersion),
            serverInfo(), request.protocolVersion));
    }

    void DownstreamMcpServer::handleToolsCall(const AutomationWire::Mcp::RequestEnvelope &request) {
        if (request.notification)
            return;
        if (!hasOnlyKeys(request.params, {QStringLiteral("_meta"), QStringLiteral("name"),
                                          QStringLiteral("arguments")})) {
            sendError(request.id, AutomationWire::Mcp::InvalidParams,
                      QStringLiteral("Invalid tools/call params"));
            return;
        }
        const auto name = request.name;
        QJsonObject schema;
        QJsonObject outputSchema;
        bool validateInput = false;
        bool validateOutput = false;
        if (const auto bridge = ConnectorRuntime::findBridgeTool(name)) {
            schema = bridge->value(QStringLiteral("inputSchema")).toObject();
            outputSchema = bridge->value(QStringLiteral("outputSchema")).toObject();
            validateInput = true;
            validateOutput = name != QStringLiteral("editor.tools.invoke");
        } else if (const auto *tool = AutomationWire::findPublicTool(name)) {
            if (!m_runtime->exposurePolicy().allowsKnownTool(*tool)) {
                const auto result = AutomationWire::Mcp::makeToolCallResult(
                    QJsonObject{
                        {QStringLiteral("code"),    QStringLiteral("connector_tool_filtered")},
                        {QStringLiteral("message"), QStringLiteral("connector_tool_filtered")}
                },
                    true, {}, {}, request.protocolVersion);
                sendJson(AutomationWire::Mcp::makeResultResponse(request.id, result, serverInfo(),
                                                                 request.protocolVersion));
                return;
            }
        } else {
            sendError(request.id, AutomationWire::Mcp::InvalidParams,
                      QStringLiteral("Unknown connector tool"));
            return;
        }

        const auto arguments = request.params.value(QStringLiteral("arguments")).toObject();
        if (validateInput) {
            const auto validation = AutomationWire::validateJsonValue(arguments, schema);
            if (!validation.valid()) {
                QJsonArray issues;
                for (const auto &issue : validation.issues) {
                    issues.append(QJsonObject{
                        {QStringLiteral("instancePath"), issue.instancePath},
                        {QStringLiteral("schemaPath"),   issue.schemaPath  },
                        {QStringLiteral("message"),      issue.message     }
                    });
                }
                sendError(request.id, AutomationWire::Mcp::InvalidParams,
                          QStringLiteral("Tool arguments do not match inputSchema"),
                          QJsonObject{
                              {QStringLiteral("issues"), issues}
                });
                return;
            }
        }

        const auto key = idKey(request.id);
        if (m_inFlight.contains(key)) {
            sendError(request.id, AutomationWire::Mcp::InvalidRequest,
                      QStringLiteral("Duplicate in-flight JSON-RPC request id"));
            return;
        }
        if (m_inFlight.size() >= MaxConcurrentToolCalls) {
            const auto result = AutomationWire::Mcp::makeToolCallResult(
                QJsonObject{
                    {QStringLiteral("code"),    QStringLiteral("busy")           },
                    {QStringLiteral("message"),
                     QStringLiteral("Connector concurrent request limit reached")},
            },
                true, {}, {}, request.protocolVersion);
            sendJson(AutomationWire::Mcp::makeResultResponse(request.id, result, serverInfo(),
                                                             request.protocolVersion));
            return;
        }
        m_inFlight.insert(key);
        emit pendingResponseStarted();
        const auto token = m_runtime->callTool(
            name, arguments,
            [this, id = request.id, key, outputSchema, validateOutput,
             protocolVersion = request.protocolVersion](ToolCallOutcome outcome) {
                const auto finish = qScopeGuard([this] { emit pendingResponseFinished(); });
                m_pending.remove(key);
                m_inFlight.remove(key);
                if (m_cancelled.remove(key))
                    return;
                if (outcome.protocolError) {
                    sendJson(AutomationWire::Mcp::makeErrorResponse(id, *outcome.protocolError));
                    return;
                }
                if (validateOutput && !outcome.result.value(QStringLiteral("isError")).toBool()) {
                    const auto outputValidation = AutomationWire::validateJsonValue(
                        outcome.result.value(QStringLiteral("structuredContent")), outputSchema);
                    if (!outputValidation.valid()) {
                        outcome.result = AutomationWire::Mcp::makeToolCallResult(
                            QJsonObject{
                                {QStringLiteral("code"),    QStringLiteral("invalid_upstream_output")},
                                {QStringLiteral("message"),
                                 QStringLiteral("Tool result does not match outputSchema")           },
                        },
                            true, {}, {}, protocolVersion);
                    }
                }
                sendJson(AutomationWire::Mcp::makeResultResponse(id, outcome.result, serverInfo(),
                                                                 protocolVersion));
            });
        if (token && m_inFlight.contains(key))
            m_pending.insert(key, token);
    }

    QString DownstreamMcpServer::idKey(const QJsonValue &id) {
        if (id.isString())
            return QStringLiteral("s:") + id.toString();
        if (id.isDouble())
            return QStringLiteral("n:") + QString::number(id.toDouble(), 'g', 17);
        return {};
    }

}
