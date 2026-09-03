#include "NativeJsonRpcDispatcher.h"

#include "Automation/Public/PublicAutomationCodecs.h"
#include "Automation/Public/PublicAutomationRegistry.h"

#include <lite/AutomationWire/PublicToolContract.h>

#include <QJsonArray>
#include <QSet>

#include <cmath>

namespace Automation {
    namespace {
        constexpr qint64 JsonSafeIntegerMaximum = 9007199254740991LL;
        constexpr int ParseError = -32700;
        constexpr int InvalidRequest = -32600;
        constexpr int MethodNotFound = -32601;
        constexpr int InvalidParams = -32602;
        constexpr int InternalError = -32603;
        constexpr int AutomationFailure = -32000;

        QJsonObject errorResponse(const QJsonValue &id, const int code, const QString &message,
                                  const QJsonValue &data = QJsonValue(QJsonValue::Undefined)) {
            QJsonObject error{
                {QStringLiteral("code"),    code   },
                {QStringLiteral("message"), message},
            };
            if (!data.isUndefined())
                error.insert(QStringLiteral("data"), data);
            return {
                {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                {QStringLiteral("id"),      id                   },
                {QStringLiteral("error"),   error                },
            };
        }

        bool isValidId(const QJsonValue &id) {
            if (id.isString())
                return true;
            if (!id.isDouble())
                return false;
            const auto number = id.toDouble();
            return std::isfinite(number) && std::floor(number) == number &&
                   std::abs(number) <= static_cast<double>(JsonSafeIntegerMaximum);
        }

        QJsonObject invalidRequest(const QJsonValue &id, const QString &reason,
                                   const QJsonValue &data = QJsonValue(QJsonValue::Undefined)) {
            auto details = data.isObject() ? data.toObject() : QJsonObject{};
            details.insert(QStringLiteral("reason"), reason);
            return errorResponse(id, InvalidRequest, QStringLiteral("Invalid Request"), details);
        }

        QJsonObject automationErrorResponse(const QJsonValue &id, const AutomationError &error) {
            if (error.code == AutomationErrorCode::InvalidArgument) {
                return errorResponse(id, InvalidParams, QStringLiteral("Invalid params"),
                                     encodePublicAutomationError(error));
            }
            if (error.code == AutomationErrorCode::InternalError) {
                return errorResponse(id, InternalError, QStringLiteral("Internal error"),
                                     encodePublicAutomationError(error));
            }
            return errorResponse(id, AutomationFailure, error.message,
                                 encodePublicAutomationError(error));
        }
    }

    NativeJsonRpcDispatcher::NativeJsonRpcDispatcher(PublicAutomationRegistry &registry)
        : m_registry(registry) {
    }

    QJsonObject NativeJsonRpcDispatcher::dispatch(const QJsonValue &message,
                                                  const QString &clientId) const {
        if (!message.isObject()) {
            return invalidRequest(QJsonValue(QJsonValue::Null),
                                  QStringLiteral("JSON-RPC request must be an object"));
        }

        const auto request = message.toObject();
        const auto rawId = request.value(QStringLiteral("id"));
        const auto validId = request.contains(QStringLiteral("id")) && isValidId(rawId);
        const auto responseId = validId ? rawId : QJsonValue(QJsonValue::Null);
        if (!validId) {
            return invalidRequest(
                responseId,
                QStringLiteral("JSON-RPC requests require a string or safe integer id"));
        }

        static const QSet<QString> allowedFields{
            QStringLiteral("jsonrpc"),
            QStringLiteral("id"),
            QStringLiteral("method"),
            QStringLiteral("params"),
        };
        for (auto it = request.constBegin(); it != request.constEnd(); ++it) {
            if (!allowedFields.contains(it.key())) {
                return invalidRequest(
                    responseId, QStringLiteral("JSON-RPC request contains an unexpected field"),
                    QJsonObject{
                        {QStringLiteral("field"), it.key()}
                });
            }
        }

        if (request.value(QStringLiteral("jsonrpc")) != QStringLiteral("2.0")) {
            return invalidRequest(responseId, QStringLiteral("jsonrpc must be the string \"2.0\""));
        }
        const auto methodValue = request.value(QStringLiteral("method"));
        if (!methodValue.isString() || methodValue.toString().isEmpty()) {
            return invalidRequest(responseId, QStringLiteral("method must be a non-empty string"));
        }
        const auto method = methodValue.toString();
        const auto paramsValue = request.value(QStringLiteral("params"));
        if (!paramsValue.isUndefined() && !paramsValue.isObject()) {
            return errorResponse(
                responseId, InvalidParams, QStringLiteral("Invalid params"),
                QJsonObject{
                    {QStringLiteral("field_path"), QStringLiteral("params")                  },
                    {QStringLiteral("message"),    QStringLiteral("params must be an object")}
            });
        }
        if (!AutomationWire::findPublicTool(method)) {
            return errorResponse(responseId, MethodNotFound, QStringLiteral("Method not found"),
                                 QJsonObject{
                                     {QStringLiteral("method"), method}
            });
        }

        const auto result = m_registry.invoke(
            method, paramsValue.isObject() ? paramsValue.toObject() : QJsonObject{},
            {.clientId = clientId, .source = InvocationSource::PublicJsonRpc});
        if (!result)
            return automationErrorResponse(responseId, result.getError());
        return {
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"),      responseId           },
            {QStringLiteral("result"),  result.get()         },
        };
    }

    QJsonObject NativeJsonRpcDispatcher::parseError() {
        return errorResponse(QJsonValue(QJsonValue::Null), ParseError,
                             QStringLiteral("Parse error"));
    }

} // namespace Automation
