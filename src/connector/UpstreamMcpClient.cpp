#include "UpstreamMcpClient.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace DsConnector {
    namespace {
        constexpr qsizetype MaxResponseBytes = 16 * 1024 * 1024;
        constexpr qsizetype MaxTransportErrorMessageCharacters = 1024;

        struct TrustedTransportError {
            QString code;
            QString message;
            bool outcomeUnknown = false;
        };

        bool validTransportErrorMessage(const QString &message) {
            if (message.isEmpty() || message.size() > MaxTransportErrorMessageCharacters)
                return false;
            return std::none_of(message.cbegin(), message.cend(), [](const QChar character) {
                const auto codePoint = character.unicode();
                return codePoint < 0x20 || codePoint == 0x7f;
            });
        }

        std::optional<TrustedTransportError> trustedTransportError(const QJsonObject &response,
                                                                   const int httpStatus,
                                                                   const QByteArray &contentType) {
            const auto mediaType = contentType.split(';').value(0).trimmed().toLower();
            if (mediaType != QByteArrayLiteral("application/json") || response.size() != 1 ||
                !response.value(QStringLiteral("error")).isObject()) {
                return std::nullopt;
            }
            const auto error = response.value(QStringLiteral("error")).toObject();
            if (error.size() != 2 || !error.value(QStringLiteral("code")).isString() ||
                !error.value(QStringLiteral("message")).isString()) {
                return std::nullopt;
            }
            const auto code = error.value(QStringLiteral("code")).toString();
            const auto message = error.value(QStringLiteral("message")).toString();
            if (!validTransportErrorMessage(message))
                return std::nullopt;

            bool valid = false;
            bool outcomeUnknown = false;
            switch (httpStatus) {
                case 403:
                    valid = code == QStringLiteral("forbidden") ||
                            code == QStringLiteral("invalid_host") ||
                            code == QStringLiteral("origin_forbidden");
                    break;
                case 405:
                    valid = code == QStringLiteral("method_not_allowed");
                    break;
                case 406:
                    valid = code == QStringLiteral("not_acceptable");
                    break;
                case 413:
                    valid = code == QStringLiteral("request_too_large");
                    break;
                case 415:
                    valid = code == QStringLiteral("unsupported_media_type");
                    break;
                case 429:
                    valid = code == QStringLiteral("too_many_requests");
                    break;
                case 503:
                    valid =
                        code == QStringLiteral("busy") || code == QStringLiteral("mcp_stopping");
                    break;
                case 504:
                    valid = code == QStringLiteral("request_timeout");
                    outcomeUnknown = valid;
                    break;
                default:
                    break;
            }
            if (!valid)
                return std::nullopt;
            return TrustedTransportError{code, message, outcomeUnknown};
        }

        bool validProtocolResponseStatus(const AutomationWire::Mcp::ResponseEnvelope &response,
                                         const int httpStatus) {
            if (!response.error)
                return httpStatus >= 200 && httpStatus < 300;
            return httpStatus == 200 || httpStatus == 400 || httpStatus == 404 || httpStatus == 500;
        }

        AutomationWire::Mcp::RequestContext requestContext(const QString &instanceId,
                                                           const QString &version,
                                                           const QString &protocolVersion) {
            return {
                .protocolVersion = protocolVersion,
                .clientCapabilities = QJsonObject{{QStringLiteral("tools"), QJsonObject{}}},
                .clientInfo =
                    AutomationWire::Mcp::ImplementationInfo{
                                                  .name = QStringLiteral("DS Connector Lite"),
                                                  .version = version,
                                                  .description =
                            QStringLiteral("Local DS Editor Lite MCP stdio connector (%1)")
                                .arg(instanceId),
                                                  },
            };
        }

        std::optional<QByteArray> sessionIdHeader(QNetworkReply *reply, bool &valid) {
            valid = true;
            std::optional<QByteArray> result;
            for (const auto &[name, value] : reply->rawHeaderPairs()) {
                if (name.compare(QByteArrayLiteral("MCP-Session-Id"), Qt::CaseInsensitive) != 0)
                    continue;
                if (result || value.isEmpty() ||
                    std::any_of(value.cbegin(), value.cend(), [](const char character) {
                        const auto code = static_cast<unsigned char>(character);
                        return code < 0x21 || code > 0x7e;
                    })) {
                    valid = false;
                    return std::nullopt;
                }
                result = value;
            }
            return result;
        }
    }

    UpstreamMcpClient::UpstreamMcpClient(QString connectorInstanceId, QString connectorVersion,
                                         QObject *parent)
        : QObject(parent), m_connectorInstanceId(std::move(connectorInstanceId)),
          m_connectorVersion(std::move(connectorVersion)),
          m_network(new QNetworkAccessManager(this)) {
        m_network->setProxy(QNetworkProxy::NoProxy);
    }

    UpstreamMcpClient::~UpstreamMcpClient() {
        abortAll(QStringLiteral("connector_stopping"));
    }

    bool UpstreamMcpClient::setEndpoint(const QString &endpoint, QString *error) {
        if (error)
            error->clear();
        const QUrl candidate(endpoint, QUrl::StrictMode);
        const auto valid = candidate.isValid() && candidate.scheme() == QStringLiteral("http") &&
                           candidate.host() == QStringLiteral("127.0.0.1") &&
                           candidate.port() > 0 && candidate.path() == QStringLiteral("/mcp") &&
                           candidate.userInfo().isEmpty() && candidate.query().isEmpty() &&
                           candidate.fragment().isEmpty();
        if (!valid) {
            if (error)
                *error =
                    QStringLiteral("The discovered MCP endpoint is not an exact local /mcp URL");
            return false;
        }
        if (m_endpoint != candidate) {
            abortAll(QStringLiteral("editor_endpoint_changed"));
            m_endpoint = candidate;
            m_sessionId.clear();
        }
        return true;
    }

    QUrl UpstreamMcpClient::endpoint() const {
        return m_endpoint;
    }

    bool UpstreamMcpClient::setProtocolVersion(const QString &protocolVersion) {
        if (!AutomationWire::Mcp::supportedProtocolVersions().contains(protocolVersion))
            return false;
        if (m_protocolVersion != protocolVersion)
            m_sessionId.clear();
        m_protocolVersion = protocolVersion;
        return true;
    }

    bool UpstreamMcpClient::adoptNegotiatedProtocolVersion(const QString &protocolVersion) {
        if (!AutomationWire::Mcp::supportedProtocolVersions().contains(protocolVersion))
            return false;
        m_protocolVersion = protocolVersion;
        return true;
    }

    QString UpstreamMcpClient::protocolVersion() const {
        return m_protocolVersion;
    }

    void UpstreamMcpClient::clearEndpoint(const QString &reason) {
        abortAll(reason);
        m_endpoint = {};
        m_sessionId.clear();
    }

    qint64 UpstreamMcpClient::send(const QString &method, QJsonObject params, Callback callback,
                                   const int timeoutMs,
                                   const QHash<QByteArray, QByteArray> &parameterHeaders) {
        return post(method, std::move(params), QJsonValue(QJsonValue::Undefined), false,
                    std::move(callback), timeoutMs, parameterHeaders);
    }

    qint64
        UpstreamMcpClient::sendNotification(const QString &method, QJsonObject params,
                                            Callback callback, const int timeoutMs,
                                            const QHash<QByteArray, QByteArray> &parameterHeaders) {
        return post(method, std::move(params), QJsonValue(QJsonValue::Undefined), true,
                    std::move(callback), timeoutMs, parameterHeaders);
    }

    qint64 UpstreamMcpClient::post(const QString &method, QJsonObject params, QJsonValue upstreamId,
                                   const bool notification, Callback callback, const int timeoutMs,
                                   const QHash<QByteArray, QByteArray> &parameterHeaders) {
        if (m_endpoint.isEmpty()) {
            if (callback)
                callback(UpstreamResult{.connectorError = QStringLiteral("editor_not_connected")});
            return 0;
        }

        const auto token = m_nextToken++;
        if (!notification)
            upstreamId = QStringLiteral("%1:%2").arg(m_connectorInstanceId).arg(token);
        const auto protocolVersion = m_protocolVersion;
        const auto legacy = AutomationWire::Mcp::isLegacyProtocolVersion(protocolVersion);
        if (legacy && method == QString::fromLatin1(AutomationWire::Mcp::InitializeMethod))
            m_sessionId.clear();
        const auto sessionId = legacy ? m_sessionId : QByteArray{};
        const auto message = AutomationWire::Mcp::makeRequest(
            method, std::move(params),
            requestContext(m_connectorInstanceId, m_connectorVersion, protocolVersion), upstreamId);

        QNetworkRequest request(m_endpoint);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::ManualRedirectPolicy);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Accept", "application/json, text/event-stream");
        const auto modern = AutomationWire::Mcp::isModernProtocolVersion(protocolVersion);
        if (modern || method != QString::fromLatin1(AutomationWire::Mcp::InitializeMethod))
            request.setRawHeader("MCP-Protocol-Version", protocolVersion.toLatin1());
        if (!sessionId.isEmpty())
            request.setRawHeader("MCP-Session-Id", sessionId);
        if (modern)
            request.setRawHeader("Mcp-Method", method.toUtf8());
        if (modern && method == QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod)) {
            request.setRawHeader("Mcp-Name", AutomationWire::Mcp::encodeHeaderValue(
                                                 message.value(QStringLiteral("params"))
                                                     .toObject()
                                                     .value(QStringLiteral("name"))
                                                     .toString())
                                                 .toUtf8());
        }
        for (auto it = parameterHeaders.constBegin(); it != parameterHeaders.constEnd(); ++it)
            request.setRawHeader(it.key(), it.value());

        auto *reply =
            m_network->post(request, QJsonDocument(message).toJson(QJsonDocument::Compact));
        reply->setReadBufferSize(MaxResponseBytes + 1);
        auto *timer = new QTimer(reply);
        timer->setSingleShot(true);
        m_pending.insert(token, PendingRequest{
                                    .reply = reply,
                                    .timer = timer,
                                    .upstreamId = upstreamId,
                                    .protocolVersion = protocolVersion,
                                    .method = method,
                                    .callback = std::move(callback),
                                    .notification = notification,
                                    .sessionIdUsed = !sessionId.isEmpty(),
                                });
        connect(timer, &QTimer::timeout, this, [this, token] {
            cancelInternal(token, QStringLiteral("upstream_timeout"), true, true);
        });
        connect(reply, &QNetworkReply::readyRead, this, [this, token] {
            const auto it = m_pending.find(token);
            if (it == m_pending.end() || !it->forcedError.isEmpty())
                return;
            const auto available = it->reply->bytesAvailable();
            if (available > MaxResponseBytes - it->responseBody.size()) {
                it->forcedError = QStringLiteral("upstream_response_too_large");
                it->forcedOutcomeUnknown = true;
                it->reply->abort();
                return;
            }
            it->responseBody.append(it->reply->read(available));
        });
        connect(reply, &QNetworkReply::finished, this, [this, token] { finish(token); });
        timer->start(std::max(1, timeoutMs));
        return token;
    }

    bool UpstreamMcpClient::cancel(const qint64 requestToken, const QString &reason) {
        return cancelInternal(requestToken, reason, true, false);
    }

    bool UpstreamMcpClient::cancelInternal(const qint64 requestToken, const QString &reason,
                                           const bool notifyPeer, const bool outcomeUnknown) {
        const auto it = m_pending.find(requestToken);
        if (it == m_pending.end())
            return false;

        const auto upstreamId = it->upstreamId;
        const auto protocolVersion = it->protocolVersion;
        const auto method = it->method;
        const auto notification = it->notification;
        if (notifyPeer && !notification &&
            AutomationWire::Mcp::isLegacyProtocolVersion(protocolVersion) &&
            protocolVersion == m_protocolVersion &&
            method != QString::fromLatin1(AutomationWire::Mcp::InitializeMethod)) {
            QJsonObject params{
                {QStringLiteral("requestId"), upstreamId}
            };
            if (!reason.isEmpty())
                params.insert(QStringLiteral("reason"), reason);
            sendNotification(QString::fromLatin1(AutomationWire::Mcp::CancelledNotification),
                             std::move(params), {}, 5000);
        }

        const auto current = m_pending.find(requestToken);
        if (current == m_pending.end())
            return false;
        current->forcedError = reason;
        current->forcedOutcomeUnknown = outcomeUnknown;
        current->reply->abort();
        return true;
    }

    void UpstreamMcpClient::abortAll(const QString &reason) {
        const auto tokens = m_pending.keys();
        for (const auto token : tokens)
            cancelInternal(token, reason, false, true);
    }

    qsizetype UpstreamMcpClient::pendingCount() const {
        return m_pending.size();
    }

    void UpstreamMcpClient::finish(const qint64 token) {
        auto it = m_pending.find(token);
        if (it == m_pending.end())
            return;
        if (it->forcedError.isEmpty()) {
            const auto available = it->reply->bytesAvailable();
            if (available > MaxResponseBytes - it->responseBody.size()) {
                it->forcedError = QStringLiteral("upstream_response_too_large");
                it->forcedOutcomeUnknown = true;
            } else {
                it->responseBody.append(it->reply->read(available));
            }
        }
        auto pending = std::move(it.value());
        m_pending.erase(it);
        pending.timer->stop();

        UpstreamResult outcome;
        outcome.httpStatus =
            pending.reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto expiredSession = pending.sessionIdUsed && outcome.httpStatus == 404;
        if (expiredSession) {
            m_sessionId.clear();
            outcome.connectorError = QStringLiteral("upstream_session_expired");
        } else if (!pending.forcedError.isEmpty()) {
            outcome.connectorError = pending.forcedError;
            outcome.outcomeUnknown = pending.forcedOutcomeUnknown;
        } else if ((outcome.httpStatus >= 300 && outcome.httpStatus < 400) ||
                   pending.reply->attribute(QNetworkRequest::RedirectionTargetAttribute)
                       .isValid()) {
            outcome.connectorError = QStringLiteral("upstream_redirect_rejected");
        } else if (pending.notification && outcome.httpStatus >= 200 && outcome.httpStatus < 300 &&
                   pending.responseBody.isEmpty()) {
            // MCP notifications have no JSON-RPC response body. Streamable HTTP acknowledges them
            // with an empty successful response, normally HTTP 202.
        } else {
            const auto &body = pending.responseBody;
            if (pending.reply->error() != QNetworkReply::NoError && body.isEmpty()) {
                outcome.connectorError = QStringLiteral("upstream_transport_error");
                outcome.outcomeUnknown = true;
            } else {
                QString decodeError;
                const auto contentType =
                    pending.reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
                const auto object = decodeResponseBody(body, contentType, decodeError);
                if (!object) {
                    outcome.connectorError = decodeError;
                    outcome.outcomeUnknown = true;
                } else {
                    const auto validation = AutomationWire::Mcp::validateResponse(
                        *object, pending.upstreamId, pending.protocolVersion);
                    if (validation.valid() &&
                        validProtocolResponseStatus(*validation.response, outcome.httpStatus)) {
                        if (validation.response->error) {
                            outcome.protocolError = validation.response->error;
                        } else if (pending.method ==
                                       QString::fromLatin1(AutomationWire::Mcp::InitializeMethod) &&
                                   AutomationWire::Mcp::isLegacyProtocolVersion(
                                       pending.protocolVersion)) {
                            bool validSessionId = false;
                            const auto sessionId = sessionIdHeader(pending.reply, validSessionId);
                            if (!validSessionId) {
                                outcome.connectorError =
                                    QStringLiteral("invalid_upstream_session_id");
                                outcome.outcomeUnknown = true;
                            } else {
                                m_sessionId = sessionId.value_or(QByteArray{});
                                outcome.result = validation.response->result;
                            }
                        } else {
                            outcome.result = validation.response->result;
                        }
                    } else {
                        const auto transport =
                            trustedTransportError(*object, outcome.httpStatus, contentType);
                        if (!transport) {
                            outcome.connectorError = QStringLiteral("invalid_upstream_response");
                            outcome.outcomeUnknown = true;
                        } else {
                            outcome.connectorError = transport->code;
                            outcome.connectorErrorMessage = transport->message;
                            outcome.outcomeUnknown = transport->outcomeUnknown;
                        }
                    }
                }
            }
        }
        pending.reply->deleteLater();
        if (expiredSession)
            emit sessionExpired();
        if (pending.callback)
            pending.callback(std::move(outcome));
    }

    std::optional<QJsonObject> UpstreamMcpClient::decodeResponseBody(const QByteArray &body,
                                                                     const QByteArray &contentType,
                                                                     QString &error) {
        const auto mediaType = contentType.split(';').value(0).trimmed().toLower();
        QByteArray payload;
        if (mediaType == QByteArrayLiteral("text/event-stream")) {
            auto normalized = body;
            normalized.replace("\r\n", "\n");
            normalized.replace('\r', '\n');

            std::optional<QJsonObject> response;
            QList<QByteArray> dataLines;
            const auto dispatch = [&]() -> bool {
                if (dataLines.isEmpty())
                    return true;
                QByteArray eventPayload;
                for (const auto &dataLine : std::as_const(dataLines)) {
                    if (!eventPayload.isEmpty())
                        eventPayload.append('\n');
                    eventPayload.append(dataLine);
                }
                dataLines.clear();
                QJsonParseError eventParseError;
                const auto eventDocument = QJsonDocument::fromJson(eventPayload, &eventParseError);
                if (eventParseError.error != QJsonParseError::NoError ||
                    !eventDocument.isObject()) {
                    error = QStringLiteral("invalid_upstream_sse_response");
                    return false;
                }
                const auto object = eventDocument.object();
                if (!object.contains(QStringLiteral("result")) &&
                    !object.contains(QStringLiteral("error"))) {
                    return true;
                }
                if (response) {
                    error = QStringLiteral("multiple_upstream_sse_responses");
                    return false;
                }
                response = object;
                return true;
            };

            for (const auto &line : normalized.split('\n')) {
                if (line.isEmpty()) {
                    if (!dispatch())
                        return std::nullopt;
                    continue;
                }
                if (line.startsWith(':'))
                    continue;
                const auto separator = line.indexOf(':');
                const auto field = separator < 0 ? line : line.first(separator);
                auto value = separator < 0 ? QByteArray{} : line.sliced(separator + 1);
                if (value.startsWith(' '))
                    value.remove(0, 1);
                if (field == QByteArrayLiteral("data"))
                    dataLines.append(value);
            }
            if (!dispatch())
                return std::nullopt;
            if (!response) {
                error = QStringLiteral("invalid_upstream_sse_response");
                return std::nullopt;
            }
            return response;
        }
        if (mediaType != QByteArrayLiteral("application/json")) {
            error = QStringLiteral("unsupported_upstream_content_type");
            return std::nullopt;
        }
        payload = body;
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            error = QStringLiteral("invalid_upstream_json_response");
            return std::nullopt;
        }
        return document.object();
    }

}
