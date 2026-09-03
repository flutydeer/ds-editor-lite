#include "McpRequestDispatcher.h"

#include "Automation/Public/PublicAutomationCodecs.h"

#include "../Public/PublicAutomationRegistry.h"

#include <lite/AutomationWire/CanonicalJson.h>
#include <lite/AutomationWire/PublicToolContract.h>

#include <QJsonArray>
#include <QSet>

#include <algorithm>
#include <utility>

namespace Automation {
    namespace {
        namespace Mcp = AutomationWire::Mcp;

        constexpr qsizetype ToolsPageSize = 100;

        QString toolsSnapshotDigest(const QStringList &toolIds, QString *error) {
            QJsonArray encodedIds;
            for (const auto &toolId : toolIds)
                encodedIds.append(toolId);
            return AutomationWire::sha256Digest(
                QJsonObject{
                    {QStringLiteral("toolset_version"),
                     static_cast<qint64>(AutomationWire::PublicToolsetVersion)},
                    {QStringLiteral("tool_ids"),        encodedIds            },
            },
                error);
        }

        Mcp::ProtocolError
            invalidParams(const QString &message,
                          const QJsonValue &data = QJsonValue(QJsonValue::Undefined)) {
            return {Mcp::InvalidParams, message, data};
        }

        bool containsOnly(const QJsonObject &params, const QSet<QString> &allowed,
                          QString &unexpected) {
            for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
                if (!allowed.contains(it.key())) {
                    unexpected = it.key();
                    return false;
                }
            }
            return true;
        }

    }

    McpRequestDispatcher::McpRequestDispatcher(PublicAutomationRegistry &registry,
                                               Mcp::ImplementationInfo serverInfo)
        : m_registry(registry), m_serverInfo(std::move(serverInfo)) {
    }

    QJsonObject McpRequestDispatcher::dispatch(const Mcp::RequestEnvelope &request,
                                               const QString &clientId) const {
        if (request.method == QString::fromLatin1(Mcp::InitializeMethod)) {
            QString unexpected;
            if (!containsOnly(request.params,
                              {QStringLiteral("_meta"), QStringLiteral("protocolVersion"),
                               QStringLiteral("capabilities"), QStringLiteral("clientInfo")},
                              unexpected)) {
                return Mcp::makeErrorResponse(
                    request.id, invalidParams(QStringLiteral("Unexpected initialize parameter"),
                                              QJsonObject{
                                                  {QStringLiteral("field"), unexpected}
                }));
            }
            return Mcp::makeResultResponse(
                request.id,
                Mcp::makeInitializeResult(
                    request.protocolVersion, m_serverInfo,
                    QStringLiteral("%1 exposes the same typed automation tools through MCP "
                                   "2025-06-18, MCP 2025-11-25, and MCP 2026-07-28.")
                        .arg(m_serverInfo.name)),
                {}, request.protocolVersion);
        }
        if (request.method == QString::fromLatin1(Mcp::PingMethod)) {
            QString unexpected;
            if (!containsOnly(request.params, {QStringLiteral("_meta")}, unexpected)) {
                return Mcp::makeErrorResponse(
                    request.id, invalidParams(QStringLiteral("Unexpected ping parameter"),
                                              QJsonObject{
                                                  {QStringLiteral("field"), unexpected}
                }));
            }
            return Mcp::makeResultResponse(request.id, {}, m_serverInfo, request.protocolVersion);
        }
        if (request.method == QString::fromLatin1(Mcp::DiscoverMethod)) {
            if (!Mcp::isModernProtocolVersion(request.protocolVersion)) {
                return Mcp::makeErrorResponse(
                    request.id, {Mcp::MethodNotFound,
                                 QStringLiteral("server/discover requires MCP 2026-07-28")});
            }
            QString unexpected;
            if (!containsOnly(request.params, {QStringLiteral("_meta")}, unexpected)) {
                return Mcp::makeErrorResponse(
                    request.id,
                    invalidParams(QStringLiteral("Unexpected server/discover parameter"),
                                  QJsonObject{
                                      {QStringLiteral("field"), unexpected}
                }));
            }
            return Mcp::makeResultResponse(request.id, Mcp::makeDiscoverResult(m_serverInfo),
                                           m_serverInfo, request.protocolVersion);
        }
        if (request.method == QString::fromLatin1(Mcp::ToolsListMethod))
            return dispatchToolsList(request);
        if (request.method == QString::fromLatin1(Mcp::ToolsCallMethod))
            return dispatchToolsCall(request, clientId);
        return Mcp::makeErrorResponse(
            request.id, {Mcp::MethodNotFound, QStringLiteral("MCP method is not supported")});
    }

    QJsonObject McpRequestDispatcher::dispatchToolsList(const Mcp::RequestEnvelope &request) const {
        QString unexpected;
        if (!containsOnly(request.params, {QStringLiteral("_meta"), QStringLiteral("cursor")},
                          unexpected)) {
            return Mcp::makeErrorResponse(
                request.id, invalidParams(QStringLiteral("Unexpected tools/list parameter"),
                                          QJsonObject{
                                              {QStringLiteral("field"), unexpected}
            }));
        }
        const auto cursorValue = request.params.value(QStringLiteral("cursor"));
        if (!cursorValue.isUndefined() && !cursorValue.isString()) {
            return Mcp::makeErrorResponse(
                request.id, invalidParams(QStringLiteral("tools/list cursor must be a string")));
        }

        const auto cursorText = cursorValue.toString();
        const auto tools = m_registry.enabledContracts();
        QStringList toolIds;
        toolIds.reserve(tools.size());
        for (const auto &tool : tools)
            toolIds.append(tool.operationId);
        if (toolIds != m_toolsSnapshotIds || m_toolsSnapshotDigest.isEmpty()) {
            QString digestError;
            const auto snapshotDigest = toolsSnapshotDigest(toolIds, &digestError);
            if (!digestError.isEmpty()) {
                return Mcp::makeErrorResponse(
                    request.id, {Mcp::InternalError,
                                 QStringLiteral("tools/list snapshot could not be encoded")});
            }
            QJsonArray snapshot;
            for (const auto &tool : tools)
                snapshot.append(tool.toMcpToolJson());
            m_toolsSnapshotIds = std::move(toolIds);
            m_toolsSnapshot = std::move(snapshot);
            m_toolsSnapshotDigest = snapshotDigest;
        }

        qint64 offset = 0;
        if (!cursorText.isEmpty()) {
            const auto parsed = m_toolsCursorCodec.parse(
                cursorText, QStringLiteral("editor-mcp-tools-list/v1"), m_toolsSnapshotDigest);
            if (!parsed.valid()) {
                return Mcp::makeErrorResponse(
                    request.id, invalidParams(QStringLiteral("tools/list cursor is invalid")));
            }
            offset = *parsed.offset;
        }
        if (offset < 0 || offset > m_toolsSnapshot.size()) {
            return Mcp::makeErrorResponse(
                request.id, invalidParams(QStringLiteral("tools/list cursor is invalid")));
        }

        QJsonArray page;
        const auto end = std::min<qsizetype>(m_toolsSnapshot.size(), offset + ToolsPageSize);
        for (auto index = static_cast<qsizetype>(offset); index < end; ++index)
            page.append(m_toolsSnapshot.at(index));
        const auto nextCursor =
            end < m_toolsSnapshot.size()
                ? m_toolsCursorCodec.issue(QStringLiteral("editor-mcp-tools-list/v1"),
                                           m_toolsSnapshotDigest, end)
                : QString{};
        if (end < m_toolsSnapshot.size() && nextCursor.isEmpty()) {
            return Mcp::makeErrorResponse(
                request.id,
                {Mcp::InternalError, QStringLiteral("tools/list cursor could not be issued")});
        }
        return Mcp::makeResultResponse(
            request.id,
            Mcp::makeToolsListResult(page, nextCursor, 0, QStringLiteral("private"), m_serverInfo,
                                     request.protocolVersion),
            m_serverInfo, request.protocolVersion);
    }

    QJsonObject McpRequestDispatcher::dispatchToolsCall(const Mcp::RequestEnvelope &request,
                                                        const QString &clientId) const {
        QString unexpected;
        if (!containsOnly(request.params,
                          {QStringLiteral("_meta"), QStringLiteral("name"),
                           QStringLiteral("arguments"), QStringLiteral("inputResponses"),
                           QStringLiteral("requestState")},
                          unexpected)) {
            return Mcp::makeErrorResponse(
                request.id, invalidParams(QStringLiteral("Unexpected tools/call parameter"),
                                          QJsonObject{
                                              {QStringLiteral("field"), unexpected}
            }));
        }
        const auto argumentsValue = request.params.value(QStringLiteral("arguments"));
        if (!argumentsValue.isUndefined() && !argumentsValue.isObject()) {
            return Mcp::makeErrorResponse(
                request.id, invalidParams(QStringLiteral("Tool arguments must be an object")));
        }
        const auto inputResponses = request.params.value(QStringLiteral("inputResponses"));
        if (!inputResponses.isUndefined() && !inputResponses.isObject()) {
            return Mcp::makeErrorResponse(
                request.id,
                invalidParams(QStringLiteral("tools/call inputResponses must be an object")));
        }
        const auto requestState = request.params.value(QStringLiteral("requestState"));
        if (!requestState.isUndefined() && !requestState.isString()) {
            return Mcp::makeErrorResponse(
                request.id,
                invalidParams(QStringLiteral("tools/call requestState must be a string")));
        }
        if (!inputResponses.isUndefined() || !requestState.isUndefined()) {
            return Mcp::makeErrorResponse(request.id,
                                          invalidParams(QStringLiteral("No pending MRTR request")));
        }
        if (!AutomationWire::findPublicTool(request.name)) {
            return Mcp::makeErrorResponse(
                request.id, invalidParams(QStringLiteral("Unknown editor tool"),
                                          QJsonObject{
                                              {QStringLiteral("name"), request.name}
            }));
        }

        const auto result = m_registry.invoke(
            request.name, argumentsValue.isObject() ? argumentsValue.toObject() : QJsonObject{},
            {.clientId = clientId});
        if (!result) {
            const auto structured = encodePublicAutomationError(result.getError());
            return Mcp::makeResultResponse(
                request.id,
                Mcp::makeToolCallResult(structured, true, result.getError().message, m_serverInfo,
                                        request.protocolVersion),
                m_serverInfo, request.protocolVersion);
        }
        return Mcp::makeResultResponse(
            request.id,
            Mcp::makeToolCallResult(result.get(), false, {}, m_serverInfo, request.protocolVersion),
            m_serverInfo, request.protocolVersion);
    }

} // namespace Automation
