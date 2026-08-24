#include "McpRequestDispatcher.h"

#include "Automation/Public/PublicAutomationCodecs.h"

#include "../Public/PublicAutomationRegistry.h"

#include <lite/AutomationWire/CanonicalJson.h>

#include <QJsonArray>
#include <QSet>

#include <algorithm>
#include <utility>

namespace Automation {
    namespace {
        namespace Mcp = AutomationWire::Mcp;

        constexpr qsizetype ToolsPageSize = 100;

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

        QJsonObject encodeError(const AutomationError &error) {
            QJsonObject result{
                {QStringLiteral("code"),    errorCodeName(error.code)},
                {QStringLiteral("message"), error.message            },
            };
            if (!error.operationId.isEmpty())
                result.insert(QStringLiteral("operation_id"), error.operationId);
            if (!error.fieldPath.isEmpty())
                result.insert(QStringLiteral("field_path"), error.fieldPath);
            if (error.object) {
                result.insert(
                    QStringLiteral("object"),
                    QJsonObject{
                        {QStringLiteral("kind"), encodePublicObjectKind(error.object->kind)},
                        {QStringLiteral("id"),   error.object->value                       }
                });
            }
            if (error.taskId)
                result.insert(QStringLiteral("task_id"), error.taskId->toString());
            if (error.documentId)
                result.insert(QStringLiteral("document_id"), error.documentId->toString());
            if (error.expectedRevision) {
                result.insert(QStringLiteral("expected_revision"),
                              static_cast<qint64>(*error.expectedRevision));
            }
            if (error.actualRevision) {
                result.insert(QStringLiteral("actual_revision"),
                              static_cast<qint64>(*error.actualRevision));
            }
            return result;
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
                    QStringLiteral("DS Editor Lite exposes the same typed automation tools through "
                                   "MCP 2025-06-18, MCP 2025-11-25, and MCP 2026-07-28.")),
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
        QJsonArray snapshot;
        for (const auto &tool : tools)
            snapshot.append(tool.toMcpToolJson());
        QString digestError;
        const auto snapshotDigest = AutomationWire::sha256Digest(snapshot, &digestError);
        if (!digestError.isEmpty()) {
            return Mcp::makeErrorResponse(
                request.id,
                {Mcp::InternalError, QStringLiteral("tools/list snapshot could not be encoded")});
        }

        qint64 offset = 0;
        if (!cursorText.isEmpty()) {
            const auto parsed = m_toolsCursorCodec.parse(
                cursorText, QStringLiteral("editor-mcp-tools-list/v1"), snapshotDigest);
            if (!parsed.valid()) {
                return Mcp::makeErrorResponse(
                    request.id, invalidParams(QStringLiteral("tools/list cursor is invalid")));
            }
            offset = *parsed.offset;
        }
        if (offset < 0 || offset > tools.size()) {
            return Mcp::makeErrorResponse(
                request.id, invalidParams(QStringLiteral("tools/list cursor is invalid")));
        }

        QJsonArray page;
        const auto end = std::min<qsizetype>(tools.size(), offset + ToolsPageSize);
        for (auto index = static_cast<qsizetype>(offset); index < end; ++index)
            page.append(tools.at(index).toMcpToolJson());
        const auto nextCursor =
            end < tools.size()
                ? m_toolsCursorCodec.issue(QStringLiteral("editor-mcp-tools-list/v1"),
                                           snapshotDigest, end)
                : QString{};
        if (end < tools.size() && nextCursor.isEmpty()) {
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
            const auto structured = encodeError(result.getError());
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
