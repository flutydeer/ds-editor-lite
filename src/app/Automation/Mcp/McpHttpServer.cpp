#include "McpHttpServer.h"

#include <QCryptographicHash>
#include <QEventLoop>
#include <QFuture>
#include <QHttpHeaders>
#include <QHttpServer>
#include <QHttpServerConfiguration>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QList>
#include <QMutex>
#include <QPointer>
#include <QPromise>
#include <QTcpServer>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <memory>
#include <utility>

namespace Automation {
    namespace {
        using StatusCode = QHttpServerResponder::StatusCode;
        namespace Mcp = AutomationWire::Mcp;
        constexpr qsizetype maximumTransportBodyBytes = 1024 * 1024;

        QHttpServerResponse jsonResponse(const QJsonObject &body, const StatusCode status) {
            return QHttpServerResponse(QByteArrayLiteral("application/json"),
                                       QJsonDocument(body).toJson(QJsonDocument::Compact), status);
        }

        QHttpServerResponse transportError(const QString &code, const QString &message,
                                           const StatusCode status) {
            return jsonResponse(
                QJsonObject{
                    {QStringLiteral("error"),
                     QJsonObject{
                         {QStringLiteral("code"), code},
                         {QStringLiteral("message"), message},
                     }},
            },
                status);
        }

        QJsonObject
            nativeProtocolError(const int code, const QString &message,
                                const QJsonValue &id = QJsonValue(QJsonValue::Null),
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

        QJsonValue nativeRequestId(const QJsonValue &message) {
            if (!message.isObject())
                return QJsonValue(QJsonValue::Null);
            const auto request = message.toObject();
            if (!request.contains(QStringLiteral("id")))
                return QJsonValue(QJsonValue::Null);
            const auto id = request.value(QStringLiteral("id"));
            if (id.isString())
                return id;
            if (!id.isDouble())
                return QJsonValue(QJsonValue::Null);
            constexpr qint64 maximumSafeInteger = 9007199254740991LL;
            const auto number = id.toDouble();
            if (!std::isfinite(number) || std::floor(number) != number ||
                std::abs(number) > static_cast<double>(maximumSafeInteger)) {
                return QJsonValue(QJsonValue::Null);
            }
            return id;
        }

        bool isValidNativeResponse(const QJsonObject &response, const QJsonValue &expectedId) {
            if (response.value(QStringLiteral("jsonrpc")) != QStringLiteral("2.0") ||
                response.value(QStringLiteral("id")) != expectedId) {
                return false;
            }
            const auto hasResult = response.contains(QStringLiteral("result"));
            const auto hasError = response.contains(QStringLiteral("error"));
            if (hasResult == hasError || response.size() != 3)
                return false;
            if (!hasError)
                return true;
            const auto error = response.value(QStringLiteral("error"));
            if (!error.isObject())
                return false;
            const auto object = error.toObject();
            const auto code = object.value(QStringLiteral("code"));
            return code.isDouble() && std::isfinite(code.toDouble()) &&
                   std::floor(code.toDouble()) == code.toDouble() &&
                   object.value(QStringLiteral("message")).isString();
        }

        QFuture<QHttpServerResponse> readyResponse(QHttpServerResponse response) {
            QPromise<QHttpServerResponse> promise;
            promise.start();
            auto future = promise.future();
            promise.addResult(std::move(response));
            promise.finish();
            return future;
        }

        double mediaTypeQuality(const QList<QByteArray> &parameters) {
            double quality = 1.0;
            bool foundQuality = false;
            for (qsizetype index = 1; index < parameters.size(); ++index) {
                const auto parameter = parameters.at(index).trimmed();
                const auto separator = parameter.indexOf('=');
                if (separator < 0 ||
                    parameter.left(separator).trimmed().toLower() != QByteArrayLiteral("q")) {
                    continue;
                }
                if (foundQuality)
                    return 0.0;
                foundQuality = true;
                auto value = parameter.mid(separator + 1).trimmed();
                if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                    value = value.sliced(1, value.size() - 2);
                bool valid = false;
                const auto parsed = value.toDouble(&valid);
                if (!valid || !std::isfinite(parsed) || parsed < 0.0 || parsed > 1.0)
                    return 0.0;
                quality = parsed;
            }
            return quality;
        }

        bool acceptsRequiredMediaTypes(const QByteArray &value) {
            bool acceptsJson = false;
            bool acceptsEventStream = false;
            const auto entries = value.toLower().split(',');
            for (auto entry : entries) {
                entry = entry.trimmed();
                const auto components = entry.split(';');
                const auto type = components.constFirst().trimmed();
                const auto acceptable = mediaTypeQuality(components) > 0.0;
                acceptsJson =
                    acceptsJson || (acceptable && type == QByteArrayLiteral("application/json"));
                acceptsEventStream = acceptsEventStream ||
                                     (acceptable && type == QByteArrayLiteral("text/event-stream"));
            }
            return acceptsJson && acceptsEventStream;
        }

        bool acceptsJsonMediaType(const QByteArray &value) {
            int bestSpecificity = -1;
            double bestQuality = 0.0;
            const auto entries = value.toLower().split(',');
            for (auto entry : entries) {
                const auto components = entry.trimmed().split(';');
                const auto type = components.constFirst().trimmed();
                int specificity = -1;
                if (type == QByteArrayLiteral("application/json"))
                    specificity = 2;
                else if (type == QByteArrayLiteral("application/*"))
                    specificity = 1;
                else if (type == QByteArrayLiteral("*/*"))
                    specificity = 0;
                if (specificity < 0)
                    continue;
                const auto quality = mediaTypeQuality(components);
                if (specificity > bestSpecificity) {
                    bestSpecificity = specificity;
                    bestQuality = quality;
                } else if (specificity == bestSpecificity) {
                    bestQuality = std::max(bestQuality, quality);
                }
            }
            return bestSpecificity >= 0 && bestQuality > 0.0;
        }

        bool isJsonContentType(const QByteArray &value) {
            return value.toLower().split(';').constFirst().trimmed() ==
                   QByteArrayLiteral("application/json");
        }

        bool isAllowedOrigin(const QByteArray &value, const quint16 localPort) {
            if (value.isEmpty())
                return false;
            const QUrl origin(QString::fromLatin1(value), QUrl::StrictMode);
            if (!origin.isValid() || origin.isRelative() || !origin.userInfo().isEmpty() ||
                !origin.query().isEmpty() || !origin.fragment().isEmpty() ||
                (!origin.path().isEmpty() && origin.path() != QStringLiteral("/"))) {
                return false;
            }
            const auto scheme = origin.scheme().toLower();
            const auto host = origin.host().toLower();
            return (scheme == QStringLiteral("http") || scheme == QStringLiteral("https")) &&
                   (host == QStringLiteral("127.0.0.1") || host == QStringLiteral("localhost")) &&
                   origin.port() == localPort;
        }

        bool hasExpectedHost(const QByteArray &value, const quint16 localPort) {
            const auto expected = QByteArrayLiteral("127.0.0.1:") + QByteArray::number(localPort);
            return value.trimmed().toLower() == expected;
        }

        std::optional<QByteArray> uniqueHeader(const QHttpHeaders &headers,
                                               const QByteArray &name) {
            const auto values = headers.values(name);
            if (values.size() != 1 || values.constFirst().isEmpty())
                return std::nullopt;
            return values.constFirst();
        }

        bool hasUniqueNonEmptyHeader(const QHttpHeaders &headers, const QByteArray &name) {
            return uniqueHeader(headers, name).has_value();
        }

        bool withinJsonLimits(const QJsonValue &value, const int maximumDepth,
                              const qsizetype maximumNodes) {
            struct Entry {
                QJsonValue value;
                int depth = 0;
            };

            QList<Entry> pending{
                {value, 1}
            };
            qsizetype nodes = 0;
            while (!pending.isEmpty()) {
                const auto entry = pending.takeLast();
                if (++nodes > maximumNodes || entry.depth > maximumDepth)
                    return false;
                if (entry.value.isArray()) {
                    const auto array = entry.value.toArray();
                    for (const auto &item : array)
                        pending.append({item, entry.depth + 1});
                } else if (entry.value.isObject()) {
                    const auto object = entry.value.toObject();
                    for (auto it = object.constBegin(); it != object.constEnd(); ++it)
                        pending.append({it.value(), entry.depth + 1});
                }
            }
            return true;
        }

        QString clientIdFor(const Mcp::RequestEnvelope &request,
                            const QHttpServerRequest &httpRequest,
                            const std::optional<QString> &sessionClientId) {
            const auto connectorMetadata =
                request.meta.value(QString::fromLatin1(Mcp::ConnectorInstanceIdMetaKey));
            const auto hasConnectorIdentity = connectorMetadata.isString() &&
                                              !connectorMetadata.toString().isEmpty() &&
                                              connectorMetadata.toString().size() <= 128;
            if (!hasConnectorIdentity && sessionClientId)
                return *sessionClientId;
            QJsonObject identity{
                {QStringLiteral("remoteAddress"), httpRequest.remoteAddress().toString()},
            };
            if (hasConnectorIdentity) {
                identity.insert(QStringLiteral("connectorMetadata"), connectorMetadata.toString());
            } else {
                identity.insert(QStringLiteral("remotePort"), httpRequest.remotePort());
                if (request.clientInfo) {
                    identity.insert(QStringLiteral("clientName"), request.clientInfo->name);
                    identity.insert(QStringLiteral("clientVersion"), request.clientInfo->version);
                }
            }
            const auto digest = QCryptographicHash::hash(
                QJsonDocument(identity).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256);
            return QStringLiteral("mcp-%1").arg(QString::fromLatin1(digest.toHex().left(24)));
        }

        QString nativeClientIdFor(const QHttpServerRequest &request) {
            const QJsonObject identity{
                {QStringLiteral("remoteAddress"), request.remoteAddress().toString()},
                {QStringLiteral("remotePort"),    request.remotePort()              },
            };
            const auto digest = QCryptographicHash::hash(
                QJsonDocument(identity).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256);
            return QStringLiteral("native-%1").arg(QString::fromLatin1(digest.toHex().left(24)));
        }

        Mcp::TransportMetadata transportMetadata(const QHttpServerRequest &request) {
            const auto optionalHeader =
                [&request](const QByteArray &name) -> std::optional<QString> {
                const auto value = uniqueHeader(request.headers(), name);
                return value ? std::optional(QString::fromLatin1(*value)) : std::nullopt;
            };
            return {
                .protocolVersion = optionalHeader(QByteArrayLiteral("MCP-Protocol-Version")),
                .method = optionalHeader(QByteArrayLiteral("Mcp-Method")),
                .name = optionalHeader(QByteArrayLiteral("Mcp-Name")),
            };
        }

        StatusCode protocolErrorStatus(const Mcp::ProtocolError &error) {
            if (error.code == Mcp::MethodNotFound)
                return StatusCode::NotFound;
            if (error.code == Mcp::InternalError)
                return StatusCode::InternalServerError;
            return StatusCode::BadRequest;
        }

    }

    class McpHttpTransportState final {
    public:
        enum class Protocol {
            Mcp,
            Native,
        };

        enum class Rejection {
            None,
            Stopping,
            GlobalBusy,
        };

        enum class SessionRetirement {
            Retired,
            NotFound,
            ProtocolMismatch,
        };

        struct PendingRequest {
            quint64 id = 0;
            QPromise<QHttpServerResponse> promise;
            Protocol protocol = Protocol::Mcp;
            QString clientId;
            QString protocolVersion;
            QJsonValue requestId = QJsonValue(QJsonValue::Undefined);
            bool handling = false;
        };

        struct Admission {
            Rejection rejection = Rejection::None;
            std::shared_ptr<PendingRequest> pending;
            QFuture<QHttpServerResponse> future;
        };

        struct Session {
            QString clientId;
            QString protocolVersion;
            bool initialized = false;
        };

        explicit McpHttpTransportState(McpHttpLimits limits) : m_limits(std::move(limits)) {
        }

        void startAccepting() {
            const QMutexLocker locker(&m_mutex);
            m_accepting = true;
            m_globalInFlight = 0;
            m_pending.clear();
            m_sessions.clear();
            m_sessionOrder.clear();
        }

        [[nodiscard]] bool isAccepting() const {
            const QMutexLocker locker(&m_mutex);
            return m_accepting;
        }

        Admission admit(const Protocol protocol) {
            const QMutexLocker locker(&m_mutex);
            if (!m_accepting)
                return {.rejection = Rejection::Stopping};
            if (m_globalInFlight >= m_limits.maximumGlobalInFlight)
                return {.rejection = Rejection::GlobalBusy};
            auto pending = std::make_shared<PendingRequest>();
            pending->id = m_nextRequestId++;
            pending->protocol = protocol;
            pending->promise.start();
            auto future = pending->promise.future();
            m_pending.insert(pending->id, pending);
            ++m_globalInFlight;
            return {
                .rejection = Rejection::None,
                .pending = std::move(pending),
                .future = std::move(future),
            };
        }

        bool correlate(const quint64 id, QString clientId, QString protocolVersion,
                       QJsonValue requestId) {
            const QMutexLocker locker(&m_mutex);
            const auto it = m_pending.find(id);
            if (it == m_pending.end())
                return false;
            it.value()->clientId = std::move(clientId);
            it.value()->protocolVersion = std::move(protocolVersion);
            it.value()->requestId = std::move(requestId);
            return true;
        }

        bool beginHandling(const quint64 id) {
            const QMutexLocker locker(&m_mutex);
            const auto it = m_pending.find(id);
            if (it == m_pending.end())
                return false;
            it.value()->handling = true;
            return true;
        }

        bool cancelQueued(const QString &clientId, const QString &protocolVersion,
                          const QJsonValue &requestId) {
            std::shared_ptr<PendingRequest> pending;
            quint64 pendingId = 0;
            {
                const QMutexLocker locker(&m_mutex);
                for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
                    const auto &candidate = it.value();
                    if (candidate->handling || candidate->clientId != clientId ||
                        candidate->protocolVersion != protocolVersion ||
                        candidate->requestId != requestId) {
                        continue;
                    }
                    if (pending)
                        return false;
                    pending = candidate;
                    pendingId = it.key();
                }
                if (pending) {
                    m_pending.remove(pendingId);
                    m_globalInFlight = std::max(0, m_globalInFlight - 1);
                }
            }
            if (!pending)
                return false;
            pending->promise.addResult(QHttpServerResponse(StatusCode::NoContent));
            pending->promise.finish();
            return true;
        }

        bool complete(const quint64 id, QHttpServerResponse response) {
            std::shared_ptr<PendingRequest> pending;
            {
                const QMutexLocker locker(&m_mutex);
                const auto it = m_pending.find(id);
                if (it == m_pending.end())
                    return false;
                pending = it.value();
                m_pending.erase(it);
                m_globalInFlight = std::max(0, m_globalInFlight - 1);
            }
            pending->promise.addResult(std::move(response));
            pending->promise.finish();
            return true;
        }

        bool completeInitialize(const quint64 id, QHttpServerResponse response, QString clientId,
                                QString protocolVersion) {
            std::shared_ptr<PendingRequest> pending;
            QByteArray sessionId;
            {
                const QMutexLocker locker(&m_mutex);
                const auto it = m_pending.find(id);
                if (it == m_pending.end())
                    return false;
                do {
                    sessionId =
                        QUuid::createUuid().toString(QUuid::WithoutBraces).remove(u'-').toLatin1();
                } while (m_sessions.contains(sessionId));
                m_sessions.insert(sessionId, {std::move(clientId), std::move(protocolVersion)});
                m_sessionOrder.append(sessionId);
                const auto maximumSessions = std::max<qsizetype>(1, m_limits.maximumLegacySessions);
                while (m_sessionOrder.size() > maximumSessions)
                    m_sessions.remove(m_sessionOrder.takeFirst());
                pending = it.value();
                m_pending.erase(it);
                m_globalInFlight = std::max(0, m_globalInFlight - 1);
            }
            auto headers = response.headers();
            headers.replaceOrAppend(QByteArrayLiteral("MCP-Session-Id"), sessionId);
            response.setHeaders(std::move(headers));
            pending->promise.addResult(std::move(response));
            pending->promise.finish();
            return true;
        }

        [[nodiscard]] std::optional<Session> session(const QByteArray &sessionId) const {
            const QMutexLocker locker(&m_mutex);
            const auto found = m_sessions.constFind(sessionId);
            return found == m_sessions.cend() ? std::nullopt : std::optional<Session>(*found);
        }

        bool markSessionInitialized(const QByteArray &sessionId, const QString &protocolVersion) {
            const QMutexLocker locker(&m_mutex);
            const auto found = m_sessions.find(sessionId);
            if (found == m_sessions.end() || found->protocolVersion != protocolVersion)
                return false;
            found->initialized = true;
            return true;
        }

        SessionRetirement retireSession(const QByteArray &sessionId,
                                        const QString &protocolVersion) {
            const QMutexLocker locker(&m_mutex);
            const auto found = m_sessions.find(sessionId);
            if (found == m_sessions.end())
                return SessionRetirement::NotFound;
            if (found->protocolVersion != protocolVersion)
                return SessionRetirement::ProtocolMismatch;
            m_sessions.erase(found);
            m_sessionOrder.removeAll(sessionId);
            return SessionRetirement::Retired;
        }

        void disableMcp() {
            QList<std::shared_ptr<PendingRequest>> pending;
            {
                const QMutexLocker locker(&m_mutex);
                m_sessions.clear();
                m_sessionOrder.clear();
                for (auto it = m_pending.begin(); it != m_pending.end();) {
                    if (it.value()->protocol != Protocol::Mcp) {
                        ++it;
                        continue;
                    }
                    pending.append(it.value());
                    it = m_pending.erase(it);
                    m_globalInFlight = std::max(0, m_globalInFlight - 1);
                }
            }
            for (const auto &request : pending) {
                request->promise.addResult(transportError(QStringLiteral("route_unavailable"),
                                                          QStringLiteral("MCP route is disabled"),
                                                          StatusCode::ServiceUnavailable));
                request->promise.finish();
            }
        }

        void stopAccepting() {
            QList<std::shared_ptr<PendingRequest>> pending;
            {
                const QMutexLocker locker(&m_mutex);
                m_accepting = false;
                pending = m_pending.values();
                m_pending.clear();
                m_sessions.clear();
                m_sessionOrder.clear();
                m_globalInFlight = 0;
            }
            for (const auto &request : pending) {
                const auto native = request->protocol == Protocol::Native;
                request->promise.addResult(transportError(
                    native ? QStringLiteral("automation_stopping") : QStringLiteral("mcp_stopping"),
                    native ? QStringLiteral("Automation server is stopping")
                           : QStringLiteral("MCP server is stopping"),
                    StatusCode::ServiceUnavailable));
                request->promise.finish();
            }
        }

    private:
        McpHttpLimits m_limits;
        mutable QMutex m_mutex;
        bool m_accepting = false;
        int m_globalInFlight = 0;
        QHash<quint64, std::shared_ptr<PendingRequest>> m_pending;
        QHash<QByteArray, Session> m_sessions;
        QList<QByteArray> m_sessionOrder;
        quint64 m_nextRequestId = 1;
    };

    class McpHttpServer::Worker final : public QObject {
    public:
        Worker(QObject *handlerContext, RequestHandler handler, NativeRequestHandler nativeHandler,
               McpHttpLimits limits, std::shared_ptr<McpHttpTransportState> transportState)
            : m_handlerContext(handlerContext), m_handler(std::move(handler)),
              m_nativeHandler(std::move(nativeHandler)), m_limits(limits),
              m_transportState(std::move(transportState)) {
        }

        bool start(const quint16 requestedPort, const AutomationHttpRoutes routes,
                   quint16 &actualPort, QString &error) {
            if (m_httpServer || m_tcpServer) {
                error = QStringLiteral("Automation HTTP server is already running");
                return false;
            }
            if ((!routes.mcp && !routes.native) || (routes.mcp && !m_handler) ||
                (routes.native && !m_nativeHandler)) {
                error = QStringLiteral("Automation HTTP routes are not configured");
                return false;
            }

            auto *httpServer = new QHttpServer(this);
            auto *tcpServer = new QTcpServer(this);
            auto configuration = httpServer->configuration();
            configuration.setMaximumBodySize(maximumTransportBodyBytes);
            configuration.setKeepAliveTimeout(
                std::chrono::seconds(std::max(1, (m_limits.requestDeadlineMs + 999) / 1000)));
            httpServer->setConfiguration(configuration);
            if (m_handler) {
                httpServer->route(
                    QStringLiteral("/mcp"), QHttpServerRequest::Method::AnyKnown,
                    [this](QHttpServerRequest request) { return handleMcpRequest(request); });
            }
            if (m_nativeHandler) {
                httpServer->route(
                    QStringLiteral("/automation/v1"), QHttpServerRequest::Method::AnyKnown,
                    [this](QHttpServerRequest request) { return handleNativeRequest(request); });
            }
            httpServer->addAfterRequestHandler(
                this, [](const QHttpServerRequest &, QHttpServerResponse &response) {
                    auto headers = response.headers();
                    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::CacheControl,
                                            QStringLiteral("no-store"));
                    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::XContentTypeOptions,
                                            QStringLiteral("nosniff"));
                    response.setHeaders(std::move(headers));
                });

            if (!tcpServer->listen(QHostAddress(QStringLiteral("127.0.0.1")), requestedPort)) {
                error = tcpServer->errorString();
                delete tcpServer;
                delete httpServer;
                return false;
            }
            if (!httpServer->bind(tcpServer)) {
                error =
                    QStringLiteral("Failed to bind the Automation HTTP server to its TCP listener");
                tcpServer->close();
                delete tcpServer;
                delete httpServer;
                return false;
            }

            m_httpServer = httpServer;
            m_tcpServer = tcpServer;
            m_routes = routes;
            actualPort = tcpServer->serverPort();
            return true;
        }

        bool setRoutes(const AutomationHttpRoutes routes, QString &error) {
            if (!m_httpServer || !m_tcpServer) {
                error = QStringLiteral("Automation HTTP server is not running");
                return false;
            }
            if ((!routes.mcp && !routes.native) || (routes.mcp && !m_handler) ||
                (routes.native && !m_nativeHandler)) {
                error = QStringLiteral("Automation HTTP routes are not configured");
                return false;
            }
            if (m_routes.mcp && !routes.mcp)
                m_transportState->disableMcp();
            m_routes = routes;
            return true;
        }

        void stop() {
            QPointer<QTcpServer> tcpServer(m_tcpServer);
            if (tcpServer)
                tcpServer->close();
            delete m_httpServer;
            m_httpServer = nullptr;
            if (tcpServer)
                delete tcpServer.data();
            m_tcpServer = nullptr;
        }

    private:
        std::optional<QHttpServerResponse>
            commonRejection(const QHttpServerRequest &request,
                            const McpHttpTransportState::Protocol protocol) const {
            if (!request.remoteAddress().isLoopback() ||
                request.localAddress() != QHostAddress(QStringLiteral("127.0.0.1"))) {
                return transportError(QStringLiteral("forbidden"),
                                      QStringLiteral("Automation endpoint is local-only"),
                                      StatusCode::Forbidden);
            }
            const auto host = uniqueHeader(request.headers(), QByteArrayLiteral("Host"));
            if (!host || !hasExpectedHost(*host, request.localPort())) {
                return transportError(
                    QStringLiteral("invalid_host"),
                    QStringLiteral("Host header does not match the Automation endpoint"),
                    StatusCode::Forbidden);
            }
            const auto origins = request.headers().values(QHttpHeaders::WellKnownHeader::Origin);
            if (origins.size() > 1 ||
                (origins.size() == 1 &&
                 !isAllowedOrigin(origins.constFirst(), request.localPort()))) {
                return transportError(QStringLiteral("origin_forbidden"),
                                      QStringLiteral("Origin is not an allowed localhost origin"),
                                      StatusCode::Forbidden);
            }
            if (!m_transportState->isAccepting()) {
                const auto native = protocol == McpHttpTransportState::Protocol::Native;
                return transportError(native ? QStringLiteral("automation_stopping")
                                             : QStringLiteral("mcp_stopping"),
                                      native ? QStringLiteral("Automation server is stopping")
                                             : QStringLiteral("MCP server is stopping"),
                                      StatusCode::ServiceUnavailable);
            }
            return std::nullopt;
        }

        QFuture<QHttpServerResponse> handleMcpRequest(const QHttpServerRequest &request) {
            if (auto rejection = commonRejection(request, McpHttpTransportState::Protocol::Mcp)) {
                return readyResponse(std::move(*rejection));
            }
            if (!m_routes.mcp) {
                return readyResponse(transportError(QStringLiteral("route_unavailable"),
                                                    QStringLiteral("MCP route is disabled"),
                                                    StatusCode::NotFound));
            }

            if (request.method() == QHttpServerRequest::Method::Delete) {
                const auto sessionHeaders =
                    request.headers().values(QByteArrayLiteral("MCP-Session-Id"));
                if (sessionHeaders.size() != 1 || sessionHeaders.constFirst().isEmpty()) {
                    return readyResponse(transportError(
                        QStringLiteral("invalid_session"),
                        QStringLiteral("MCP-Session-Id header is missing or malformed"),
                        StatusCode::BadRequest));
                }
                const auto protocolHeaders =
                    request.headers().values(QByteArrayLiteral("MCP-Protocol-Version"));
                if (protocolHeaders.size() != 1 || protocolHeaders.constFirst().isEmpty()) {
                    return readyResponse(transportError(
                        QStringLiteral("invalid_protocol_version"),
                        QStringLiteral("MCP-Protocol-Version header is missing or malformed"),
                        StatusCode::BadRequest));
                }
                switch (m_transportState->retireSession(
                    sessionHeaders.constFirst(),
                    QString::fromLatin1(protocolHeaders.constFirst()))) {
                    case McpHttpTransportState::SessionRetirement::Retired:
                        return readyResponse(QHttpServerResponse(StatusCode::NoContent));
                    case McpHttpTransportState::SessionRetirement::NotFound:
                        return readyResponse(
                            transportError(QStringLiteral("session_not_found"),
                                           QStringLiteral("MCP session is unknown or expired"),
                                           StatusCode::NotFound));
                    case McpHttpTransportState::SessionRetirement::ProtocolMismatch:
                        return readyResponse(transportError(
                            QStringLiteral("session_protocol_mismatch"),
                            QStringLiteral(
                                "MCP session protocol version does not match the request"),
                            StatusCode::BadRequest));
                }
            }

            if (request.method() != QHttpServerRequest::Method::Post) {
                auto response =
                    transportError(QStringLiteral("method_not_allowed"),
                                   QStringLiteral("MCP endpoint accepts POST and DELETE requests"),
                                   StatusCode::MethodNotAllowed);
                auto headers = response.headers();
                headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Allow,
                                        QStringLiteral("POST, DELETE"));
                response.setHeaders(std::move(headers));
                return readyResponse(std::move(response));
            }

            const auto contentType =
                uniqueHeader(request.headers(), QByteArrayLiteral("Content-Type"));
            if (!contentType || !isJsonContentType(*contentType)) {
                return readyResponse(
                    transportError(QStringLiteral("unsupported_media_type"),
                                   QStringLiteral("MCP requests require application/json"),
                                   StatusCode::UnsupportedMediaType));
            }
            const auto acceptValues =
                request.headers().values(QHttpHeaders::WellKnownHeader::Accept);
            if (!acceptsRequiredMediaTypes(acceptValues.join(QByteArrayLiteral(",")))) {
                return readyResponse(transportError(
                    QStringLiteral("not_acceptable"),
                    QStringLiteral("Accept must include application/json and text/event-stream"),
                    StatusCode::NotAcceptable));
            }
            if (request.body().size() > m_limits.maximumRequestBytes) {
                return readyResponse(
                    transportError(QStringLiteral("request_too_large"),
                                   QStringLiteral("MCP request body exceeds the configured limit"),
                                   StatusCode::PayloadTooLarge));
            }

            QJsonParseError parseError;
            const auto document = QJsonDocument::fromJson(request.body(), &parseError);
            if (parseError.error != QJsonParseError::NoError) {
                return readyResponse(jsonResponse(
                    Mcp::makeErrorResponse(QJsonValue(QJsonValue::Null),
                                           {Mcp::ParseError, QStringLiteral("Invalid JSON")}),
                    StatusCode::BadRequest));
            }
            const QJsonValue message = document.isObject()  ? QJsonValue(document.object())
                                       : document.isArray() ? QJsonValue(document.array())
                                                            : QJsonValue();
            const auto requestObject = document.isObject() ? document.object() : QJsonObject{};
            const auto rawId = requestObject.value(QStringLiteral("id"));
            const auto hasRequestId =
                requestObject.contains(QStringLiteral("id")) && Mcp::isValidRequestId(rawId);
            if (document.isObject()) {
                const auto &headers = request.headers();
                const auto protocolHeader =
                    uniqueHeader(headers, QByteArrayLiteral("MCP-Protocol-Version"));
                const auto methodHeader = uniqueHeader(headers, QByteArrayLiteral("Mcp-Method"));
                const auto bodyMethod = requestObject.value(QStringLiteral("method")).toString();
                const auto transportMethod =
                    methodHeader ? QString::fromLatin1(*methodHeader) : QString{};
                const auto bodyMeta = requestObject.value(QStringLiteral("params"))
                                          .toObject()
                                          .value(QStringLiteral("_meta"))
                                          .toObject();
                const auto modernTransport =
                    bodyMethod == QString::fromLatin1(Mcp::DiscoverMethod) ||
                    (protocolHeader && *protocolHeader == QByteArray(Mcp::ProtocolVersion)) ||
                    bodyMeta.value(QString::fromLatin1(Mcp::ProtocolVersionMetaKey)).toString() ==
                        QString::fromLatin1(Mcp::ProtocolVersion);
                const auto missingRequiredHeader =
                    modernTransport &&
                    (!protocolHeader || !methodHeader ||
                     ((bodyMethod == QString::fromLatin1(Mcp::ToolsCallMethod) ||
                       transportMethod == QString::fromLatin1(Mcp::ToolsCallMethod)) &&
                      !hasUniqueNonEmptyHeader(headers, QByteArrayLiteral("Mcp-Name"))));
                if (missingRequiredHeader) {
                    return readyResponse(jsonResponse(
                        Mcp::makeErrorResponse(
                            rawId, {Mcp::HeaderMismatch,
                                    QStringLiteral(
                                        "Required MCP request metadata is missing or malformed")}),
                        StatusCode::BadRequest));
                }
            }

            if (!withinJsonLimits(message, m_limits.maximumJsonDepth, m_limits.maximumJsonNodes)) {
                return readyResponse(jsonResponse(
                    Mcp::makeErrorResponse(
                        hasRequestId ? rawId : QJsonValue(QJsonValue::Null),
                        {Mcp::InvalidRequest, QStringLiteral("JSON structure limit exceeded")}),
                    StatusCode::BadRequest));
            }

            const auto metadata = transportMetadata(request);
            const auto validation = Mcp::parseRequest(
                message, metadata.protocolVersion ? *metadata.protocolVersion : QString{});
            if (!validation.valid()) {
                const auto id = document.isObject() ? document.object().value(QStringLiteral("id"))
                                                    : QJsonValue(QJsonValue::Null);
                return readyResponse(jsonResponse(Mcp::makeErrorResponse(id, validation.error),
                                                  protocolErrorStatus(validation.error)));
            }
            const auto &validatedRequest = *validation.request;
            const auto protocolHeaderValues =
                request.headers().values(QByteArrayLiteral("MCP-Protocol-Version"));
            const auto protocolHeaderWellFormed =
                protocolHeaderValues.size() == 1 && !protocolHeaderValues.constFirst().isEmpty();
            const auto initializeRequest =
                validatedRequest.method == QString::fromLatin1(Mcp::InitializeMethod);
            if ((!protocolHeaderValues.isEmpty() && !protocolHeaderWellFormed) ||
                (!initializeRequest && !protocolHeaderWellFormed)) {
                const Mcp::ProtocolError error{
                    Mcp::HeaderMismatch,
                    QStringLiteral("MCP-Protocol-Version header is missing or malformed")};
                return readyResponse(
                    jsonResponse(Mcp::makeErrorResponse(validatedRequest.id, error),
                                 protocolErrorStatus(error)));
            }
            const auto metadataValidation =
                Mcp::validateTransportMetadata(metadata, validatedRequest);
            if (!metadataValidation.valid()) {
                return readyResponse(jsonResponse(
                    Mcp::makeErrorResponse(validatedRequest.id, *metadataValidation.error),
                    protocolErrorStatus(*metadataValidation.error)));
            }
            if (!validatedRequest.notification &&
                validatedRequest.method == QString::fromLatin1(Mcp::InitializeMethod) &&
                !Mcp::isLegacyProtocolVersion(validatedRequest.protocolVersion)) {
                const Mcp::ProtocolError error{
                    Mcp::InvalidRequest,
                    QStringLiteral(
                        "initialize is only available for MCP 2025-06-18 and 2025-11-25")};
                return readyResponse(
                    jsonResponse(Mcp::makeErrorResponse(validatedRequest.id, error),
                                 protocolErrorStatus(error)));
            }
            const auto sessionHeaderValues =
                request.headers().values(QByteArrayLiteral("MCP-Session-Id"));
            if (sessionHeaderValues.size() > 1 ||
                (sessionHeaderValues.size() == 1 && sessionHeaderValues.constFirst().isEmpty())) {
                return readyResponse(transportError(
                    QStringLiteral("invalid_session"),
                    QStringLiteral("MCP-Session-Id header is malformed"), StatusCode::BadRequest));
            }
            if (!initializeRequest &&
                Mcp::isLegacyProtocolVersion(validatedRequest.protocolVersion) &&
                sessionHeaderValues.isEmpty()) {
                return readyResponse(transportError(
                    QStringLiteral("invalid_session"),
                    QStringLiteral("MCP-Session-Id header is required for legacy requests"),
                    StatusCode::BadRequest));
            }
            std::optional<McpHttpTransportState::Session> session;
            if (sessionHeaderValues.size() == 1) {
                session = m_transportState->session(sessionHeaderValues.constFirst());
                if (!session) {
                    return readyResponse(transportError(
                        QStringLiteral("session_not_found"),
                        QStringLiteral("MCP session is unknown or expired"), StatusCode::NotFound));
                }
                if (session->protocolVersion != validatedRequest.protocolVersion) {
                    return readyResponse(transportError(
                        QStringLiteral("session_protocol_mismatch"),
                        QStringLiteral("MCP session protocol version does not match the request"),
                        StatusCode::BadRequest));
                }
            }
            const auto clientId =
                clientIdFor(validatedRequest, request,
                            session ? std::optional<QString>(session->clientId) : std::nullopt);
            if (validatedRequest.method == QString::fromLatin1(Mcp::InitializedNotification)) {
                if (!validatedRequest.notification) {
                    const Mcp::ProtocolError error{
                        Mcp::InvalidRequest,
                        QStringLiteral("notifications/initialized must be a notification")};
                    return readyResponse(
                        jsonResponse(Mcp::makeErrorResponse(validatedRequest.id, error),
                                     protocolErrorStatus(error)));
                }
                if (Mcp::isLegacyProtocolVersion(validatedRequest.protocolVersion) &&
                    !m_transportState->markSessionInitialized(sessionHeaderValues.constFirst(),
                                                              validatedRequest.protocolVersion)) {
                    return readyResponse(transportError(
                        QStringLiteral("session_not_found"),
                        QStringLiteral("MCP session is unknown or expired"), StatusCode::NotFound));
                }
                return readyResponse(QHttpServerResponse(StatusCode::Accepted));
            }
            if (session && Mcp::isLegacyProtocolVersion(validatedRequest.protocolVersion) &&
                !session->initialized &&
                validatedRequest.method != QString::fromLatin1(Mcp::PingMethod)) {
                if (validatedRequest.notification)
                    return readyResponse(QHttpServerResponse(StatusCode::Accepted));
                const Mcp::ProtocolError error{Mcp::ServerNotInitialized,
                                               QStringLiteral("MCP server is not initialized")};
                return readyResponse(
                    jsonResponse(Mcp::makeErrorResponse(validatedRequest.id, error),
                                 protocolErrorStatus(error)));
            }
            if (validatedRequest.notification) {
                if (validatedRequest.method == QString::fromLatin1(Mcp::CancelledNotification)) {
                    const auto requestId =
                        validatedRequest.params.value(QStringLiteral("requestId"));
                    const auto reason = validatedRequest.params.value(QStringLiteral("reason"));
                    if (Mcp::isValidRequestId(requestId) &&
                        (reason.isUndefined() || reason.isString())) {
                        m_transportState->cancelQueued(clientId, validatedRequest.protocolVersion,
                                                       requestId);
                    }
                }
                return readyResponse(QHttpServerResponse(StatusCode::Accepted));
            }
            if (!Mcp::isSupportedCoreMethod(validatedRequest.method)) {
                const Mcp::ProtocolError error{Mcp::MethodNotFound,
                                               QStringLiteral("MCP method is not supported")};
                return readyResponse(jsonResponse(
                    Mcp::makeErrorResponse(validatedRequest.id, error), StatusCode::NotFound));
            }

            auto admission = m_transportState->admit(McpHttpTransportState::Protocol::Mcp);
            if (admission.rejection != McpHttpTransportState::Rejection::None) {
                switch (admission.rejection) {
                    case McpHttpTransportState::Rejection::Stopping:
                        return readyResponse(
                            transportError(QStringLiteral("mcp_stopping"),
                                           QStringLiteral("MCP server is stopping"),
                                           StatusCode::ServiceUnavailable));
                    case McpHttpTransportState::Rejection::GlobalBusy:
                        return readyResponse(transportError(
                            QStringLiteral("busy"),
                            QStringLiteral("Global MCP concurrency limit was reached"),
                            StatusCode::ServiceUnavailable));
                    case McpHttpTransportState::Rejection::None:
                        break;
                }
            }

            const auto pendingId = admission.pending->id;
            if (validatedRequest.method != QString::fromLatin1(Mcp::InitializeMethod)) {
                m_transportState->correlate(pendingId, clientId, validatedRequest.protocolVersion,
                                            validatedRequest.id);
            }
            QTimer::singleShot(
                m_limits.requestDeadlineMs, this, [state = m_transportState, pendingId] {
                    state->complete(
                        pendingId,
                        transportError(QStringLiteral("request_timeout"),
                                       QStringLiteral("MCP request exceeded its deadline"),
                                       StatusCode::GatewayTimeout));
                });

            const auto invoked =
                m_handlerContext &&
                QMetaObject::invokeMethod(
                    m_handlerContext.data(),
                    [state = m_transportState, pendingId, handler = m_handler,
                     validated = validatedRequest, clientId, limits = m_limits]() mutable {
                        if (!state->beginHandling(pendingId))
                            return;
                        QJsonObject responseObject;
                        try {
                            if (handler)
                                responseObject = handler(validated, clientId);
                        } catch (const std::exception &) {
                            responseObject = {};
                        } catch (...) {
                            responseObject = {};
                        }
                        const auto responseValidation = Mcp::validateResponse(
                            responseObject, validated.id, validated.protocolVersion);
                        if (!responseValidation.valid()) {
                            const Mcp::ProtocolError error{
                                Mcp::InternalError,
                                QStringLiteral("MCP handler returned an invalid response")};
                            responseObject = Mcp::makeErrorResponse(validated.id, error);
                        }
                        const auto responseBytes =
                            QJsonDocument(responseObject).toJson(QJsonDocument::Compact);
                        if (responseBytes.size() > limits.maximumResponseBytes) {
                            const Mcp::ProtocolError error{
                                Mcp::InternalError,
                                QStringLiteral("MCP response exceeds the configured limit")};
                            responseObject = Mcp::makeErrorResponse(validated.id, error);
                            state->complete(
                                pendingId,
                                jsonResponse(responseObject, StatusCode::InternalServerError));
                            return;
                        }
                        auto response = jsonResponse(responseObject, StatusCode::Ok);
                        const auto successfulInitialize =
                            validated.method == QString::fromLatin1(Mcp::InitializeMethod) &&
                            responseValidation.valid() && !responseValidation.response->error;
                        if (successfulInitialize) {
                            state->completeInitialize(pendingId, std::move(response), clientId,
                                                      validated.protocolVersion);
                        } else {
                            state->complete(pendingId, std::move(response));
                        }
                    },
                    Qt::QueuedConnection);
            if (!invoked) {
                m_transportState->complete(
                    pendingId,
                    jsonResponse(Mcp::makeErrorResponse(
                                     validatedRequest.id,
                                     {Mcp::InternalError,
                                      QStringLiteral("MCP handler context is unavailable")}),
                                 StatusCode::InternalServerError));
            }
            return admission.future;
        }

        QFuture<QHttpServerResponse> handleNativeRequest(const QHttpServerRequest &request) {
            if (auto rejection =
                    commonRejection(request, McpHttpTransportState::Protocol::Native)) {
                return readyResponse(std::move(*rejection));
            }
            if (!m_routes.native) {
                return readyResponse(transportError(
                    QStringLiteral("route_unavailable"),
                    QStringLiteral("Native automation route is disabled"), StatusCode::NotFound));
            }
            if (request.method() != QHttpServerRequest::Method::Post) {
                auto response = transportError(
                    QStringLiteral("method_not_allowed"),
                    QStringLiteral("Native automation endpoint accepts POST requests"),
                    StatusCode::MethodNotAllowed);
                auto headers = response.headers();
                headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Allow,
                                        QStringLiteral("POST"));
                response.setHeaders(std::move(headers));
                return readyResponse(std::move(response));
            }

            const auto contentType =
                uniqueHeader(request.headers(), QByteArrayLiteral("Content-Type"));
            if (!contentType || !isJsonContentType(*contentType)) {
                return readyResponse(transportError(
                    QStringLiteral("unsupported_media_type"),
                    QStringLiteral("Native automation requests require application/json"),
                    StatusCode::UnsupportedMediaType));
            }
            const auto acceptValues =
                request.headers().values(QHttpHeaders::WellKnownHeader::Accept);
            if (!acceptsJsonMediaType(acceptValues.join(QByteArrayLiteral(",")))) {
                return readyResponse(
                    transportError(QStringLiteral("not_acceptable"),
                                   QStringLiteral("Accept must include application/json"),
                                   StatusCode::NotAcceptable));
            }
            if (request.body().size() > m_limits.maximumRequestBytes) {
                return readyResponse(transportError(
                    QStringLiteral("request_too_large"),
                    QStringLiteral("Native automation request body exceeds the configured limit"),
                    StatusCode::PayloadTooLarge));
            }

            QJsonParseError parseError;
            QByteArray wrapped;
            wrapped.reserve(request.body().size() + 2);
            wrapped.append('[');
            wrapped.append(request.body());
            wrapped.append(']');
            const auto document = QJsonDocument::fromJson(wrapped, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isArray() ||
                document.array().size() != 1) {
                return readyResponse(jsonResponse(
                    nativeProtocolError(-32700, QStringLiteral("Parse error")), StatusCode::Ok));
            }
            const auto message = document.array().at(0);
            if (!withinJsonLimits(message, m_limits.maximumJsonDepth, m_limits.maximumJsonNodes)) {
                const auto response = nativeProtocolError(
                    -32600, QStringLiteral("Invalid Request"), QJsonValue(QJsonValue::Null),
                    QJsonObject{
                        {QStringLiteral("reason"),
                         QStringLiteral("JSON structure limit exceeded")}
                });
                return readyResponse(jsonResponse(response, StatusCode::Ok));
            }

            auto admission = m_transportState->admit(McpHttpTransportState::Protocol::Native);
            if (admission.rejection != McpHttpTransportState::Rejection::None) {
                switch (admission.rejection) {
                    case McpHttpTransportState::Rejection::Stopping:
                        return readyResponse(
                            transportError(QStringLiteral("automation_stopping"),
                                           QStringLiteral("Automation server is stopping"),
                                           StatusCode::ServiceUnavailable));
                    case McpHttpTransportState::Rejection::GlobalBusy:
                        return readyResponse(transportError(
                            QStringLiteral("busy"),
                            QStringLiteral("Global automation concurrency limit was reached"),
                            StatusCode::ServiceUnavailable));
                    case McpHttpTransportState::Rejection::None:
                        break;
                }
            }

            const auto pendingId = admission.pending->id;
            const auto clientId = nativeClientIdFor(request);
            const auto requestId = nativeRequestId(message);
            QTimer::singleShot(
                m_limits.requestDeadlineMs, this, [state = m_transportState, pendingId] {
                    state->complete(
                        pendingId,
                        transportError(QStringLiteral("request_timeout"),
                                       QStringLiteral("Automation request exceeded its deadline"),
                                       StatusCode::GatewayTimeout));
                });

            const auto invoked =
                m_handlerContext &&
                QMetaObject::invokeMethod(
                    m_handlerContext.data(),
                    [state = m_transportState, pendingId, handler = m_nativeHandler, message,
                     clientId, requestId, limits = m_limits]() mutable {
                        if (!state->beginHandling(pendingId))
                            return;
                        QJsonObject responseObject;
                        try {
                            if (handler)
                                responseObject = handler(message, clientId);
                        } catch (const std::exception &) {
                            responseObject = nativeProtocolError(
                                -32603, QStringLiteral("Internal error"), requestId);
                        } catch (...) {
                            responseObject = nativeProtocolError(
                                -32603, QStringLiteral("Internal error"), requestId);
                        }
                        if (!isValidNativeResponse(responseObject, requestId)) {
                            responseObject = nativeProtocolError(
                                -32603, QStringLiteral("Internal error"), requestId);
                        }
                        auto responseBytes =
                            QJsonDocument(responseObject).toJson(QJsonDocument::Compact);
                        if (responseBytes.size() > limits.maximumResponseBytes) {
                            responseObject = nativeProtocolError(
                                -32603,
                                QStringLiteral("Automation response exceeds the configured limit"),
                                requestId);
                            responseBytes =
                                QJsonDocument(responseObject).toJson(QJsonDocument::Compact);
                        }
                        if (responseBytes.size() > limits.maximumResponseBytes) {
                            state->complete(
                                pendingId,
                                transportError(
                                    QStringLiteral("response_too_large"),
                                    QStringLiteral(
                                        "Automation response exceeds the configured limit"),
                                    StatusCode::InternalServerError));
                            return;
                        }
                        state->complete(pendingId, jsonResponse(responseObject, StatusCode::Ok));
                    },
                    Qt::QueuedConnection);
            if (!invoked) {
                const auto response = nativeProtocolError(
                    -32603, QStringLiteral("Automation handler context is unavailable"), requestId);
                m_transportState->complete(pendingId, jsonResponse(response, StatusCode::Ok));
            }
            return admission.future;
        }

        QPointer<QObject> m_handlerContext;
        RequestHandler m_handler;
        NativeRequestHandler m_nativeHandler;
        McpHttpLimits m_limits;
        std::shared_ptr<McpHttpTransportState> m_transportState;
        QHttpServer *m_httpServer = nullptr;
        QTcpServer *m_tcpServer = nullptr;
        AutomationHttpRoutes m_routes;
    };

    McpHttpServer::McpHttpServer(RequestHandler handler, QObject *parent)
        : McpHttpServer(nullptr, std::move(handler), {}, McpHttpLimits{}, parent) {
    }

    McpHttpServer::McpHttpServer(RequestHandler handler, McpHttpLimits limits, QObject *parent)
        : McpHttpServer(nullptr, std::move(handler), {}, std::move(limits), parent) {
    }

    McpHttpServer::McpHttpServer(QObject *handlerContext, RequestHandler handler, QObject *parent)
        : McpHttpServer(handlerContext, std::move(handler), {}, McpHttpLimits{}, parent) {
    }

    McpHttpServer::McpHttpServer(QObject *handlerContext, RequestHandler handler,
                                 McpHttpLimits limits, QObject *parent)
        : McpHttpServer(handlerContext, std::move(handler), {}, std::move(limits), parent) {
    }

    McpHttpServer::McpHttpServer(QObject *handlerContext, RequestHandler handler,
                                 NativeRequestHandler nativeHandler, QObject *parent)
        : McpHttpServer(handlerContext, std::move(handler), std::move(nativeHandler),
                        McpHttpLimits{}, parent) {
    }

    McpHttpServer::McpHttpServer(QObject *handlerContext, RequestHandler handler,
                                 NativeRequestHandler nativeHandler, McpHttpLimits limits,
                                 QObject *parent)
        : QObject(parent), m_handler(std::move(handler)), m_nativeHandler(std::move(nativeHandler)),
          m_limits(std::move(limits)), m_handlerContext(handlerContext ? handlerContext : this) {
        m_limits.maximumRequestBytes =
            std::clamp<qsizetype>(m_limits.maximumRequestBytes, 1024, maximumTransportBodyBytes);
        m_limits.maximumResponseBytes = std::max<qsizetype>(
            {1024, m_limits.maximumResponseBytes, m_limits.maximumRequestBytes + 1024});
        m_limits.maximumJsonDepth = std::max(8, m_limits.maximumJsonDepth);
        m_limits.maximumJsonNodes = std::max<qsizetype>(128, m_limits.maximumJsonNodes);
        m_limits.maximumGlobalInFlight = std::max(1, m_limits.maximumGlobalInFlight);
        m_limits.requestDeadlineMs = std::max(10, m_limits.requestDeadlineMs);
        m_transportState = std::make_shared<McpHttpTransportState>(m_limits);
    }

    McpHttpServer::~McpHttpServer() {
        stop();
    }

    bool McpHttpServer::start(const quint16 requestedPort, QString &error) {
        return start(requestedPort, {.mcp = true, .native = false}, error);
    }

    bool McpHttpServer::start(const quint16 requestedPort, const AutomationHttpRoutes routes,
                              QString &error) {
        error.clear();
        if (m_thread || m_worker || m_stopping) {
            error = QStringLiteral("Automation HTTP server is already running");
            return false;
        }
        if ((!routes.mcp && !routes.native) || (routes.mcp && !m_handler) ||
            (routes.native && !m_nativeHandler)) {
            error = QStringLiteral("Automation HTTP routes are not configured");
            return false;
        }

        m_thread = new QThread;
        m_routes = routes;
        m_accepting.store(false, std::memory_order_release);
        m_transportState->stopAccepting();
        m_worker =
            new Worker(m_handlerContext, m_handler, m_nativeHandler, m_limits, m_transportState);
        m_worker->moveToThread(m_thread);
        m_thread->start();

        bool started = false;
        QMetaObject::invokeMethod(
            m_worker,
            [this, requestedPort, routes, &started, &error] {
                started = m_worker->start(requestedPort, routes, m_port, error);
            },
            Qt::BlockingQueuedConnection);
        if (!started) {
            stop();
            return false;
        }
        m_transportState->startAccepting();
        m_accepting.store(true, std::memory_order_release);
        return true;
    }

    bool McpHttpServer::setRoutes(const AutomationHttpRoutes routes, QString &error) {
        error.clear();
        if (QThread::currentThread() != thread()) {
            error = QStringLiteral("Automation HTTP routes must be updated on the owner thread");
            return false;
        }
        if (!isListening() || m_stopping || !m_thread || !m_worker) {
            error = QStringLiteral("Automation HTTP server is not listening");
            return false;
        }

        bool updated = false;
        QMetaObject::invokeMethod(
            m_worker,
            [this, routes, &updated, &error] { updated = m_worker->setRoutes(routes, error); },
            Qt::BlockingQueuedConnection);
        if (updated)
            m_routes = routes;
        return updated;
    }

    void McpHttpServer::requestStop() {
        if (QThread::currentThread() != thread()) {
            QMetaObject::invokeMethod(this, [this] { requestStop(); }, Qt::QueuedConnection);
            return;
        }
        m_accepting.store(false, std::memory_order_release);
        m_transportState->stopAccepting();
        m_port = 0;
        if (!m_thread || !m_worker) {
            emit stopped();
            return;
        }
        if (m_stopping)
            return;
        m_stopping = true;
        auto *ownerThread = thread();
        QMetaObject::invokeMethod(
            m_worker,
            [this, ownerThread] {
                m_worker->stop();
                m_worker->moveToThread(ownerThread);
                QMetaObject::invokeMethod(
                    this, [this] { finalizeAsyncStop(); }, Qt::QueuedConnection);
            },
            Qt::QueuedConnection);
    }

    void McpHttpServer::stop() {
        m_accepting.store(false, std::memory_order_release);
        m_transportState->stopAccepting();
        if (m_stopping) {
            if (m_thread && m_worker) {
                QEventLoop loop;
                connect(this, &McpHttpServer::stopped, &loop, &QEventLoop::quit,
                        Qt::SingleShotConnection);
                loop.exec();
            }
            return;
        }
        if (!m_thread || !m_worker) {
            m_port = 0;
            m_routes = {};
            return;
        }
        auto *ownerThread = thread();
        QMetaObject::invokeMethod(
            m_worker,
            [this, ownerThread] {
                m_worker->stop();
                m_worker->moveToThread(ownerThread);
            },
            Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
        delete m_worker;
        delete m_thread;
        m_worker = nullptr;
        m_thread = nullptr;
        m_port = 0;
        m_routes = {};
    }

    void McpHttpServer::finalizeAsyncStop() {
        if (!m_stopping)
            return;
        m_thread->quit();
        m_thread->wait();
        delete m_worker;
        delete m_thread;
        m_worker = nullptr;
        m_thread = nullptr;
        m_port = 0;
        m_routes = {};
        m_stopping = false;
        emit stopped();
    }

    bool McpHttpServer::isListening() const {
        return m_thread && m_worker && m_port != 0 && m_accepting.load(std::memory_order_acquire);
    }

    bool McpHttpServer::isStopping() const {
        return m_stopping;
    }

    quint16 McpHttpServer::port() const {
        return m_port;
    }

    QString McpHttpServer::endpoint() const {
        if (!isListening() || !m_routes.mcp)
            return {};
        return QStringLiteral("http://127.0.0.1:%1/mcp").arg(m_port);
    }

    QString McpHttpServer::nativeEndpoint() const {
        if (!isListening() || !m_routes.native)
            return {};
        return QStringLiteral("http://127.0.0.1:%1/automation/v1").arg(m_port);
    }

    AutomationHttpRoutes McpHttpServer::routes() const {
        return m_routes;
    }

} // namespace Automation
