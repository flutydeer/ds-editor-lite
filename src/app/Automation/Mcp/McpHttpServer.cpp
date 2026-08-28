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
#include <QMutex>
#include <QPointer>
#include <QPromise>
#include <QTcpServer>
#include <QThread>
#include <QTimer>
#include <QUrl>

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
                            const QHttpServerRequest &httpRequest) {
            const auto connectorMetadata = request.meta.value(
                QStringLiteral("com.openvpi.ds-editor-lite/connectorInstanceId"));
            QJsonObject identity{
                {QStringLiteral("remoteAddress"), httpRequest.remoteAddress().toString()},
            };
            if (request.clientInfo)
                identity.insert(QStringLiteral("clientInfo"), request.clientInfo->toJson());
            if (connectorMetadata.isString() && !connectorMetadata.toString().isEmpty() &&
                connectorMetadata.toString().size() <= 128) {
                identity.insert(QStringLiteral("connectorMetadata"), connectorMetadata.toString());
            }
            const auto digest = QCryptographicHash::hash(
                QJsonDocument(identity).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256);
            return QStringLiteral("mcp-%1").arg(QString::fromLatin1(digest.toHex().left(24)));
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
        enum class Rejection {
            None,
            Stopping,
            GlobalBusy,
        };

        struct PendingRequest {
            quint64 id = 0;
            QPromise<QHttpServerResponse> promise;
        };

        struct Admission {
            Rejection rejection = Rejection::None;
            std::shared_ptr<PendingRequest> pending;
            QFuture<QHttpServerResponse> future;
        };

        explicit McpHttpTransportState(McpHttpLimits limits) : m_limits(std::move(limits)) {
        }

        void startAccepting() {
            const QMutexLocker locker(&m_mutex);
            m_accepting = true;
            m_globalInFlight = 0;
            m_pending.clear();
        }

        [[nodiscard]] bool isAccepting() const {
            const QMutexLocker locker(&m_mutex);
            return m_accepting;
        }

        Admission admit() {
            const QMutexLocker locker(&m_mutex);
            if (!m_accepting)
                return {.rejection = Rejection::Stopping};
            if (m_globalInFlight >= m_limits.maximumGlobalInFlight)
                return {.rejection = Rejection::GlobalBusy};
            auto pending = std::make_shared<PendingRequest>();
            pending->id = m_nextRequestId++;
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

        [[nodiscard]] bool isPending(const quint64 id) const {
            const QMutexLocker locker(&m_mutex);
            return m_pending.contains(id);
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

        void stopAccepting() {
            QList<std::shared_ptr<PendingRequest>> pending;
            {
                const QMutexLocker locker(&m_mutex);
                m_accepting = false;
                pending = m_pending.values();
                m_pending.clear();
                m_globalInFlight = 0;
            }
            for (const auto &request : pending) {
                request->promise.addResult(transportError(QStringLiteral("mcp_stopping"),
                                                          QStringLiteral("MCP server is stopping"),
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
        quint64 m_nextRequestId = 1;
    };

    class McpHttpServer::Worker final : public QObject {
    public:
        Worker(QObject *handlerContext, RequestHandler handler, McpHttpLimits limits,
               std::shared_ptr<McpHttpTransportState> transportState)
            : m_handlerContext(handlerContext), m_handler(std::move(handler)), m_limits(limits),
              m_transportState(std::move(transportState)) {
        }

        bool start(const quint16 requestedPort, quint16 &actualPort, QString &error) {
            if (m_httpServer || m_tcpServer) {
                error = QStringLiteral("MCP HTTP server is already running");
                return false;
            }

            auto *httpServer = new QHttpServer(this);
            auto *tcpServer = new QTcpServer(this);
            auto configuration = httpServer->configuration();
            configuration.setMaximumBodySize(maximumTransportBodyBytes);
            configuration.setKeepAliveTimeout(
                std::chrono::seconds(std::max(1, (m_limits.requestDeadlineMs + 999) / 1000)));
            httpServer->setConfiguration(configuration);
            httpServer->route(
                QStringLiteral("/mcp"), QHttpServerRequest::Method::AnyKnown,
                [this](QHttpServerRequest request) { return handleRequest(request); });
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
                error = QStringLiteral("Failed to bind the MCP HTTP server to its TCP listener");
                tcpServer->close();
                delete tcpServer;
                delete httpServer;
                return false;
            }

            m_httpServer = httpServer;
            m_tcpServer = tcpServer;
            actualPort = tcpServer->serverPort();
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
        QFuture<QHttpServerResponse> handleRequest(const QHttpServerRequest &request) {
            if (!request.remoteAddress().isLoopback() ||
                request.localAddress() != QHostAddress(QStringLiteral("127.0.0.1"))) {
                return readyResponse(transportError(QStringLiteral("forbidden"),
                                                    QStringLiteral("MCP endpoint is local-only"),
                                                    StatusCode::Forbidden));
            }
            const auto host = uniqueHeader(request.headers(), QByteArrayLiteral("Host"));
            if (!host || !hasExpectedHost(*host, request.localPort())) {
                return readyResponse(
                    transportError(QStringLiteral("invalid_host"),
                                   QStringLiteral("Host header does not match the MCP endpoint"),
                                   StatusCode::Forbidden));
            }
            const auto origins = request.headers().values(QHttpHeaders::WellKnownHeader::Origin);
            if (origins.size() > 1 ||
                (origins.size() == 1 &&
                 !isAllowedOrigin(origins.constFirst(), request.localPort()))) {
                return readyResponse(
                    transportError(QStringLiteral("origin_forbidden"),
                                   QStringLiteral("Origin is not an allowed localhost origin"),
                                   StatusCode::Forbidden));
            }
            if (!m_transportState->isAccepting()) {
                return readyResponse(transportError(QStringLiteral("mcp_stopping"),
                                                    QStringLiteral("MCP server is stopping"),
                                                    StatusCode::ServiceUnavailable));
            }

            auto admission = m_transportState->admit();
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
            const auto fail = [state = m_transportState, pendingId,
                               future = admission.future](QHttpServerResponse response) {
                state->complete(pendingId, std::move(response));
                return future;
            };

            if (request.method() != QHttpServerRequest::Method::Post) {
                auto response =
                    transportError(QStringLiteral("method_not_allowed"),
                                   QStringLiteral("MCP endpoint accepts POST requests only"),
                                   StatusCode::MethodNotAllowed);
                auto headers = response.headers();
                headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::Allow,
                                        QStringLiteral("POST"));
                response.setHeaders(std::move(headers));
                return fail(std::move(response));
            }

            const auto contentType =
                uniqueHeader(request.headers(), QByteArrayLiteral("Content-Type"));
            if (!contentType || !isJsonContentType(*contentType)) {
                return fail(transportError(QStringLiteral("unsupported_media_type"),
                                           QStringLiteral("MCP requests require application/json"),
                                           StatusCode::UnsupportedMediaType));
            }
            const auto acceptValues =
                request.headers().values(QHttpHeaders::WellKnownHeader::Accept);
            if (!acceptsRequiredMediaTypes(acceptValues.join(QByteArrayLiteral(",")))) {
                return fail(transportError(
                    QStringLiteral("not_acceptable"),
                    QStringLiteral("Accept must include application/json and text/event-stream"),
                    StatusCode::NotAcceptable));
            }
            if (request.body().size() > m_limits.maximumRequestBytes) {
                return fail(
                    transportError(QStringLiteral("request_too_large"),
                                   QStringLiteral("MCP request body exceeds the configured limit"),
                                   StatusCode::PayloadTooLarge));
            }

            QJsonParseError parseError;
            const auto document = QJsonDocument::fromJson(request.body(), &parseError);
            if (parseError.error != QJsonParseError::NoError) {
                return fail(jsonResponse(
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
                    return fail(jsonResponse(
                        Mcp::makeErrorResponse(
                            rawId, {Mcp::HeaderMismatch,
                                    QStringLiteral(
                                        "Required MCP request metadata is missing or malformed")}),
                        StatusCode::BadRequest));
                }
            }

            if (!withinJsonLimits(message, m_limits.maximumJsonDepth, m_limits.maximumJsonNodes)) {
                return fail(jsonResponse(
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
                return fail(jsonResponse(Mcp::makeErrorResponse(id, validation.error),
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
                return fail(jsonResponse(Mcp::makeErrorResponse(validatedRequest.id, error),
                                         protocolErrorStatus(error)));
            }
            const auto metadataValidation =
                Mcp::validateTransportMetadata(metadata, validatedRequest);
            if (!metadataValidation.valid()) {
                return fail(jsonResponse(
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
                return fail(jsonResponse(Mcp::makeErrorResponse(validatedRequest.id, error),
                                         protocolErrorStatus(error)));
            }
            const auto clientId = clientIdFor(validatedRequest, request);
            if (validatedRequest.notification)
                return fail(QHttpServerResponse(StatusCode::Accepted));
            if (!Mcp::isSupportedCoreMethod(validatedRequest.method)) {
                const Mcp::ProtocolError error{Mcp::MethodNotFound,
                                               QStringLiteral("MCP method is not supported")};
                return fail(jsonResponse(Mcp::makeErrorResponse(validatedRequest.id, error),
                                         StatusCode::NotFound));
            }
            QTimer::singleShot(
                m_limits.requestDeadlineMs, this, [state = m_transportState, pendingId] {
                    state->complete(
                        pendingId,
                        transportError(QStringLiteral("request_timeout"),
                                       QStringLiteral("MCP request exceeded its deadline"),
                                       StatusCode::GatewayTimeout));
                });

            const auto invoked = QMetaObject::invokeMethod(
                m_handlerContext,
                [state = m_transportState, pendingId, handler = m_handler,
                 validated = validatedRequest, clientId, limits = m_limits]() mutable {
                    if (!state->isPending(pendingId))
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
                        state->complete(pendingId, jsonResponse(responseObject,
                                                                StatusCode::InternalServerError));
                        return;
                    }
                    state->complete(pendingId, jsonResponse(responseObject, StatusCode::Ok));
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

        QObject *m_handlerContext = nullptr;
        RequestHandler m_handler;
        McpHttpLimits m_limits;
        std::shared_ptr<McpHttpTransportState> m_transportState;
        QHttpServer *m_httpServer = nullptr;
        QTcpServer *m_tcpServer = nullptr;
    };

    McpHttpServer::McpHttpServer(RequestHandler handler, QObject *parent)
        : McpHttpServer(nullptr, std::move(handler), McpHttpLimits{}, parent) {
    }

    McpHttpServer::McpHttpServer(RequestHandler handler, McpHttpLimits limits, QObject *parent)
        : McpHttpServer(nullptr, std::move(handler), std::move(limits), parent) {
    }

    McpHttpServer::McpHttpServer(QObject *handlerContext, RequestHandler handler, QObject *parent)
        : McpHttpServer(handlerContext, std::move(handler), McpHttpLimits{}, parent) {
    }

    McpHttpServer::McpHttpServer(QObject *handlerContext, RequestHandler handler,
                                 McpHttpLimits limits, QObject *parent)
        : QObject(parent), m_handler(std::move(handler)), m_limits(std::move(limits)),
          m_handlerContext(handlerContext ? handlerContext : this) {
        m_limits.maximumRequestBytes =
            std::clamp<qsizetype>(m_limits.maximumRequestBytes, 1024, maximumTransportBodyBytes);
        m_limits.maximumResponseBytes = std::max<qsizetype>(1024, m_limits.maximumResponseBytes);
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
        error.clear();
        if (m_thread || m_worker || m_stopping) {
            error = QStringLiteral("MCP HTTP server is already running");
            return false;
        }

        m_thread = new QThread;
        m_accepting.store(false, std::memory_order_release);
        m_transportState->stopAccepting();
        m_worker = new Worker(m_handlerContext, m_handler, m_limits, m_transportState);
        m_worker->moveToThread(m_thread);
        m_thread->start();

        bool started = false;
        QMetaObject::invokeMethod(
            m_worker,
            [this, requestedPort, &started, &error] {
                started = m_worker->start(requestedPort, m_port, error);
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
        if (!isListening())
            return {};
        return QStringLiteral("http://127.0.0.1:%1/mcp").arg(m_port);
    }

} // namespace Automation
