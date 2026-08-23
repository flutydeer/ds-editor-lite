#include "McpProtocol.h"

#include "CanonicalJson.h"
#include "JsonSchema.h"

#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrl>

#include <cmath>
#include <limits>

namespace AutomationWire::Mcp {
    namespace {
        ProtocolError validationError(const int code, const QString &message,
                                      const QJsonValue &data = QJsonValue(QJsonValue::Undefined)) {
            return {code, message, data};
        }

        ProtocolError unsupportedVersionError(const QString &requested,
                                              const QStringList &supportedVersions) {
            QJsonArray supported;
            for (const auto &version : supportedVersions)
                supported.append(version);
            return validationError(UnsupportedProtocolVersion,
                                   QStringLiteral("Unsupported MCP protocol version"),
                                   QJsonObject{
                                       {QStringLiteral("supported"), supported},
                                       {QStringLiteral("requested"), requested},
            });
        }

        bool idsEqual(const QJsonValue &left, const QJsonValue &right) {
            if (left.type() != right.type())
                return false;
            if (left.isNull())
                return true;
            if (left.isString())
                return left.toString() == right.toString();
            if (left.isDouble())
                return left.toDouble() == right.toDouble();
            return false;
        }

        bool isSafePlainHeaderValue(const QString &value) {
            if (!value.isEmpty() && (value.front().isSpace() || value.back().isSpace()))
                return false;
            for (const auto character : value) {
                const auto code = character.unicode();
                if (code == 0x09)
                    continue;
                if (code < 0x20 || code > 0x7e)
                    return false;
            }
            return true;
        }

        bool hasSentinelShape(const QString &value) {
            return value.startsWith(QStringLiteral("=?base64?")) &&
                   value.endsWith(QStringLiteral("?="));
        }

        void insertServerInfo(QJsonObject &result,
                              const std::optional<ImplementationInfo> &serverInfo) {
            if (!serverInfo || !serverInfo->valid())
                return;
            auto meta = result.value(QStringLiteral("_meta")).toObject();
            meta.insert(QString::fromLatin1(ServerInfoMetaKey), serverInfo->toJson());
            result.insert(QStringLiteral("_meta"), meta);
        }
    }

    bool ImplementationInfo::valid() const {
        if (name.isEmpty() || version.isEmpty() ||
            name.size() > MaximumImplementationFieldCodeUnits ||
            version.size() > MaximumImplementationFieldCodeUnits ||
            description.size() > MaximumImplementationFieldCodeUnits ||
            websiteUrl.size() > MaximumImplementationFieldCodeUnits) {
            return false;
        }
        if (!websiteUrl.isEmpty()) {
            const QUrl url(websiteUrl, QUrl::StrictMode);
            if (!url.isValid() || url.isRelative())
                return false;
        }
        return true;
    }

    QJsonObject ImplementationInfo::toJson() const {
        QJsonObject result{
            {QStringLiteral("name"),    name   },
            {QStringLiteral("version"), version},
        };
        if (!description.isEmpty())
            result.insert(QStringLiteral("description"), description);
        if (!websiteUrl.isEmpty())
            result.insert(QStringLiteral("websiteUrl"), websiteUrl);
        return result;
    }

    std::optional<ImplementationInfo> ImplementationInfo::fromJson(const QJsonValue &value,
                                                                   QString *errorMessage) {
        if (errorMessage)
            errorMessage->clear();
        if (!value.isObject()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Implementation info must be an object");
            return std::nullopt;
        }
        const auto object = value.toObject();
        ImplementationInfo result{
            .name = object.value(QStringLiteral("name")).toString(),
            .version = object.value(QStringLiteral("version")).toString(),
            .description = object.value(QStringLiteral("description")).toString(),
            .websiteUrl = object.value(QStringLiteral("websiteUrl")).toString(),
        };
        if (!object.value(QStringLiteral("name")).isString() ||
            !object.value(QStringLiteral("version")).isString() ||
            (object.contains(QStringLiteral("description")) &&
             !object.value(QStringLiteral("description")).isString()) ||
            (object.contains(QStringLiteral("websiteUrl")) &&
             !object.value(QStringLiteral("websiteUrl")).isString()) ||
            !result.valid()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Implementation info has invalid fields");
            return std::nullopt;
        }
        return result;
    }

    QJsonObject ProtocolError::toJson() const {
        QJsonObject result{
            {QStringLiteral("code"),    code   },
            {QStringLiteral("message"), message},
        };
        if (!data.isUndefined())
            result.insert(QStringLiteral("data"), data);
        return result;
    }

    bool isValidRequestId(const QJsonValue &id) {
        if (id.isString())
            return id.toString().size() <= MaximumRequestIdCodeUnits;
        return id.isDouble() && std::isfinite(id.toDouble()) &&
               std::trunc(id.toDouble()) == id.toDouble() &&
               std::abs(id.toDouble()) <=
                   static_cast<double>(AutomationWire::MaximumSafeJsonInteger);
    }

    bool isSupportedCoreMethod(const QString &method) {
        return method == QString::fromLatin1(DiscoverMethod) ||
               method == QString::fromLatin1(ToolsListMethod) ||
               method == QString::fromLatin1(ToolsCallMethod);
    }

    QJsonObject makeRequest(const QString &method, QJsonObject params,
                            const RequestContext &context, const QJsonValue &id) {
        QJsonObject meta{
            {QString::fromLatin1(ProtocolVersionMetaKey),    context.protocolVersion   },
            {QString::fromLatin1(ClientCapabilitiesMetaKey), context.clientCapabilities},
        };
        if (context.clientInfo && context.clientInfo->valid()) {
            meta.insert(QString::fromLatin1(ClientInfoMetaKey), context.clientInfo->toJson());
        }
        params.insert(QStringLiteral("_meta"), meta);

        QJsonObject result{
            {QStringLiteral("jsonrpc"), QString::fromLatin1(JsonRpcVersion)},
            {QStringLiteral("method"),  method                             },
            {QStringLiteral("params"),  params                             },
        };
        if (!id.isUndefined())
            result.insert(QStringLiteral("id"), id);
        return result;
    }

    RequestValidationResult parseRequest(const QJsonValue &message) {
        RequestValidationResult result;
        if (!AutomationWire::checkJsonResourceLimits(message).valid()) {
            result.error = validationError(
                InvalidRequest, QStringLiteral("JSON-RPC request exceeds resource limits"));
            return result;
        }
        if (!message.isObject()) {
            result.error =
                validationError(InvalidRequest, QStringLiteral("Expected one request object"));
            return result;
        }
        const auto object = message.toObject();
        if (object.contains(QStringLiteral("result")) || object.contains(QStringLiteral("error")) ||
            object.value(QStringLiteral("jsonrpc")).toString() !=
                QString::fromLatin1(JsonRpcVersion) ||
            !object.value(QStringLiteral("method")).isString() ||
            object.value(QStringLiteral("method")).toString().isEmpty() ||
            object.value(QStringLiteral("method")).toString().size() > MaximumMethodCodeUnits) {
            result.error =
                validationError(InvalidRequest, QStringLiteral("Invalid JSON-RPC request"));
            return result;
        }
        if (object.contains(QStringLiteral("id")) &&
            !isValidRequestId(object.value(QStringLiteral("id")))) {
            result.error =
                validationError(InvalidRequest, QStringLiteral("Invalid JSON-RPC request id"));
            return result;
        }
        if (!object.value(QStringLiteral("params")).isObject()) {
            result.error =
                validationError(InvalidParams, QStringLiteral("Request params are required"));
            return result;
        }
        const auto params = object.value(QStringLiteral("params")).toObject();
        const auto notification = !object.contains(QStringLiteral("id"));
        if ((!notification && !params.value(QStringLiteral("_meta")).isObject()) ||
            (notification && params.contains(QStringLiteral("_meta")) &&
             !params.value(QStringLiteral("_meta")).isObject())) {
            result.error =
                validationError(InvalidParams, QStringLiteral("Request _meta is required"));
            return result;
        }
        const auto meta = params.value(QStringLiteral("_meta")).toObject();
        const auto protocolVersion = meta.value(QString::fromLatin1(ProtocolVersionMetaKey));
        const auto capabilities = meta.value(QString::fromLatin1(ClientCapabilitiesMetaKey));
        if (!notification &&
            (!protocolVersion.isString() || protocolVersion.toString().isEmpty() ||
             protocolVersion.toString().size() > MaximumProtocolVersionCodeUnits ||
             !capabilities.isObject())) {
            result.error = validationError(
                InvalidParams,
                QStringLiteral("protocolVersion and clientCapabilities are required in _meta"));
            return result;
        }
        std::optional<ImplementationInfo> clientInfo;
        if (meta.contains(QString::fromLatin1(ClientInfoMetaKey))) {
            QString infoError;
            clientInfo = ImplementationInfo::fromJson(
                meta.value(QString::fromLatin1(ClientInfoMetaKey)), &infoError);
            if (!clientInfo) {
                result.error = validationError(InvalidParams, infoError);
                return result;
            }
        }

        const auto method = object.value(QStringLiteral("method")).toString();
        QString name;
        if (method == QString::fromLatin1(ToolsCallMethod)) {
            if (!params.value(QStringLiteral("name")).isString() ||
                params.value(QStringLiteral("name")).toString().isEmpty() ||
                params.value(QStringLiteral("name")).toString().size() > MaximumToolNameCodeUnits) {
                result.error = validationError(InvalidParams,
                                               QStringLiteral("tools/call requires a tool name"));
                return result;
            }
            if (params.contains(QStringLiteral("arguments")) &&
                !params.value(QStringLiteral("arguments")).isObject()) {
                result.error = validationError(InvalidParams,
                                               QStringLiteral("Tool arguments must be an object"));
                return result;
            }
            name = params.value(QStringLiteral("name")).toString();
        }

        result.request = RequestEnvelope{
            .id = object.value(QStringLiteral("id")),
            .notification = notification,
            .method = method,
            .params = params,
            .meta = meta,
            .protocolVersion = protocolVersion.toString(),
            .clientCapabilities = capabilities.toObject(),
            .clientInfo = clientInfo,
            .name = name,
        };
        return result;
    }

    RequestValidationResult validateRequest(const QJsonValue &message,
                                            const QStringList &supportedVersions) {
        auto result = parseRequest(message);
        if (!result.valid() || result.request->notification ||
            supportedVersions.contains(result.request->protocolVersion))
            return result;

        const auto requested = result.request->protocolVersion;
        result.request.reset();
        result.error = unsupportedVersionError(requested, supportedVersions);
        return result;
    }

    ResponseValidationResult validateResponse(const QJsonValue &message,
                                              const QJsonValue &expectedId) {
        ResponseValidationResult validation;
        if (!AutomationWire::checkJsonResourceLimits(message).valid()) {
            validation.error = validationError(
                InvalidRequest, QStringLiteral("JSON-RPC response exceeds resource limits"));
            return validation;
        }
        if (!message.isObject()) {
            validation.error =
                validationError(InvalidRequest, QStringLiteral("Expected one response object"));
            return validation;
        }
        const auto object = message.toObject();
        if (object.value(QStringLiteral("jsonrpc")).toString() !=
            QString::fromLatin1(JsonRpcVersion)) {
            validation.error =
                validationError(InvalidRequest, QStringLiteral("Invalid JSON-RPC response"));
            return validation;
        }
        const auto hasResult = object.contains(QStringLiteral("result"));
        const auto hasError = object.contains(QStringLiteral("error"));
        const auto hasId = object.contains(QStringLiteral("id"));
        if (hasResult == hasError || !hasId ||
            (hasId && !isValidRequestId(object.value(QStringLiteral("id"))) &&
             !(hasError && object.value(QStringLiteral("id")).isNull())) ||
            (hasResult && object.value(QStringLiteral("id")).isNull())) {
            validation.error = validationError(
                InvalidRequest, QStringLiteral("Invalid JSON-RPC response envelope"));
            return validation;
        }
        if (!expectedId.isUndefined() &&
            !idsEqual(object.value(QStringLiteral("id")), expectedId)) {
            validation.error =
                validationError(InvalidRequest, QStringLiteral("JSON-RPC response id mismatch"));
            return validation;
        }

        ResponseEnvelope response{.id = object.value(QStringLiteral("id"))};
        if (hasResult) {
            if (!object.value(QStringLiteral("result")).isObject()) {
                validation.error =
                    validationError(InvalidRequest, QStringLiteral("MCP result must be an object"));
                return validation;
            }
            response.result = object.value(QStringLiteral("result")).toObject();
            if (response.result.value(QStringLiteral("resultType")).toString() !=
                QStringLiteral("complete")) {
                validation.error = validationError(
                    InvalidRequest, QStringLiteral("MCP resultType must be complete"));
                return validation;
            }
        } else {
            if (!object.value(QStringLiteral("error")).isObject()) {
                validation.error = validationError(
                    InvalidRequest, QStringLiteral("JSON-RPC error must be an object"));
                return validation;
            }
            const auto error = object.value(QStringLiteral("error")).toObject();
            const auto errorCode = error.value(QStringLiteral("code")).toDouble();
            if (!error.value(QStringLiteral("code")).isDouble() || !std::isfinite(errorCode) ||
                std::trunc(errorCode) != errorCode || errorCode < std::numeric_limits<int>::min() ||
                errorCode > std::numeric_limits<int>::max() ||
                !error.value(QStringLiteral("message")).isString()) {
                validation.error = validationError(InvalidRequest,
                                                   QStringLiteral("Invalid JSON-RPC error fields"));
                return validation;
            }
            response.error = ProtocolError{
                .code = error.value(QStringLiteral("code")).toInt(),
                .message = error.value(QStringLiteral("message")).toString(),
                .data = error.value(QStringLiteral("data")),
            };
        }
        validation.response = response;
        return validation;
    }

    QString encodeHeaderValue(const QString &value) {
        if (isSafePlainHeaderValue(value) && !hasSentinelShape(value))
            return value;
        return QStringLiteral("=?base64?%1?=").arg(QString::fromLatin1(value.toUtf8().toBase64()));
    }

    std::optional<QString> decodeHeaderValue(const QString &value, QString *errorMessage) {
        if (errorMessage)
            errorMessage->clear();
        if (!hasSentinelShape(value)) {
            if (isSafePlainHeaderValue(value))
                return value;
            if (errorMessage)
                *errorMessage = QStringLiteral("Header value contains unsafe characters");
            return std::nullopt;
        }

        const auto payload = value.sliced(9, value.size() - 11).toLatin1();
        static const QRegularExpression base64Expression(
            QStringLiteral("^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|"
                           "[A-Za-z0-9+/]{3}=)?$"));
        if (!base64Expression.match(QString::fromLatin1(payload)).hasMatch()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Header Base64 sentinel is malformed");
            return std::nullopt;
        }
        const auto bytes = QByteArray::fromBase64(payload);
        if (bytes.toBase64() != payload) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Header Base64 sentinel is not canonical");
            return std::nullopt;
        }
        const auto decoded = QString::fromUtf8(bytes);
        if (decoded.toUtf8() != bytes) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Header Base64 sentinel is not valid UTF-8");
            return std::nullopt;
        }
        return decoded;
    }

    MetadataValidationResult validateTransportMetadata(const TransportMetadata &metadata,
                                                       const RequestEnvelope &request,
                                                       const QStringList &supportedVersions) {
        const auto mismatch = [](const QString &message) {
            return MetadataValidationResult{
                validationError(HeaderMismatch, message),
            };
        };
        if (!metadata.protocolVersion || !metadata.method)
            return mismatch(QStringLiteral("Required MCP request metadata is missing"));
        if (*metadata.protocolVersion != request.protocolVersion)
            return mismatch(QStringLiteral("MCP protocol header and body do not match"));
        if (*metadata.method != request.method)
            return mismatch(QStringLiteral("MCP method header and body do not match"));
        if (request.method == QString::fromLatin1(ToolsCallMethod)) {
            if (!metadata.name)
                return mismatch(QStringLiteral("Mcp-Name is required for tools/call"));
            QString decodeError;
            const auto decodedName = decodeHeaderValue(*metadata.name, &decodeError);
            if (!decodedName || *decodedName != request.name) {
                return mismatch(decodeError.isEmpty()
                                    ? QStringLiteral("MCP name header and body do not match")
                                    : decodeError);
            }
        }
        if (!supportedVersions.contains(request.protocolVersion)) {
            return MetadataValidationResult{
                unsupportedVersionError(request.protocolVersion, supportedVersions)};
        }
        return {};
    }

    QJsonObject makeResultResponse(const QJsonValue &id, QJsonObject result,
                                   const std::optional<ImplementationInfo> &serverInfo) {
        result.insert(QStringLiteral("resultType"), QStringLiteral("complete"));
        insertServerInfo(result, serverInfo);
        return {
            {QStringLiteral("jsonrpc"), QString::fromLatin1(JsonRpcVersion)},
            {QStringLiteral("id"),      id                                 },
            {QStringLiteral("result"),  result                             },
        };
    }

    QJsonObject makeErrorResponse(const QJsonValue &id, const ProtocolError &error) {
        QJsonObject response{
            {QStringLiteral("jsonrpc"), QString::fromLatin1(JsonRpcVersion)                     },
            {QStringLiteral("id"),      isValidRequestId(id) ? id : QJsonValue(QJsonValue::Null)},
            {QStringLiteral("error"),   error.toJson()                                          },
        };
        return response;
    }

    QJsonObject makeDiscoverResult(const ImplementationInfo &serverInfo, const qint64 ttlMs,
                                   const QString &cacheScope,
                                   const QStringList &supportedVersions) {
        QJsonArray versions;
        for (const auto &version : supportedVersions)
            versions.append(version);
        QJsonObject result{
            {QStringLiteral("resultType"), QStringLiteral("complete")},
            {QStringLiteral("supportedVersions"), versions},
            {QStringLiteral("capabilities"), QJsonObject{{QStringLiteral("tools"), QJsonObject{}}}},
            {QStringLiteral("ttlMs"), std::max<qint64>(0, ttlMs)},
            {QStringLiteral("cacheScope"), cacheScope == QStringLiteral("public")
                                               ? QStringLiteral("public")
                                               : QStringLiteral("private")},
        };
        insertServerInfo(result, serverInfo);
        return result;
    }

    QJsonObject makeToolsListResult(const QJsonArray &tools, const QString &nextCursor,
                                    const qint64 ttlMs, const QString &cacheScope,
                                    const std::optional<ImplementationInfo> &serverInfo) {
        QJsonObject result{
            {QStringLiteral("resultType"), QStringLiteral("complete")},
            {QStringLiteral("tools"), tools},
            {QStringLiteral("ttlMs"), std::max<qint64>(0, ttlMs)},
            {QStringLiteral("cacheScope"), cacheScope == QStringLiteral("public")
                                               ? QStringLiteral("public")
                                               : QStringLiteral("private")},
        };
        if (!nextCursor.isEmpty())
            result.insert(QStringLiteral("nextCursor"), nextCursor);
        insertServerInfo(result, serverInfo);
        return result;
    }

    QJsonObject makeToolCallResult(const QJsonValue &structuredContent, const bool isError,
                                   const QString &text,
                                   const std::optional<ImplementationInfo> &serverInfo) {
        auto renderedText = text;
        if (renderedText.isEmpty()) {
            QString canonicalError;
            renderedText = QString::fromUtf8(canonicalJson(structuredContent, &canonicalError));
            if (!canonicalError.isEmpty())
                renderedText = QStringLiteral("null");
        }
        QJsonObject result{
            {QStringLiteral("resultType"),        QStringLiteral("complete")},
            {QStringLiteral("content"),
             QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                                    {QStringLiteral("text"), renderedText}}}},
            {QStringLiteral("structuredContent"), structuredContent         },
            {QStringLiteral("isError"),           isError                   },
        };
        insertServerInfo(result, serverInfo);
        return result;
    }

}
