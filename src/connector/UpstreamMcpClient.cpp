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

        std::optional<TrustedTransportError>
            trustedTransportError(const QJsonObject &response, const int httpStatus,
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
                    valid = code == QStringLiteral("busy") ||
                            code == QStringLiteral("mcp_stopping");
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
            return httpStatus == 200 || httpStatus == 400 || httpStatus == 404 ||
                   httpStatus == 500;
        }

        AutomationWire::Mcp::RequestContext requestContext(const QString &instanceId,
                                                           const QString &version) {
            return {
                .protocolVersion = QString::fromLatin1(AutomationWire::Mcp::ProtocolVersion),
                .clientCapabilities = QJsonObject{{QStringLiteral("tools"), QJsonObject{}}},
                .clientInfo = AutomationWire::Mcp::ImplementationInfo{
                    .name = QStringLiteral("DS Connector Lite"),
                    .version = version,
                    .description = QStringLiteral("Local DS Editor Lite MCP stdio connector (%1)")
                                       .arg(instanceId),
                },
            };
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
        const auto valid =
            candidate.isValid() && candidate.scheme() == QStringLiteral("http") &&
            candidate.host() == QStringLiteral("127.0.0.1") && candidate.port() > 0 &&
            candidate.path() == QStringLiteral("/mcp") && candidate.userInfo().isEmpty() &&
            candidate.query().isEmpty() && candidate.fragment().isEmpty();
        if (!valid) {
            if (error)
                *error =
                    QStringLiteral("The discovered MCP endpoint is not an exact local /mcp URL");
            return false;
        }
        if (m_endpoint != candidate) {
            abortAll(QStringLiteral("editor_endpoint_changed"));
            m_endpoint = candidate;
        }
        return true;
    }

    QUrl UpstreamMcpClient::endpoint() const {
        return m_endpoint;
    }

    void UpstreamMcpClient::clearEndpoint(const QString &reason) {
        abortAll(reason);
        m_endpoint = {};
    }

    qint64 UpstreamMcpClient::send(const QString &method, QJsonObject params, Callback callback,
                                   const int timeoutMs,
                                   const QHash<QByteArray, QByteArray> &parameterHeaders) {
        if (m_endpoint.isEmpty()) {
            if (callback)
                callback(UpstreamResult{.connectorError = QStringLiteral("editor_not_connected")});
            return 0;
        }

        const auto token = m_nextToken++;
        const auto upstreamId = QStringLiteral("%1:%2").arg(m_connectorInstanceId).arg(token);
        const auto message = AutomationWire::Mcp::makeRequest(
            method, std::move(params), requestContext(m_connectorInstanceId, m_connectorVersion),
            upstreamId);

        QNetworkRequest request(m_endpoint);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::ManualRedirectPolicy);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Accept", "application/json, text/event-stream");
        request.setRawHeader("MCP-Protocol-Version", AutomationWire::Mcp::ProtocolVersion);
        request.setRawHeader("Mcp-Method", method.toUtf8());
        if (method == QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod)) {
            request.setRawHeader(
                "Mcp-Name",
                AutomationWire::Mcp::encodeHeaderValue(message.value(QStringLiteral("params"))
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
        m_pending.insert(
            token, PendingRequest{reply, timer, upstreamId, std::move(callback), {}, false, {}});
        connect(timer, &QTimer::timeout, this, [this, token] {
            const auto it = m_pending.find(token);
            if (it == m_pending.end())
                return;
            it->forcedError = QStringLiteral("upstream_timeout");
            it->forcedOutcomeUnknown = true;
            it->reply->abort();
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
        const auto it = m_pending.find(requestToken);
        if (it == m_pending.end())
            return false;
        it->forcedError = reason;
        it->forcedOutcomeUnknown = reason != QStringLiteral("request_cancelled");
        it->reply->abort();
        return true;
    }

    void UpstreamMcpClient::abortAll(const QString &reason) {
        const auto tokens = m_pending.keys();
        for (const auto token : tokens)
            cancel(token, reason);
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
        if (!pending.forcedError.isEmpty()) {
            outcome.connectorError = pending.forcedError;
            outcome.outcomeUnknown = pending.forcedOutcomeUnknown;
        } else if ((outcome.httpStatus >= 300 && outcome.httpStatus < 400) ||
                   pending.reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {
            outcome.connectorError = QStringLiteral("upstream_redirect_rejected");
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
                    const auto validation =
                        AutomationWire::Mcp::validateResponse(*object, pending.upstreamId);
                    if (validation.valid() &&
                        validProtocolResponseStatus(*validation.response, outcome.httpStatus)) {
                        if (validation.response->error)
                            outcome.protocolError = validation.response->error;
                        else
                            outcome.result = validation.response->result;
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
        if (pending.callback)
            pending.callback(std::move(outcome));
    }

    std::optional<QJsonObject> UpstreamMcpClient::decodeResponseBody(
        const QByteArray &body, const QByteArray &contentType, QString &error) {
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
                const auto eventDocument =
                    QJsonDocument::fromJson(eventPayload, &eventParseError);
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
