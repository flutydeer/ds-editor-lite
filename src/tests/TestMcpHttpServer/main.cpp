#include "Automation/Mcp/McpHttpServer.h"

#include <lite/AutomationWire/McpProtocol.h>

#include <QCoreApplication>
#include <QtTest>
#include "../TestSupport/TestAssertions.h"
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSemaphore>
#include <QScopeGuard>
#include <QTcpSocket>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include <stdexcept>
#include <utility>

namespace {
    namespace Mcp = AutomationWire::Mcp;

    using TestSupport::expect;

    struct HttpResult {
        int status = 0;
        QByteArray body;
        QByteArray cacheControl;
        QByteArray contentType;
        QByteArray contentTypeOptions;
        QByteArray corsOrigin;
        QByteArray sessionId;
        QNetworkReply::NetworkError networkError = QNetworkReply::NoError;
        bool timedOut = false;
    };

    enum class HttpMethod {
        Post,
        Get,
        Delete,
    };

    QNetworkReply *startRequest(QNetworkAccessManager &manager, QNetworkRequest request,
                                const QByteArray &body = {},
                                const HttpMethod method = HttpMethod::Post) {
        switch (method) {
            case HttpMethod::Post:
                return manager.post(request, body);
            case HttpMethod::Get:
                return manager.get(request);
            case HttpMethod::Delete:
                return manager.deleteResource(request);
        }
        return nullptr;
    }

    HttpResult finishRequest(QNetworkReply *reply, const int timeoutMs = 5000) {
        if (!reply)
            return {.timedOut = true};
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(timeoutMs);
        loop.exec();

        HttpResult result;
        if (!reply->isFinished()) {
            result.timedOut = true;
            reply->abort();
        }
        result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.body = reply->readAll();
        result.cacheControl = reply->rawHeader("Cache-Control");
        result.contentType = reply->rawHeader("Content-Type");
        result.contentTypeOptions = reply->rawHeader("X-Content-Type-Options");
        result.corsOrigin = reply->rawHeader("Access-Control-Allow-Origin");
        result.sessionId = reply->rawHeader("Mcp-Session-Id");
        result.networkError = reply->error();
        reply->deleteLater();
        return result;
    }

    HttpResult send(QNetworkAccessManager &manager, QNetworkRequest request,
                    const QByteArray &body = {}, const HttpMethod method = HttpMethod::Post,
                    const int timeoutMs = 5000) {
        return finishRequest(startRequest(manager, std::move(request), body, method), timeoutMs);
    }

    bool acquireWhileProcessing(QSemaphore &semaphore, const int timeoutMs) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            if (semaphore.tryAcquire())
                return true;
            QCoreApplication::processEvents();
            QThread::msleep(1);
        }
        return semaphore.tryAcquire();
    }

    int rawHttpStatus(const quint16 port, const QByteArray &request, const int timeoutMs = 2000) {
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, port);
        if (!socket.waitForConnected(timeoutMs) || socket.write(request) != request.size() ||
            !socket.waitForBytesWritten(timeoutMs) || !socket.waitForReadyRead(timeoutMs)) {
            return 0;
        }
        const auto statusLine = socket.readLine().trimmed().split(' ');
        return statusLine.size() >= 2 ? statusLine.at(1).toInt() : 0;
    }

    bool waitForStop(Automation::McpHttpServer &server, const int timeoutMs = 2000) {
        if (!server.isListening() && !server.isStopping())
            return true;
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&server, &Automation::McpHttpServer::stopped, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(timeoutMs);
        if (server.isListening() || server.isStopping())
            loop.exec();
        return !timeout.isActive() ? false : !server.isListening() && !server.isStopping();
    }

    QNetworkRequest baseRequest(const QUrl &url, const QString &method, const QString &name = {}) {
        QNetworkRequest request(url);
        request.setRawHeader("Content-Type", "application/json; charset=utf-8");
        request.setRawHeader("Accept", "application/json, text/event-stream");
        request.setRawHeader("MCP-Protocol-Version", Mcp::ProtocolVersion);
        request.setRawHeader("Mcp-Method", method.toLatin1());
        if (!name.isEmpty())
            request.setRawHeader("Mcp-Name", Mcp::encodeHeaderValue(name).toLatin1());
        return request;
    }

    QNetworkRequest nativeRequest(const QUrl &url) {
        QNetworkRequest request(url);
        request.setRawHeader("Content-Type", "application/json; charset=utf-8");
        request.setRawHeader("Accept", "application/json");
        return request;
    }

    QNetworkRequest legacyRequest(const QUrl &url, const bool includeProtocolHeader = true,
                                  const char *protocolVersion = Mcp::LegacyProtocolVersion,
                                  const QByteArray &sessionId = {}) {
        QNetworkRequest request(url);
        request.setRawHeader("Content-Type", "application/json; charset=utf-8");
        request.setRawHeader("Accept", "application/json, text/event-stream");
        if (includeProtocolHeader) {
            request.setRawHeader("MCP-Protocol-Version", QByteArray(protocolVersion));
        }
        if (!sessionId.isEmpty())
            request.setRawHeader("MCP-Session-Id", sessionId);
        return request;
    }

    QJsonObject requestObject(const QString &method, const QJsonValue &id,
                              QJsonObject params = {}) {
        Mcp::RequestContext context;
        context.clientCapabilities = {};
        context.clientInfo = Mcp::ImplementationInfo{
            QStringLiteral("TestMcpHttpServer"),
            QStringLiteral("1.0"),
            {},
            {},
        };
        return Mcp::makeRequest(method, std::move(params), context, id);
    }

    QJsonObject withConnectorInstanceId(QJsonObject request, const QString &instanceId) {
        auto params = request.value(QStringLiteral("params")).toObject();
        auto meta = params.value(QStringLiteral("_meta")).toObject();
        meta.insert(QString::fromLatin1(Mcp::ConnectorInstanceIdMetaKey), instanceId);
        params.insert(QStringLiteral("_meta"), meta);
        request.insert(QStringLiteral("params"), params);
        return request;
    }

    QJsonObject bodyObject(const HttpResult &result) {
        return QJsonDocument::fromJson(result.body).object();
    }

    int jsonRpcErrorCode(const HttpResult &result) {
        return bodyObject(result)
            .value(QStringLiteral("error"))
            .toObject()
            .value(QStringLiteral("code"))
            .toInt();
    }

    class McpServerFixture final {
    public:
        QMutex observationMutex;
        QString observedClientId;
        QString observedMethod;
        const Mcp::ImplementationInfo serverInfo{QStringLiteral("DS Editor Lite Test"),
                                                 QStringLiteral("1.0"),
                                                 {},
                                                 {}};
        Automation::McpHttpLimits limits = [] {
            Automation::McpHttpLimits value;
            value.maximumRequestBytes = 1024;
            value.maximumResponseBytes = 4096;
            value.maximumJsonNodes = 128;
            value.maximumLegacySessions = 2;
            return value;
        }();
        Automation::McpHttpServer server;
        QNetworkAccessManager manager;
        QString error;

        McpServerFixture()
            : server(
                  [this](const Mcp::RequestEnvelope &request, const QString &clientId) {
                      {
                          const QMutexLocker locker(&observationMutex);
                          observedClientId = clientId;
                          observedMethod = request.method;
                      }
                      if (request.method == QString::fromLatin1(Mcp::InitializeMethod)) {
                          return Mcp::makeResultResponse(
                              request.id,
                              Mcp::makeInitializeResult(request.protocolVersion, serverInfo), {},
                              request.protocolVersion);
                      }
                      if (request.method == QString::fromLatin1(Mcp::PingMethod)) {
                          return Mcp::makeResultResponse(request.id, {}, serverInfo,
                                                         request.protocolVersion);
                      }
                      if (request.method == QString::fromLatin1(Mcp::DiscoverMethod)) {
                          return Mcp::makeResultResponse(request.id,
                                                         Mcp::makeDiscoverResult(serverInfo),
                                                         serverInfo, request.protocolVersion);
                      }
                      if (request.method == QString::fromLatin1(Mcp::ToolsListMethod)) {
                          return Mcp::makeResultResponse(
                              request.id,
                              Mcp::makeToolsListResult({}, {}, 0, QStringLiteral("private"),
                                                       serverInfo, request.protocolVersion),
                              serverInfo, request.protocolVersion);
                      }
                      if (request.name == QStringLiteral("invalid.response"))
                          return QJsonObject{};
                      if (request.name == QStringLiteral("large.response")) {
                          return Mcp::makeResultResponse(
                              request.id,
                              Mcp::makeToolCallResult(
                                  QJsonObject{
                                      {QStringLiteral("payload"), QString(5000, u'x')}
                          },
                                  false, {}, {}, request.protocolVersion),
                              serverInfo, request.protocolVersion);
                      }
                      return Mcp::makeResultResponse(request.id,
                                                     Mcp::makeToolCallResult(
                                                         QJsonObject{
                                                             {QStringLiteral("ok"), true}
                      },
                                                         false, QStringLiteral("ok"), serverInfo,
                                                         request.protocolVersion),
                                                     serverInfo, request.protocolVersion);
                  },
                  limits) {
            manager.setProxy(QNetworkProxy::NoProxy);
        }

        bool start() {
            return server.start(0, error);
        }

        QUrl endpoint() const {
            return QUrl(server.endpoint());
        }
    };

    class NativeServerFixture final {
    public:
        const Mcp::ImplementationInfo serverInfo{QStringLiteral("DS Editor Lite Test"),
                                                 QStringLiteral("1.0"),
                                                 {},
                                                 {}};
        Automation::McpHttpLimits limits = [] {
            Automation::McpHttpLimits value;
            value.maximumRequestBytes = 1024;
            value.maximumJsonDepth = 16;
            value.maximumJsonNodes = 128;
            return value;
        }();
        Automation::McpHttpServer server;
        QNetworkAccessManager manager;
        QString error;

        NativeServerFixture()
            : server(
                  QCoreApplication::instance(),
                  Automation::McpHttpServer::RequestHandler(
                      [this](const Mcp::RequestEnvelope &request, const QString &) {
                          return Mcp::makeResultResponse(request.id,
                                                         Mcp::makeDiscoverResult(serverInfo),
                                                         serverInfo, request.protocolVersion);
                      }),
                  Automation::McpHttpServer::NativeRequestHandler([](const QJsonValue &message,
                                                                     const QString &clientId) {
                      if (!message.isObject()) {
                          return QJsonObject{
                              {QStringLiteral("jsonrpc"), QStringLiteral("2.0")       },
                              {QStringLiteral("id"),      QJsonValue(QJsonValue::Null)},
                              {QStringLiteral("error"),
                               QJsonObject{
                                   {QStringLiteral("code"), -32600},
                                   {QStringLiteral("message"), QStringLiteral("Invalid Request")},
                               }                                                      },
                          };
                      }
                      const auto request = message.toObject();
                      const auto method = request.value(QStringLiteral("method")).toString();
                      if (method == QStringLiteral("test.throw"))
                          throw std::runtime_error("native handler failure");
                      if (method == QStringLiteral("test.invalid_response")) {
                          return QJsonObject{
                              {QStringLiteral("jsonrpc"), QStringLiteral("2.0")     },
                              {QStringLiteral("id"),      QStringLiteral("wrong-id")},
                              {QStringLiteral("result"),  QJsonObject{}             },
                          };
                      }
                      return QJsonObject{
                          {QStringLiteral("jsonrpc"), QStringLiteral("2.0")              },
                          {QStringLiteral("id"),      request.value(QStringLiteral("id"))},
                          {QStringLiteral("result"),
                           QJsonObject{
                               {QStringLiteral("params"),
                                request.value(QStringLiteral("params")).toObject()},
                               {QStringLiteral("client_id_present"), !clientId.isEmpty()},
                           }                                                             },
                      };
                  }),
                  limits) {
            manager.setProxy(QNetworkProxy::NoProxy);
        }

        bool start() {
            return server.start(0, {.mcp = true, .native = true}, error);
        }
    };

    class HandlerThreadFixture final {
    public:
        QThread thread;
        QObject context;

        HandlerThreadFixture() {
            context.moveToThread(&thread);
            thread.start();
        }

        ~HandlerThreadFixture() {
            QMetaObject::invokeMethod(
                &context, [&] { context.moveToThread(QCoreApplication::instance()->thread()); },
                Qt::BlockingQueuedConnection);
            thread.quit();
            expect(thread.wait(2000),
                   QStringLiteral("handler executor must stop within its timeout"));
        }
    };
}

class TestMcpHttpServer final : public QObject {
    Q_OBJECT
private slots:
    void modernClientIdentity();
    void legacySessionLifecycle();
    void httpAdmission_data();
    void httpAdmission();
    void emptyOriginRejected();
    void transportMetadataRouting();
    void jsonAndRequestLimits();
    void handlerResponseLimits();
    void listenerLifecycle();
    void responseSurvivesShutdown_data();
    void responseSurvivesShutdown();
    void nativeRequestValidation();
    void nativeMcpRouteLifecycle();
    void nativeResponseLimit();
    void defaultRequestDeadline();
    void sharedAdmissionAndDeadline();
    void crossConnectionCancellation_data();
    void crossConnectionCancellation();
};

void TestMcpHttpServer::modernClientIdentity() {
    McpServerFixture fixture;
    QVERIFY2(fixture.start(), qPrintable(fixture.error));
    auto &server = fixture.server;
    auto &manager = fixture.manager;
    const auto endpoint = fixture.endpoint();
    auto &observationMutex = fixture.observationMutex;
    auto &observedClientId = fixture.observedClientId;
    auto &observedMethod = fixture.observedMethod;
    const auto discover =
        withConnectorInstanceId(requestObject(QString::fromLatin1(Mcp::DiscoverMethod), 1),
                                QStringLiteral("connector-test-instance"));

    const auto success =
        send(manager, baseRequest(endpoint, QString::fromLatin1(Mcp::DiscoverMethod)),
             QJsonDocument(discover).toJson(QJsonDocument::Compact));
    expect(!success.timedOut && success.status == 200 &&
               bodyObject(success)
                       .value(QStringLiteral("result"))
                       .toObject()
                       .value(QStringLiteral("resultType")) == QStringLiteral("complete"),
           QStringLiteral("valid server/discover must return a complete JSON-RPC result"));
    expect(
        success.cacheControl == QByteArrayLiteral("no-store") &&
            success.contentType.startsWith(QByteArrayLiteral("application/json")) &&
            success.contentTypeOptions == QByteArrayLiteral("nosniff") &&
            success.corsOrigin.isEmpty() && success.sessionId.isEmpty(),
        QStringLiteral("responses must be non-cacheable JSON without CORS or a legacy session id"));
    {
        const QMutexLocker locker(&observationMutex);
        expect(
            observedClientId.startsWith(QStringLiteral("mcp-")) &&
                observedClientId != QStringLiteral("connector-test-instance") &&
                observedMethod == QString::fromLatin1(Mcp::DiscoverMethod),
            QStringLiteral("a server-derived client identity and method must reach the handler"));
    }

    QNetworkAccessManager directManagerA;
    QNetworkAccessManager directManagerB;
    directManagerA.setProxy(QNetworkProxy::NoProxy);
    directManagerB.setProxy(QNetworkProxy::NoProxy);
    const auto directDiscoverA =
        requestObject(QString::fromLatin1(Mcp::DiscoverMethod), QStringLiteral("direct-a"));
    const auto directResultA =
        send(directManagerA, baseRequest(endpoint, QString::fromLatin1(Mcp::DiscoverMethod)),
             QJsonDocument(directDiscoverA).toJson(QJsonDocument::Compact));
    QString directClientA;
    {
        const QMutexLocker locker(&observationMutex);
        directClientA = observedClientId;
    }
    const auto directDiscoverB =
        requestObject(QString::fromLatin1(Mcp::DiscoverMethod), QStringLiteral("direct-b"));
    const auto directResultB =
        send(directManagerB, baseRequest(endpoint, QString::fromLatin1(Mcp::DiscoverMethod)),
             QJsonDocument(directDiscoverB).toJson(QJsonDocument::Compact));
    QString directClientB;
    {
        const QMutexLocker locker(&observationMutex);
        directClientB = observedClientId;
    }
    expect(directResultA.status == 200 && directResultB.status == 200 && !directClientA.isEmpty() &&
               !directClientB.isEmpty() && directClientA != directClientB,
           QStringLiteral("independent direct clients must have isolated connection identities"));

    auto distinctDirectDiscover =
        requestObject(QString::fromLatin1(Mcp::DiscoverMethod), QStringLiteral("direct-distinct"));
    auto distinctParams = distinctDirectDiscover.value(QStringLiteral("params")).toObject();
    auto distinctMeta = distinctParams.value(QStringLiteral("_meta")).toObject();
    auto distinctClientInfo =
        distinctMeta.value(QString::fromLatin1(Mcp::ClientInfoMetaKey)).toObject();
    distinctClientInfo.insert(QStringLiteral("name"), QStringLiteral("DistinctMcpClient"));
    distinctMeta.insert(QString::fromLatin1(Mcp::ClientInfoMetaKey), distinctClientInfo);
    distinctParams.insert(QStringLiteral("_meta"), distinctMeta);
    distinctDirectDiscover.insert(QStringLiteral("params"), distinctParams);
    const auto distinctDirectResult =
        send(directManagerB, baseRequest(endpoint, QString::fromLatin1(Mcp::DiscoverMethod)),
             QJsonDocument(distinctDirectDiscover).toJson(QJsonDocument::Compact));
    QString distinctDirectClient;
    {
        const QMutexLocker locker(&observationMutex);
        distinctDirectClient = observedClientId;
    }
    expect(distinctDirectResult.status == 200 && !distinctDirectClient.isEmpty() &&
               distinctDirectClient != directClientA,
           QStringLiteral("different modern client implementations must have distinct identities"));
}

void TestMcpHttpServer::legacySessionLifecycle() {
    McpServerFixture fixture;
    QVERIFY2(fixture.start(), qPrintable(fixture.error));
    auto &server = fixture.server;
    auto &manager = fixture.manager;
    const auto endpoint = fixture.endpoint();
    auto &observationMutex = fixture.observationMutex;
    auto &observedClientId = fixture.observedClientId;
    auto &observedMethod = fixture.observedMethod;
    const Mcp::RequestContext legacyContext{
        .protocolVersion = QString::fromLatin1(Mcp::LegacyProtocolVersion),
        .clientCapabilities = QJsonObject{                                           },
        .clientInfo =
            Mcp::ImplementationInfo{
                                          .name = QStringLiteral("legacy-http-client"),
                                          .version = QStringLiteral("1.0"),
                                          },
    };
    auto modernInitializeContext = legacyContext;
    modernInitializeContext.protocolVersion = QString::fromLatin1(Mcp::ProtocolVersion);
    const auto rejectedModernInitialize =
        send(manager, baseRequest(endpoint, QString::fromLatin1(Mcp::InitializeMethod)),
             QJsonDocument(
                 Mcp::makeInitializeRequest(modernInitializeContext, QStringLiteral("init-2026")))
                 .toJson(QJsonDocument::Compact));
    expect(rejectedModernInitialize.status == 400 &&
               jsonRpcErrorCode(rejectedModernInitialize) == Mcp::InvalidRequest,
           QStringLiteral("MCP 2026-07-28 initialize must be rejected before dispatch"));

    const auto initialize = Mcp::makeInitializeRequest(legacyContext, QStringLiteral("init-2025"));
    const auto initializeResult = send(manager, legacyRequest(endpoint, false),
                                       QJsonDocument(initialize).toJson(QJsonDocument::Compact));
    const auto initializePayload =
        bodyObject(initializeResult).value(QStringLiteral("result")).toObject();
    expect(initializeResult.status == 200 &&
               initializePayload.value(QStringLiteral("protocolVersion")) ==
                   QString::fromLatin1(Mcp::LegacyProtocolVersion) &&
               initializePayload.value(QStringLiteral("capabilities"))
                   .toObject()
                   .value(QStringLiteral("tools"))
                   .isObject() &&
               !initializePayload.contains(QStringLiteral("resultType")) &&
               !initializeResult.sessionId.isEmpty(),
           QStringLiteral("MCP 2025-11-25 initialize must work without modern transport headers"));

    const auto prematureLegacyCall =
        Mcp::makeRequest(QString::fromLatin1(Mcp::ToolsCallMethod),
                         QJsonObject{
                             {QStringLiteral("name"),      QStringLiteral("legacy.tool")},
                             {QStringLiteral("arguments"), QJsonObject{}                }
    },
                         legacyContext, QStringLiteral("premature-call-2025"));
    {
        const QMutexLocker locker(&observationMutex);
        observedMethod = QStringLiteral("before-initialized");
    }
    const auto prematureLegacyCallResult =
        send(manager,
             legacyRequest(endpoint, true, Mcp::LegacyProtocolVersion, initializeResult.sessionId),
             QJsonDocument(prematureLegacyCall).toJson(QJsonDocument::Compact));
    {
        const QMutexLocker locker(&observationMutex);
        expect(prematureLegacyCallResult.status == 400 &&
                   jsonRpcErrorCode(prematureLegacyCallResult) == Mcp::ServerNotInitialized &&
                   observedMethod == QStringLiteral("before-initialized"),
               QStringLiteral("legacy tools must not dispatch before notifications/initialized"));
    }

    const auto initialized =
        Mcp::makeRequest(QString::fromLatin1(Mcp::InitializedNotification), {}, legacyContext);
    const auto initializedResult =
        send(manager,
             legacyRequest(endpoint, true, Mcp::LegacyProtocolVersion, initializeResult.sessionId),
             QJsonDocument(initialized).toJson(QJsonDocument::Compact));
    expect(initializedResult.status == 202 && initializedResult.body.isEmpty(),
           QStringLiteral("MCP 2025-11-25 initialized notification must return empty HTTP 202"));

    const auto legacyListWithoutVersion =
        Mcp::makeRequest(QString::fromLatin1(Mcp::ToolsListMethod), {}, legacyContext,
                         QStringLiteral("list-2025-without-version"));
    const auto legacyListWithoutVersionResult =
        send(manager,
             legacyRequest(endpoint, false, Mcp::LegacyProtocolVersion, initializeResult.sessionId),
             QJsonDocument(legacyListWithoutVersion).toJson(QJsonDocument::Compact));
    expect(legacyListWithoutVersionResult.status == 400 &&
               jsonRpcErrorCode(legacyListWithoutVersionResult) == Mcp::HeaderMismatch,
           QStringLiteral("MCP 2025-11-25 requires a protocol header after initialize"));

    const auto legacyPing = Mcp::makeRequest(QString::fromLatin1(Mcp::PingMethod), {},
                                             legacyContext, QStringLiteral("ping-2025"));
    const auto legacyPingResult =
        send(manager,
             legacyRequest(endpoint, true, Mcp::LegacyProtocolVersion, initializeResult.sessionId),
             QJsonDocument(legacyPing).toJson(QJsonDocument::Compact));
    expect(legacyPingResult.status == 200 &&
               bodyObject(legacyPingResult).value(QStringLiteral("result")).toObject().isEmpty(),
           QStringLiteral("MCP 2025-11-25 ping must use the legacy result envelope"));

    const auto legacyList = Mcp::makeRequest(QString::fromLatin1(Mcp::ToolsListMethod), {},
                                             legacyContext, QStringLiteral("list-2025"));
    {
        const QMutexLocker locker(&observationMutex);
        observedMethod = QStringLiteral("before-missing-session");
    }
    const auto legacyListWithoutSessionResult =
        send(manager, legacyRequest(endpoint, true, Mcp::LegacyProtocolVersion),
             QJsonDocument(legacyList).toJson(QJsonDocument::Compact));
    {
        const QMutexLocker locker(&observationMutex);
        expect(legacyListWithoutSessionResult.status == 400 &&
                   observedMethod == QStringLiteral("before-missing-session"),
               QStringLiteral("legacy requests without a live session must not dispatch"));
    }
    const auto legacyListResult =
        send(manager,
             legacyRequest(endpoint, true, Mcp::LegacyProtocolVersion, initializeResult.sessionId),
             QJsonDocument(legacyList).toJson(QJsonDocument::Compact));
    const auto legacyListPayload =
        bodyObject(legacyListResult).value(QStringLiteral("result")).toObject();
    expect(legacyListResult.status == 200 &&
               legacyListPayload.value(QStringLiteral("tools")).isArray() &&
               !legacyListPayload.contains(QStringLiteral("resultType")) &&
               !legacyListPayload.contains(QStringLiteral("ttlMs")),
           QStringLiteral("MCP 2025-11-25 tools/list must not require 2026 routing headers"));

    const auto legacyCall =
        Mcp::makeRequest(QString::fromLatin1(Mcp::ToolsCallMethod),
                         QJsonObject{
                             {QStringLiteral("name"),      QStringLiteral("legacy.tool")},
                             {QStringLiteral("arguments"), QJsonObject{}                }
    },
                         legacyContext, QStringLiteral("call-2025"));
    const auto legacyCallResult =
        send(manager,
             legacyRequest(endpoint, true, Mcp::LegacyProtocolVersion, initializeResult.sessionId),
             QJsonDocument(legacyCall).toJson(QJsonDocument::Compact));
    const auto legacyCallPayload =
        bodyObject(legacyCallResult).value(QStringLiteral("result")).toObject();
    expect(legacyCallResult.status == 200 &&
               legacyCallPayload.value(QStringLiteral("content")).isArray() &&
               legacyCallPayload.value(QStringLiteral("structuredContent")).isObject() &&
               !legacyCallPayload.contains(QStringLiteral("resultType")),
           QStringLiteral("MCP 2025-11-25 tools/call must retain text and structured results"));

    auto compatibilityContext = legacyContext;
    compatibilityContext.protocolVersion = QString::fromLatin1(Mcp::CompatibilityProtocolVersion);
    const auto compatibilityInitialize =
        Mcp::makeInitializeRequest(compatibilityContext, QStringLiteral("init-2025-06"));
    const auto compatibilityInitializeResult =
        send(manager, legacyRequest(endpoint, false, Mcp::CompatibilityProtocolVersion),
             QJsonDocument(compatibilityInitialize).toJson(QJsonDocument::Compact));
    const auto compatibilityInitializePayload =
        bodyObject(compatibilityInitializeResult).value(QStringLiteral("result")).toObject();
    expect(compatibilityInitializeResult.status == 200 &&
               compatibilityInitializePayload.value(QStringLiteral("protocolVersion")) ==
                   QString::fromLatin1(Mcp::CompatibilityProtocolVersion) &&
               !compatibilityInitializePayload.contains(QStringLiteral("resultType")) &&
               !compatibilityInitializeResult.sessionId.isEmpty(),
           QStringLiteral("MCP 2025-06-18 initialize must echo the requested supported version"));
    const auto compatibilityInitialized = Mcp::makeRequest(
        QString::fromLatin1(Mcp::InitializedNotification), {}, compatibilityContext);
    const auto compatibilityInitializedResult =
        send(manager,
             legacyRequest(endpoint, true, Mcp::CompatibilityProtocolVersion,
                           compatibilityInitializeResult.sessionId),
             QJsonDocument(compatibilityInitialized).toJson(QJsonDocument::Compact));
    expect(compatibilityInitializedResult.status == 202 &&
               compatibilityInitializedResult.body.isEmpty(),
           QStringLiteral("MCP 2025-06-18 initialized notification must complete the session"));
    const auto compatibilityList =
        Mcp::makeRequest(QString::fromLatin1(Mcp::ToolsListMethod), {}, compatibilityContext,
                         QStringLiteral("list-2025-06"));
    const auto compatibilityListResult =
        send(manager,
             legacyRequest(endpoint, true, Mcp::CompatibilityProtocolVersion,
                           compatibilityInitializeResult.sessionId),
             QJsonDocument(compatibilityList).toJson(QJsonDocument::Compact));
    const auto compatibilityListPayload =
        bodyObject(compatibilityListResult).value(QStringLiteral("result")).toObject();
    expect(compatibilityListResult.status == 200 &&
               compatibilityListPayload.value(QStringLiteral("tools")).isArray() &&
               !compatibilityListPayload.contains(QStringLiteral("resultType")),
           QStringLiteral("MCP 2025-06-18 tools/list must use the legacy result envelope"));

    const auto thirdInitialize =
        Mcp::makeInitializeRequest(legacyContext, QStringLiteral("init-2025-third"));
    const auto thirdInitializeResult =
        send(manager, legacyRequest(endpoint, false),
             QJsonDocument(thirdInitialize).toJson(QJsonDocument::Compact));
    expect(thirdInitializeResult.status == 200 && !thirdInitializeResult.sessionId.isEmpty(),
           QStringLiteral("a replacement legacy session must initialize successfully"));
    const auto evictedSessionResult =
        send(manager,
             legacyRequest(endpoint, true, Mcp::LegacyProtocolVersion, initializeResult.sessionId),
             QJsonDocument(legacyPing).toJson(QJsonDocument::Compact));
    expect(evictedSessionResult.status == 404,
           QStringLiteral("legacy session retention must evict the oldest session at its limit"));

    auto deleteSessionRequest =
        legacyRequest(endpoint, true, Mcp::LegacyProtocolVersion, thirdInitializeResult.sessionId);
    const auto retiredSession = send(manager, deleteSessionRequest, {}, HttpMethod::Delete);
    const auto repeatedRetirement = send(manager, deleteSessionRequest, {}, HttpMethod::Delete);
    expect(retiredSession.status == 204 && repeatedRetirement.status == 404,
           QStringLiteral("DELETE /mcp must retire one known legacy session exactly once"));
    const auto retiredSessionRequest = send(
        manager,
        legacyRequest(endpoint, true, Mcp::LegacyProtocolVersion, thirdInitializeResult.sessionId),
        QJsonDocument(legacyPing).toJson(QJsonDocument::Compact));
    expect(retiredSessionRequest.status == 404,
           QStringLiteral("retired legacy sessions must not dispatch later requests"));

    {
        const QMutexLocker locker(&observationMutex);
        observedMethod = QStringLiteral("unchanged");
    }
    const auto cancelledNotification =
        requestObject(QStringLiteral("notifications/cancelled"), QJsonValue(QJsonValue::Undefined),
                      QJsonObject{
                          {QStringLiteral("requestId"), 1}
    });
    const auto notificationResult =
        send(manager, baseRequest(endpoint, QStringLiteral("notifications/cancelled")),
             QJsonDocument(cancelledNotification).toJson(QJsonDocument::Compact));
    expect(notificationResult.status == 202 && notificationResult.body.isEmpty(),
           QStringLiteral("valid notifications must return 202 with an empty body"));
    {
        const QMutexLocker locker(&observationMutex);
        expect(observedMethod == QStringLiteral("unchanged"),
               QStringLiteral("notifications must not enter the request handler"));
    }
}

void TestMcpHttpServer::httpAdmission_data() {
    QTest::addColumn<QByteArray>("header");
    QTest::addColumn<QByteArray>("value");
    QTest::addColumn<int>("method");
    QTest::addColumn<int>("status");
    const auto add = [](const char *name, const QByteArray &header, const QByteArray &value,
                        HttpMethod method, int status) {
        QTest::newRow(name) << header << value << int(method) << status;
    };
    add("get-without-stream", {}, {}, HttpMethod::Get, 405);
    add("delete-without-session", {}, {}, HttpMethod::Delete, 400);
    add("foreign-origin", "Origin", "https://example.com", HttpMethod::Post, 403);
    add("origin-before-method", "Origin", "https://example.com", HttpMethod::Get, 403);
    add("local-origin", "Origin", "http://localhost:$PORT", HttpMethod::Post, 200);
    add("wrong-authority", "Host", "localhost:$PORT", HttpMethod::Post, 403);
    add("missing-event-stream", "Accept", "application/json", HttpMethod::Post, 406);
    add("wildcard", "Accept", "*/*", HttpMethod::Post, 406);
    add("zero-quality", "Accept", "application/json;q=0, text/event-stream; q=0.0",
        HttpMethod::Post, 406);
    add("non-json-content", "Content-Type", "text/plain", HttpMethod::Post, 415);
}

void TestMcpHttpServer::httpAdmission() {
    QFETCH(QByteArray, header);
    QFETCH(QByteArray, value);
    QFETCH(int, method);
    QFETCH(int, status);
    McpServerFixture fixture;
    QVERIFY2(fixture.start(), qPrintable(fixture.error));
    auto &server = fixture.server;
    auto &manager = fixture.manager;
    const auto endpoint = fixture.endpoint();
    const auto discover =
        requestObject(QString::fromLatin1(Mcp::DiscoverMethod), QStringLiteral("discover"));
    auto request = baseRequest(endpoint, QString::fromLatin1(Mcp::DiscoverMethod));
    value.replace("$PORT", QByteArray::number(server.port()));
    if (!header.isEmpty())
        request.setRawHeader(header, value);
    const auto result =
        send(manager, request, QJsonDocument(discover).toJson(QJsonDocument::Compact),
             static_cast<HttpMethod>(method));
    QCOMPARE(result.status, status);
    if (status != 200) {
        const QMutexLocker locker(&fixture.observationMutex);
        QVERIFY(fixture.observedMethod.isEmpty());
    }
}

void TestMcpHttpServer::emptyOriginRejected() {
    McpServerFixture fixture;
    QVERIFY2(fixture.start(), qPrintable(fixture.error));
    auto &server = fixture.server;
    auto &manager = fixture.manager;
    const auto endpoint = fixture.endpoint();
    const auto status = rawHttpStatus(
        server.port(), QByteArrayLiteral("GET /mcp HTTP/1.1\r\nHost: 127.0.0.1:") +
                           QByteArray::number(server.port()) +
                           QByteArrayLiteral("\r\nOrigin:\r\nConnection: close\r\n\r\n"));
    QCOMPARE(status, 403);
}

void TestMcpHttpServer::transportMetadataRouting() {
    McpServerFixture fixture;
    QVERIFY2(fixture.start(), qPrintable(fixture.error));
    auto &server = fixture.server;
    auto &manager = fixture.manager;
    const auto endpoint = fixture.endpoint();
    const auto discover =
        requestObject(QString::fromLatin1(Mcp::DiscoverMethod), QStringLiteral("discover"));
    auto missingHeaderRequest = baseRequest(endpoint, QString::fromLatin1(Mcp::DiscoverMethod));
    missingHeaderRequest.setRawHeader("MCP-Protocol-Version", QByteArray{});
    const auto missingProtocolHeader =
        send(manager, missingHeaderRequest, QJsonDocument(discover).toJson(QJsonDocument::Compact));
    expect(missingProtocolHeader.status == 400 &&
               jsonRpcErrorCode(missingProtocolHeader) == Mcp::HeaderMismatch,
           QStringLiteral("missing protocol transport metadata must return -32020"));

    auto missingMetaBody = discover;
    auto missingMetaParams = missingMetaBody.value(QStringLiteral("params")).toObject();
    missingMetaParams.remove(QStringLiteral("_meta"));
    missingMetaBody.insert(QStringLiteral("params"), missingMetaParams);
    auto missingMetadataHeaders = QNetworkRequest(endpoint);
    missingMetadataHeaders.setRawHeader("Content-Type", "application/json");
    missingMetadataHeaders.setRawHeader("Accept", "application/json, text/event-stream");
    const auto missingMetadataPreflight =
        send(manager, missingMetadataHeaders,
             QJsonDocument(missingMetaBody).toJson(QJsonDocument::Compact));
    expect(
        missingMetadataPreflight.status == 400 &&
            jsonRpcErrorCode(missingMetadataPreflight) == Mcp::HeaderMismatch,
        QStringLiteral("request IDs must trigger header preflight before body metadata parsing"));

    auto malformedToolCall = missingMetaBody;
    malformedToolCall.remove(QStringLiteral("method"));
    const auto missingToolName =
        send(manager, baseRequest(endpoint, QString::fromLatin1(Mcp::ToolsCallMethod)),
             QJsonDocument(malformedToolCall).toJson(QJsonDocument::Compact));
    expect(
        missingToolName.status == 400 && jsonRpcErrorCode(missingToolName) == Mcp::HeaderMismatch,
        QStringLiteral("transport tools/call routing must require Mcp-Name before body parsing"));

    auto mismatchRequest = baseRequest(endpoint, QString::fromLatin1(Mcp::ToolsListMethod));
    const auto headerMismatch =
        send(manager, mismatchRequest, QJsonDocument(discover).toJson(QJsonDocument::Compact));
    expect(headerMismatch.status == 400 && jsonRpcErrorCode(headerMismatch) == Mcp::HeaderMismatch,
           QStringLiteral("header/body method mismatch must return -32020"));

    auto unsupported = discover;
    auto unsupportedParams = unsupported.value(QStringLiteral("params")).toObject();
    auto unsupportedMeta = unsupportedParams.value(QStringLiteral("_meta")).toObject();
    unsupportedMeta.insert(QString::fromLatin1(Mcp::ProtocolVersionMetaKey),
                           QStringLiteral("2099-01-01"));
    unsupportedParams.insert(QStringLiteral("_meta"), unsupportedMeta);
    unsupported.insert(QStringLiteral("params"), unsupportedParams);
    const auto unsupportedBodyMismatch =
        send(manager, baseRequest(endpoint, QString::fromLatin1(Mcp::DiscoverMethod)),
             QJsonDocument(unsupported).toJson(QJsonDocument::Compact));
    expect(unsupportedBodyMismatch.status == 400 &&
               jsonRpcErrorCode(unsupportedBodyMismatch) == Mcp::HeaderMismatch,
           QStringLiteral(
               "a supported header and different unsupported body version must return -32020"));
    auto unsupportedRequest = baseRequest(endpoint, QString::fromLatin1(Mcp::DiscoverMethod));
    unsupportedRequest.setRawHeader("MCP-Protocol-Version", "2099-01-01");
    const auto unsupportedVersion = send(manager, unsupportedRequest,
                                         QJsonDocument(unsupported).toJson(QJsonDocument::Compact));
    const auto unsupportedData = bodyObject(unsupportedVersion)
                                     .value(QStringLiteral("error"))
                                     .toObject()
                                     .value(QStringLiteral("data"))
                                     .toObject();
    const auto supportedVersions = unsupportedData.value(QStringLiteral("supported")).toArray();
    expect(unsupportedVersion.status == 400 &&
               jsonRpcErrorCode(unsupportedVersion) == Mcp::UnsupportedProtocolVersion &&
               unsupportedData.value(QStringLiteral("requested")) == QStringLiteral("2099-01-01") &&
               supportedVersions ==
                   QJsonArray{QString::fromLatin1(Mcp::ProtocolVersion),
                              QString::fromLatin1(Mcp::LegacyProtocolVersion),
                              QString::fromLatin1(Mcp::CompatibilityProtocolVersion)} &&
               !unsupportedData.contains(QStringLiteral("supportedVersions")),
           QStringLiteral("-32022 data must contain exact supported and requested fields"));

    auto unicodeCall =
        requestObject(QString::fromLatin1(Mcp::ToolsCallMethod), QStringLiteral("unicode"),
                      QJsonObject{
                          {QStringLiteral("name"),      QStringLiteral("测试.tool")},
                          {QStringLiteral("arguments"), QJsonObject{}              }
    });
    const auto unicodeResult = send(manager,
                                    baseRequest(endpoint, QString::fromLatin1(Mcp::ToolsCallMethod),
                                                QStringLiteral("测试.tool")),
                                    QJsonDocument(unicodeCall).toJson(QJsonDocument::Compact));
    expect(unicodeResult.status == 200,
           QStringLiteral("non-ASCII tool names must round-trip through encoded headers"));

    auto nameMismatchRequest = baseRequest(endpoint, QString::fromLatin1(Mcp::ToolsCallMethod),
                                           QStringLiteral("different.tool"));
    const auto nameMismatch = send(manager, nameMismatchRequest,
                                   QJsonDocument(unicodeCall).toJson(QJsonDocument::Compact));
    expect(nameMismatch.status == 400 && jsonRpcErrorCode(nameMismatch) == Mcp::HeaderMismatch,
           QStringLiteral("Mcp-Name mismatches must return -32020"));
}

void TestMcpHttpServer::jsonAndRequestLimits() {
    McpServerFixture fixture;
    QVERIFY2(fixture.start(), qPrintable(fixture.error));
    auto &server = fixture.server;
    auto &manager = fixture.manager;
    const auto endpoint = fixture.endpoint();
    const auto discover =
        requestObject(QString::fromLatin1(Mcp::DiscoverMethod), QStringLiteral("discover"));
    const auto batch =
        send(manager, baseRequest(endpoint, QString::fromLatin1(Mcp::DiscoverMethod)),
             QByteArrayLiteral("[]"));
    expect(batch.status == 400 && jsonRpcErrorCode(batch) == Mcp::InvalidRequest,
           QStringLiteral("JSON-RPC batch arrays must be rejected"));

    const auto malformed =
        send(manager, baseRequest(endpoint, QString::fromLatin1(Mcp::DiscoverMethod)),
             QByteArrayLiteral("{not-json"));
    expect(malformed.status == 400 && jsonRpcErrorCode(malformed) == Mcp::ParseError,
           QStringLiteral("malformed JSON must return the JSON-RPC parse error"));

    QJsonArray nested;
    for (int depth = 0; depth < 70; ++depth) {
        QJsonArray wrapper;
        wrapper.append(nested);
        nested = std::move(wrapper);
    }
    auto tooDeep = discover;
    auto deepParams = tooDeep.value(QStringLiteral("params")).toObject();
    deepParams.insert(QStringLiteral("nested"), nested);
    tooDeep.insert(QStringLiteral("params"), deepParams);
    const auto tooDeepBody = QJsonDocument(tooDeep).toJson(QJsonDocument::Compact);
    auto missingDepthHeaders = QNetworkRequest(endpoint);
    missingDepthHeaders.setRawHeader("Content-Type", "application/json");
    missingDepthHeaders.setRawHeader("Accept", "application/json, text/event-stream");
    const auto depthHeaderPreflight = send(manager, missingDepthHeaders, tooDeepBody);
    expect(depthHeaderPreflight.status == 400 &&
               jsonRpcErrorCode(depthHeaderPreflight) == Mcp::HeaderMismatch,
           QStringLiteral("header preflight must precede JSON depth validation for requests"));
    const auto depthLimited =
        send(manager, baseRequest(endpoint, QString::fromLatin1(Mcp::DiscoverMethod)), tooDeepBody);
    expect(
        depthLimited.status == 400 && jsonRpcErrorCode(depthLimited) == Mcp::InvalidRequest,
        QStringLiteral("excessively deep JSON must be rejected before dispatch (HTTP %1, code %2)")
            .arg(depthLimited.status)
            .arg(jsonRpcErrorCode(depthLimited)));

    QJsonArray excessiveNodes;
    for (int index = 0; index < 150; ++index)
        excessiveNodes.append(index);
    auto tooManyNodes = discover;
    auto nodeParams = tooManyNodes.value(QStringLiteral("params")).toObject();
    nodeParams.insert(QStringLiteral("nodes"), excessiveNodes);
    tooManyNodes.insert(QStringLiteral("params"), nodeParams);
    const auto nodeLimited =
        send(manager, baseRequest(endpoint, QString::fromLatin1(Mcp::DiscoverMethod)),
             QJsonDocument(tooManyNodes).toJson(QJsonDocument::Compact));
    expect(nodeLimited.status == 400 && jsonRpcErrorCode(nodeLimited) == Mcp::InvalidRequest,
           QStringLiteral("excessive JSON node counts must be rejected before dispatch"));

    auto unknown = requestObject(QStringLiteral("unknown/method"), QStringLiteral("unknown"));
    const auto unknownMethod =
        send(manager, baseRequest(endpoint, QStringLiteral("unknown/method")),
             QJsonDocument(unknown).toJson(QJsonDocument::Compact));
    expect(unknownMethod.status == 404 && jsonRpcErrorCode(unknownMethod) == Mcp::MethodNotFound,
           QStringLiteral("unknown MCP method must return HTTP 404 and -32601"));

    QByteArray oversized(1100, 'x');
    const auto tooLarge =
        send(manager, baseRequest(endpoint, QString::fromLatin1(Mcp::DiscoverMethod)), oversized);
    expect(tooLarge.status == 413,
           QStringLiteral("request bodies above the configured limit must be rejected"));
    QByteArray transportOversized(1024 * 1024 + 1, 'x');
    const auto transportTooLarge =
        send(manager, baseRequest(endpoint, QString::fromLatin1(Mcp::DiscoverMethod)),
             transportOversized, HttpMethod::Post, 3000);
    expect(!transportTooLarge.timedOut &&
               (transportTooLarge.status == 413 ||
                (transportTooLarge.status == 0 &&
                 transportTooLarge.networkError != QNetworkReply::NoError)),
           QStringLiteral("the HTTP parser must enforce the one-MiB transport body limit "
                          "(HTTP %1, network error %2, timeout %3)")
               .arg(transportTooLarge.status)
               .arg(static_cast<int>(transportTooLarge.networkError))
               .arg(transportTooLarge.timedOut));
}

void TestMcpHttpServer::handlerResponseLimits() {
    McpServerFixture fixture;
    QVERIFY2(fixture.start(), qPrintable(fixture.error));
    auto &server = fixture.server;
    auto &manager = fixture.manager;
    const auto endpoint = fixture.endpoint();
    const auto invalidResponseCall =
        requestObject(QString::fromLatin1(Mcp::ToolsCallMethod), QStringLiteral("invalid-response"),
                      QJsonObject{
                          {QStringLiteral("name"),      QStringLiteral("invalid.response")},
                          {QStringLiteral("arguments"), QJsonObject{}                     }
    });
    const auto invalidResponse =
        send(manager,
             baseRequest(endpoint, QString::fromLatin1(Mcp::ToolsCallMethod),
                         QStringLiteral("invalid.response")),
             QJsonDocument(invalidResponseCall).toJson(QJsonDocument::Compact));
    expect(invalidResponse.status == 200 && jsonRpcErrorCode(invalidResponse) == Mcp::InternalError,
           QStringLiteral("invalid handler envelopes must be replaced by an internal error"));

    const auto largeResponseCall =
        requestObject(QString::fromLatin1(Mcp::ToolsCallMethod), QStringLiteral("large-response"),
                      QJsonObject{
                          {QStringLiteral("name"),      QStringLiteral("large.response")},
                          {QStringLiteral("arguments"), QJsonObject{}                   }
    });
    const auto largeResponse =
        send(manager,
             baseRequest(endpoint, QString::fromLatin1(Mcp::ToolsCallMethod),
                         QStringLiteral("large.response")),
             QJsonDocument(largeResponseCall).toJson(QJsonDocument::Compact));
    expect(largeResponse.status == 500 && jsonRpcErrorCode(largeResponse) == Mcp::InternalError,
           QStringLiteral("responses above the configured limit must fail closed"));
}

void TestMcpHttpServer::listenerLifecycle() {
    McpServerFixture fixture;
    QVERIFY2(fixture.start(), qPrintable(fixture.error));
    auto &server = fixture.server;
    auto &manager = fixture.manager;
    const auto endpoint = fixture.endpoint();
    Automation::McpHttpServer conflictingServer(
        [](const Mcp::RequestEnvelope &, const QString &) { return QJsonObject{}; });
    QString conflictError;
    expect(!conflictingServer.start(server.port(), conflictError) &&
               !conflictingServer.isListening() && !conflictError.isEmpty(),
           QStringLiteral("an occupied explicit port must fail without stealing the listener"));

    server.stop();
    expect(!server.isListening() && server.endpoint().isEmpty(),
           QStringLiteral("stopping the MCP server must release its endpoint"));

    expect(server.start(0, fixture.error),
           QStringLiteral("the MCP server must support a clean restart: %1").arg(fixture.error));
    server.requestStop();
    expect(waitForStop(server),
           QStringLiteral("asynchronous MCP shutdown must complete without blocking the GUI loop"));
    expect(!server.isListening() && !server.isStopping() && server.endpoint().isEmpty(),
           QStringLiteral("asynchronous MCP shutdown must release its worker and endpoint"));
}

void TestMcpHttpServer::responseSurvivesShutdown_data() {
    QTest::addColumn<bool>("consumeResponse");
    QTest::newRow("reading-peer") << true;
    QTest::newRow("stalled-peer") << false;
}

void TestMcpHttpServer::responseSurvivesShutdown() {
    QFETCH(bool, consumeResponse);
    const QString payload(6 * 1024 * 1024, u'x');
    Automation::McpHttpServer server(this, {}, [&](const QJsonValue &message, const QString &) {
        QTimer::singleShot(0, this, [&] { server.requestStop(); });
        return QJsonObject{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")                            },
            {QStringLiteral("id"),      message.toObject().value(QStringLiteral("id"))   },
            {QStringLiteral("result"),  QJsonObject{{QStringLiteral("payload"), payload}}},
        };
    });
    QString error;
    QVERIFY2(server.start(0, {.native = true}, error), qPrintable(error));
    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy::NoProxy);
    const QJsonObject request{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")      },
        {QStringLiteral("id"),      1                          },
        {QStringLiteral("method"),  QStringLiteral("test.exit")},
    };
    auto *reply = startRequest(manager, nativeRequest(QUrl(server.nativeEndpoint())),
                               QJsonDocument(request).toJson(QJsonDocument::Compact));
    reply->setReadBufferSize(4096);
    if (!consumeResponse) {
        const auto stopped = waitForStop(server, 5000);
        const auto buffered = reply->bytesAvailable();
        reply->abort();
        reply->deleteLater();
        QVERIFY2(stopped, "An unread response must not prevent bounded server shutdown");
        QVERIFY(buffered > 0);
        QVERIFY(!server.isListening() && !server.isStopping() && server.nativeEndpoint().isEmpty());
        return;
    }
    QByteArray received;
    const auto readConnection =
        connect(reply, &QNetworkReply::readyRead, this, [&] { received.append(reply->readAll()); });
    const auto result = finishRequest(reply, 10000);
    disconnect(readConnection);
    received.append(result.body);
    QVERIFY(!result.timedOut);
    QCOMPARE(result.networkError, QNetworkReply::NoError);
    QCOMPARE(result.status, 200);
    QJsonParseError parseError;
    const auto response = QJsonDocument::fromJson(received, &parseError).object();
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QCOMPARE(response.value(QStringLiteral("id")).toInt(), 1);
    QVERIFY2(response.value(QStringLiteral("result"))
                     .toObject()
                     .value(QStringLiteral("payload"))
                     .toString() == payload,
             "Shutdown must deliver the complete response payload");
    QVERIFY(waitForStop(server));
}

void TestMcpHttpServer::nativeRequestValidation() {
    NativeServerFixture fixture;
    QVERIFY2(fixture.start(), qPrintable(fixture.error));
    auto &dualProtocolServer = fixture.server;
    const auto &dualProtocolLimits = fixture.limits;
    auto &manager = fixture.manager;
    const auto nativeCall = QJsonObject{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")                       },
        {QStringLiteral("id"),      7                                           },
        {QStringLiteral("method"),  QStringLiteral("application.get_status")    },
        {QStringLiteral("params"),  QJsonObject{{QStringLiteral("probe"), true}}},
    };
    const auto nativeResult =
        send(manager, nativeRequest(QUrl(dualProtocolServer.nativeEndpoint())),
             QJsonDocument(nativeCall).toJson(QJsonDocument::Compact));
    const auto nativeResultBody = bodyObject(nativeResult);
    expect(nativeResult.status == 200 &&
               nativeResultBody.value(QStringLiteral("result"))
                   .toObject()
                   .value(QStringLiteral("params"))
                   .toObject()
                   .value(QStringLiteral("probe"))
                   .toBool() &&
               nativeResultBody.value(QStringLiteral("result"))
                   .toObject()
                   .value(QStringLiteral("client_id_present"))
                   .toBool(),
           QStringLiteral(
               "Native requests must return direct JSON-RPC results on the shared listener"));
    const auto malformedNative =
        send(manager, nativeRequest(QUrl(dualProtocolServer.nativeEndpoint())), "{");
    expect(malformedNative.status == 200 && jsonRpcErrorCode(malformedNative) == -32700,
           QStringLiteral("malformed Native JSON must return a JSON-RPC parse error"));
    const auto nativeBatch =
        send(manager, nativeRequest(QUrl(dualProtocolServer.nativeEndpoint())), "[]");
    expect(nativeBatch.status == 200 && jsonRpcErrorCode(nativeBatch) == -32600,
           QStringLiteral("Native JSON-RPC batches must be rejected as Invalid Request"));
    const auto nativeGet = send(manager, nativeRequest(QUrl(dualProtocolServer.nativeEndpoint())),
                                {}, HttpMethod::Get);
    expect(nativeGet.status == 405,
           QStringLiteral("the Native endpoint must reject non-POST methods"));

    auto rejectedAcceptRequest = nativeRequest(QUrl(dualProtocolServer.nativeEndpoint()));
    rejectedAcceptRequest.setRawHeader("Accept", "*/*;q=1, application/json;q=0");
    const auto rejectedNativeAccept =
        send(manager, std::move(rejectedAcceptRequest),
             QJsonDocument(nativeCall).toJson(QJsonDocument::Compact));
    expect(rejectedNativeAccept.status == 406,
           QStringLiteral("an exact application/json q=0 must override wildcard acceptance"));

    auto rejectedNativeContentType = nativeRequest(QUrl(dualProtocolServer.nativeEndpoint()));
    rejectedNativeContentType.setRawHeader("Content-Type", "text/plain");
    const auto wrongNativeContentType =
        send(manager, std::move(rejectedNativeContentType),
             QJsonDocument(nativeCall).toJson(QJsonDocument::Compact));
    expect(wrongNativeContentType.status == 415,
           QStringLiteral("Native requests must require JSON content"));

    const auto oversizedNativeRequest =
        send(manager, nativeRequest(QUrl(dualProtocolServer.nativeEndpoint())),
             QByteArray(dualProtocolLimits.maximumRequestBytes + 1, 'x'));
    expect(oversizedNativeRequest.status == 413,
           QStringLiteral("Native request bodies above the configured limit must be rejected"));

    QJsonArray nativeNested;
    for (int depth = 0; depth < 20; ++depth) {
        QJsonArray wrapper;
        wrapper.append(nativeNested);
        nativeNested = std::move(wrapper);
    }
    auto deepNativeCall = nativeCall;
    deepNativeCall.insert(QStringLiteral("params"), QJsonObject{
                                                        {QStringLiteral("nested"), nativeNested}
    });
    const auto deepNativeResult =
        send(manager, nativeRequest(QUrl(dualProtocolServer.nativeEndpoint())),
             QJsonDocument(deepNativeCall).toJson(QJsonDocument::Compact));
    expect(deepNativeResult.status == 200 && jsonRpcErrorCode(deepNativeResult) == -32600,
           QStringLiteral("Native JSON depth must be bounded before dispatch"));

    QJsonArray nativeNodes;
    for (int index = 0; index < 140; ++index)
        nativeNodes.append(index);
    auto nodeHeavyNativeCall = nativeCall;
    nodeHeavyNativeCall.insert(QStringLiteral("params"),
                               QJsonObject{
                                   {QStringLiteral("nodes"), nativeNodes}
    });
    const auto nodeHeavyNativeResult =
        send(manager, nativeRequest(QUrl(dualProtocolServer.nativeEndpoint())),
             QJsonDocument(nodeHeavyNativeCall).toJson(QJsonDocument::Compact));
    expect(nodeHeavyNativeResult.status == 200 && jsonRpcErrorCode(nodeHeavyNativeResult) == -32600,
           QStringLiteral("Native JSON node counts must be bounded before dispatch"));

    auto exceptionalNativeCall = nativeCall;
    exceptionalNativeCall.insert(QStringLiteral("id"), QStringLiteral("native-throw-id"));
    exceptionalNativeCall.insert(QStringLiteral("method"), QStringLiteral("test.throw"));
    const auto exceptionalNative =
        send(manager, nativeRequest(QUrl(dualProtocolServer.nativeEndpoint())),
             QJsonDocument(exceptionalNativeCall).toJson(QJsonDocument::Compact));
    const auto exceptionalNativeBody = bodyObject(exceptionalNative);
    expect(exceptionalNative.status == 200 && jsonRpcErrorCode(exceptionalNative) == -32603 &&
               exceptionalNativeBody.value(QStringLiteral("id")).toString() ==
                   QStringLiteral("native-throw-id"),
           QStringLiteral("Native handler exceptions must preserve a valid request id"));

    auto invalidNativeResponseCall = nativeCall;
    invalidNativeResponseCall.insert(QStringLiteral("id"), QStringLiteral("native-invalid-id"));
    invalidNativeResponseCall.insert(QStringLiteral("method"),
                                     QStringLiteral("test.invalid_response"));
    const auto invalidNativeResponse =
        send(manager, nativeRequest(QUrl(dualProtocolServer.nativeEndpoint())),
             QJsonDocument(invalidNativeResponseCall).toJson(QJsonDocument::Compact));
    const auto invalidNativeResponseBody = bodyObject(invalidNativeResponse);
    expect(invalidNativeResponse.status == 200 &&
               jsonRpcErrorCode(invalidNativeResponse) == -32603 &&
               invalidNativeResponseBody.value(QStringLiteral("id")).toString() ==
                   QStringLiteral("native-invalid-id"),
           QStringLiteral("invalid Native handler envelopes must fail closed with the request id"));

    const auto dualMcpCall =
        requestObject(QString::fromLatin1(Mcp::DiscoverMethod), QStringLiteral("dual-mcp"));
    const auto dualMcpResult = send(
        manager,
        baseRequest(QUrl(dualProtocolServer.endpoint()), QString::fromLatin1(Mcp::DiscoverMethod)),
        QJsonDocument(dualMcpCall).toJson(QJsonDocument::Compact));
    expect(dualMcpResult.status == 200,
           QStringLiteral("the MCP route must remain usable beside the Native route"));
}

void TestMcpHttpServer::nativeMcpRouteLifecycle() {
    NativeServerFixture fixture;
    QVERIFY2(fixture.start(), qPrintable(fixture.error));
    auto &dualProtocolServer = fixture.server;
    const auto &dualProtocolLimits = fixture.limits;
    auto &manager = fixture.manager;
    const Mcp::RequestContext legacyContext{
        .protocolVersion = QString::fromLatin1(Mcp::LegacyProtocolVersion),
        .clientCapabilities = {},
        .clientInfo = Mcp::ImplementationInfo{.name = QStringLiteral("legacy-http-client"),
                               .version = QStringLiteral("1.0")},
    };
    const auto legacyPing = Mcp::makeRequest(QString::fromLatin1(Mcp::PingMethod), {},
                                             legacyContext, QStringLiteral("ping"));
    const auto nativeCall = QJsonObject{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")                       },
        {QStringLiteral("id"),      7                                           },
        {QStringLiteral("method"),  QStringLiteral("application.get_status")    },
        {QStringLiteral("params"),  QJsonObject{{QStringLiteral("probe"), true}}},
    };
    const auto dualMcpCall =
        requestObject(QString::fromLatin1(Mcp::DiscoverMethod), QStringLiteral("dual-mcp"));
    const auto sharedPort = dualProtocolServer.port();
    const auto sharedNativeEndpoint = dualProtocolServer.nativeEndpoint();
    const auto sharedMcpEndpoint = dualProtocolServer.endpoint();
    const auto dualLegacyInitialize =
        Mcp::makeInitializeRequest(legacyContext, QStringLiteral("dual-route-init"));
    const auto dualLegacyInitializeResult =
        send(manager, legacyRequest(QUrl(sharedMcpEndpoint), false),
             QJsonDocument(dualLegacyInitialize).toJson(QJsonDocument::Compact));
    expect(dualLegacyInitializeResult.status == 200 &&
               !dualLegacyInitializeResult.sessionId.isEmpty(),
           QStringLiteral("the route-update fixture must establish a legacy MCP session"));
    QString routeUpdateError;
    expect(dualProtocolServer.setRoutes({.mcp = false, .native = true}, routeUpdateError),
           QStringLiteral("the MCP route must be disabled without rebinding: %1")
               .arg(routeUpdateError));
    expect(dualProtocolServer.port() == sharedPort &&
               dualProtocolServer.nativeEndpoint() == sharedNativeEndpoint &&
               dualProtocolServer.endpoint().isEmpty(),
           QStringLiteral("disabling MCP must preserve the Native listener and endpoint"));
    const auto disabledMcp = send(
        manager, baseRequest(QUrl(sharedMcpEndpoint), QString::fromLatin1(Mcp::DiscoverMethod)),
        QJsonDocument(dualMcpCall).toJson(QJsonDocument::Compact));
    expect(disabledMcp.status == 404 &&
               bodyObject(disabledMcp)
                       .value(QStringLiteral("error"))
                       .toObject()
                       .value(QStringLiteral("code")) == QStringLiteral("route_unavailable"),
           QStringLiteral("a disabled MCP route must return a stable route-unavailable response"));
    const auto nativeWhileMcpDisabled =
        send(manager, nativeRequest(QUrl(sharedNativeEndpoint)),
             QJsonDocument(nativeCall).toJson(QJsonDocument::Compact));
    const auto nativeWhileMcpDisabledResult =
        bodyObject(nativeWhileMcpDisabled).value(QStringLiteral("result")).toObject();
    expect(nativeWhileMcpDisabled.status == 200 &&
               nativeWhileMcpDisabledResult.value(QStringLiteral("params"))
                   .toObject()
                   .value(QStringLiteral("probe"))
                   .toBool() &&
               nativeWhileMcpDisabledResult.value(QStringLiteral("client_id_present")).toBool(),
           QStringLiteral("Native requests must remain available while MCP is disabled"));

    routeUpdateError.clear();
    expect(dualProtocolServer.setRoutes({.mcp = true, .native = true}, routeUpdateError),
           QStringLiteral("the MCP route must be re-enabled without rebinding: %1")
               .arg(routeUpdateError));
    expect(dualProtocolServer.port() == sharedPort &&
               dualProtocolServer.nativeEndpoint() == sharedNativeEndpoint &&
               dualProtocolServer.endpoint() == sharedMcpEndpoint,
           QStringLiteral("re-enabling MCP must preserve both shared endpoints"));
    const auto reenabledMcp = send(
        manager, baseRequest(QUrl(sharedMcpEndpoint), QString::fromLatin1(Mcp::DiscoverMethod)),
        QJsonDocument(dualMcpCall).toJson(QJsonDocument::Compact));
    expect(reenabledMcp.status == 200,
           QStringLiteral("the MCP route must accept requests after an in-place re-enable"));
    const auto staleLegacySession =
        send(manager,
             legacyRequest(QUrl(sharedMcpEndpoint), true, Mcp::LegacyProtocolVersion,
                           dualLegacyInitializeResult.sessionId),
             QJsonDocument(legacyPing).toJson(QJsonDocument::Compact));
    expect(staleLegacySession.status == 404,
           QStringLiteral("disabling MCP must retire legacy sessions before a later re-enable"));
    dualProtocolServer.stop();
    expect(dualProtocolServer.endpoint().isEmpty() && dualProtocolServer.nativeEndpoint().isEmpty(),
           QStringLiteral("stopping the shared listener must clear both endpoints"));
}

void TestMcpHttpServer::nativeResponseLimit() {
    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy::NoProxy);
    const auto nativeCall = QJsonObject{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")                       },
        {QStringLiteral("id"),      7                                           },
        {QStringLiteral("method"),  QStringLiteral("application.get_status")    },
        {QStringLiteral("params"),  QJsonObject{{QStringLiteral("probe"), true}}},
    };
    Automation::McpHttpLimits nativeResponseLimits;
    nativeResponseLimits.maximumRequestBytes = 4096;
    nativeResponseLimits.maximumResponseBytes = 1024;
    Automation::McpHttpServer nativeResponseLimitServer(
        QCoreApplication::instance(), Automation::McpHttpServer::RequestHandler{},
        Automation::McpHttpServer::NativeRequestHandler(
            [](const QJsonValue &message, const QString &) {
                return QJsonObject{
                    {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), message.toObject().value(QStringLiteral("id"))},
                    {QStringLiteral("result"), QString(2048, QLatin1Char('x'))},
                };
            }),
        nativeResponseLimits);
    QString nativeResponseLimitError;
    expect(nativeResponseLimitServer.start(0, {.native = true}, nativeResponseLimitError),
           QStringLiteral("Native response-limit server must start: %1")
               .arg(nativeResponseLimitError));
    const auto oversizedNative =
        send(manager, nativeRequest(QUrl(nativeResponseLimitServer.nativeEndpoint())),
             QJsonDocument(nativeCall).toJson(QJsonDocument::Compact));
    const auto oversizedNativeBody = bodyObject(oversizedNative);
    expect(oversizedNative.status == 200 && jsonRpcErrorCode(oversizedNative) == -32603 &&
               oversizedNativeBody.value(QStringLiteral("id")).toInt() == 7 &&
               oversizedNative.body.size() <= nativeResponseLimits.maximumResponseBytes,
           QStringLiteral("oversized Native results must become bounded correlated errors"));
    nativeResponseLimitServer.stop();
}

void TestMcpHttpServer::defaultRequestDeadline() {
    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy::NoProxy);
    const Mcp::ImplementationInfo serverInfo{
        QStringLiteral("DS Editor Lite Test"), QStringLiteral("1.0"), {}, {}};
    const auto discover =
        requestObject(QString::fromLatin1(Mcp::DiscoverMethod), QStringLiteral("discover"));
    const auto basicHandler = [serverInfo](const Mcp::RequestEnvelope &request, const QString &) {
        if (request.method == QString::fromLatin1(Mcp::ToolsListMethod)) {
            return Mcp::makeResultResponse(
                request.id,
                Mcp::makeToolsListResult({}, {}, 0, QStringLiteral("private"), serverInfo),
                serverInfo);
        }
        return Mcp::makeResultResponse(request.id, Mcp::makeDiscoverResult(serverInfo), serverInfo);
    };
    const auto list =
        requestObject(QString::fromLatin1(Mcp::ToolsListMethod), QStringLiteral("list"));

    Automation::McpHttpServer defaultLimitsServer(
        [basicHandler](const Mcp::RequestEnvelope &request, const QString &clientId) {
            QThread::msleep(50);
            return basicHandler(request, clientId);
        });
    QString defaultLimitsError;
    expect(defaultLimitsServer.start(0, defaultLimitsError),
           QStringLiteral("default-limits server must start: %1").arg(defaultLimitsError));
    const auto defaultDeadline = send(
        manager,
        baseRequest(QUrl(defaultLimitsServer.endpoint()), QString::fromLatin1(Mcp::DiscoverMethod)),
        QJsonDocument(discover).toJson(QJsonDocument::Compact));
    expect(!defaultDeadline.timedOut && defaultDeadline.status == 200,
           QStringLiteral("the no-limits constructor must apply server-side default deadlines"));
    defaultLimitsServer.stop();
}

void TestMcpHttpServer::sharedAdmissionAndDeadline() {
    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy::NoProxy);
    const Mcp::ImplementationInfo serverInfo{
        QStringLiteral("DS Editor Lite Test"), QStringLiteral("1.0"), {}, {}};
    const auto discover =
        requestObject(QString::fromLatin1(Mcp::DiscoverMethod), QStringLiteral("discover"));
    const auto nativeCall = QJsonObject{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")                       },
        {QStringLiteral("id"),      7                                           },
        {QStringLiteral("method"),  QStringLiteral("application.get_status")    },
        {QStringLiteral("params"),  QJsonObject{{QStringLiteral("probe"), true}}},
    };
    HandlerThreadFixture executor;
    auto &handlerContext = executor.context;
    QSemaphore globalEntered;
    QSemaphore globalRelease;
    QSemaphore globalDone;
    Automation::McpHttpLimits globalConcurrencyLimits;
    globalConcurrencyLimits.maximumGlobalInFlight = 1;
    globalConcurrencyLimits.requestDeadlineMs = 1000;
    Automation::McpHttpServer globalConcurrencyServer(
        &handlerContext,
        [&](const Mcp::RequestEnvelope &request, const QString &) {
            globalEntered.release();
            globalRelease.acquire();
            globalDone.release();
            return Mcp::makeResultResponse(request.id, Mcp::makeDiscoverResult(serverInfo),
                                           serverInfo);
        },
        [&](const QJsonValue &message, const QString &) {
            globalEntered.release();
            globalRelease.acquire();
            globalDone.release();
            return QJsonObject{
                {QStringLiteral("jsonrpc"), QStringLiteral("2.0")                         },
                {QStringLiteral("id"),      message.toObject().value(QStringLiteral("id"))},
                {QStringLiteral("result"),  QJsonObject{}                                 },
            };
        },
        globalConcurrencyLimits);
    QString globalConcurrencyError;
    expect(globalConcurrencyServer.start(0, {.mcp = true, .native = true}, globalConcurrencyError),
           QStringLiteral("global-concurrency test server must start: %1")
               .arg(globalConcurrencyError));
    const QUrl globalConcurrencyEndpoint(globalConcurrencyServer.endpoint());
    const QUrl globalConcurrencyNativeEndpoint(globalConcurrencyServer.nativeEndpoint());
    auto *globalPending = startRequest(manager, nativeRequest(globalConcurrencyNativeEndpoint),
                                       QJsonDocument(nativeCall).toJson(QJsonDocument::Compact));
    const auto globalHandlerEntered = acquireWhileProcessing(globalEntered, 2000);
    expect(globalHandlerEntered,
           QStringLiteral("the first global-concurrency request must reach the handler"));
    const auto mcpGlobalBusy = send(
        manager, baseRequest(globalConcurrencyEndpoint, QString::fromLatin1(Mcp::DiscoverMethod)),
        QJsonDocument(discover).toJson(QJsonDocument::Compact), HttpMethod::Post, 2000);
    expect(!mcpGlobalBusy.timedOut && mcpGlobalBusy.status == 503,
           QStringLiteral("MCP requests must observe Native-held global transport admission"));
    const auto globalBusy =
        send(manager, nativeRequest(globalConcurrencyNativeEndpoint),
             QJsonDocument(nativeCall).toJson(QJsonDocument::Compact), HttpMethod::Post, 2000);
    expect(!globalBusy.timedOut && globalBusy.status == 503,
           QStringLiteral("MCP and Native routes must share the global transport admission limit"));
    const auto deadlineResult = finishRequest(globalPending, 2500);
    expect(!deadlineResult.timedOut && deadlineResult.status == 504,
           QStringLiteral("Native transport deadlines must finish requests while work continues"));
    globalConcurrencyServer.requestStop();
    expect(waitForStop(globalConcurrencyServer),
           QStringLiteral("listener shutdown must finish while a handler remains blocked"));
    globalRelease.release();
    if (globalHandlerEntered) {
        expect(acquireWhileProcessing(globalDone, 2000),
               QStringLiteral("the timed-out handler must finish after its gate is released"));
    }
}

void TestMcpHttpServer::crossConnectionCancellation_data() {
    QTest::addColumn<bool>("legacy");
    QTest::newRow("modern") << false;
    QTest::newRow("legacy") << true;
}

void TestMcpHttpServer::crossConnectionCancellation() {
    QFETCH(bool, legacy);
    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy::NoProxy);
    const Mcp::ImplementationInfo serverInfo{
        QStringLiteral("DS Editor Lite Test"), QStringLiteral("1.0"), {}, {}};
    HandlerThreadFixture executor;
    auto &handlerContext = executor.context;
    QSemaphore cancellationContextEntered;
    QSemaphore cancellationContextRelease;
    QSemaphore canceledHandlerEntered;
    Automation::McpHttpLimits cancellationLimits;
    cancellationLimits.maximumGlobalInFlight = 1;
    cancellationLimits.requestDeadlineMs = 2000;
    Automation::McpHttpServer cancellationServer(
        &handlerContext,
        [&](const Mcp::RequestEnvelope &request, const QString &) {
            if (request.method == QString::fromLatin1(Mcp::InitializeMethod)) {
                return Mcp::makeResultResponse(
                    request.id, Mcp::makeInitializeResult(request.protocolVersion, serverInfo), {},
                    request.protocolVersion);
            }
            canceledHandlerEntered.release();
            return Mcp::makeResultResponse(request.id,
                                           Mcp::makeToolCallResult(
                                               QJsonObject{
                                                   {QStringLiteral("mutated"), true}
            },
                                               false, {}, serverInfo, request.protocolVersion),
                                           serverInfo, request.protocolVersion);
        },
        cancellationLimits);
    QString cancellationError;
    expect(cancellationServer.start(0, cancellationError),
           QStringLiteral("cancellation server must start: %1").arg(cancellationError));
    const QUrl cancellationEndpoint(cancellationServer.endpoint());
    QNetworkAccessManager cancellationRequestManager;
    QNetworkAccessManager cancellationNotificationManager;
    cancellationRequestManager.setProxy(QNetworkProxy::NoProxy);
    cancellationNotificationManager.setProxy(QNetworkProxy::NoProxy);

    const Mcp::RequestContext legacyContext{
        .protocolVersion = QString::fromLatin1(Mcp::LegacyProtocolVersion),
        .clientCapabilities = {},
        .clientInfo = Mcp::ImplementationInfo{.name = QStringLiteral("legacy-http-client"),
                               .version = QStringLiteral("1.0")},
    };
    QByteArray sessionId;
    if (legacy) {
        const auto initialized = send(
            cancellationRequestManager, legacyRequest(cancellationEndpoint, false),
            QJsonDocument(Mcp::makeInitializeRequest(legacyContext, QStringLiteral("cancel-init")))
                .toJson(QJsonDocument::Compact));
        QCOMPARE(initialized.status, 200);
        QVERIFY(!initialized.sessionId.isEmpty());
        sessionId = initialized.sessionId;
        const auto ready =
            send(cancellationRequestManager,
                 legacyRequest(cancellationEndpoint, true, Mcp::LegacyProtocolVersion, sessionId),
                 QJsonDocument(Mcp::makeRequest(QString::fromLatin1(Mcp::InitializedNotification),
                                                {}, legacyContext))
                     .toJson(QJsonDocument::Compact));
        QCOMPARE(ready.status, 202);
    }

    bool gateReleased = false;
    const auto releaseGate = qScopeGuard([&] {
        if (!gateReleased)
            cancellationContextRelease.release();
    });
    QMetaObject::invokeMethod(
        &handlerContext,
        [&] {
            cancellationContextEntered.release();
            cancellationContextRelease.acquire();
        },
        Qt::QueuedConnection);
    QVERIFY(acquireWhileProcessing(cancellationContextEntered, 2000));
    const QString requestId = QStringLiteral("cancel-before-dispatch");
    const QJsonObject params{
        {QStringLiteral("name"),      QStringLiteral("mutating.test")},
        {QStringLiteral("arguments"), QJsonObject{}                  }
    };
    const auto request =
        legacy ? Mcp::makeRequest(QString::fromLatin1(Mcp::ToolsCallMethod), params, legacyContext,
                                  requestId)
               : withConnectorInstanceId(
                     requestObject(QString::fromLatin1(Mcp::ToolsCallMethod), requestId, params),
                     QStringLiteral("cancel-instance"));
    const auto requestHeaders =
        legacy ? legacyRequest(cancellationEndpoint, true, Mcp::LegacyProtocolVersion, sessionId)
               : baseRequest(cancellationEndpoint, QString::fromLatin1(Mcp::ToolsCallMethod),
                             QStringLiteral("mutating.test"));
    auto *reply = startRequest(cancellationRequestManager, requestHeaders,
                               QJsonDocument(request).toJson(QJsonDocument::Compact));
    QTest::qWait(100);
    const QJsonObject cancellationParams{
        {QStringLiteral("requestId"), requestId                          },
        {QStringLiteral("reason"),    QStringLiteral("test cancellation")}
    };
    const auto notification =
        legacy ? Mcp::makeRequest(QString::fromLatin1(Mcp::CancelledNotification),
                                  cancellationParams, legacyContext)
               : withConnectorInstanceId(
                     requestObject(QString::fromLatin1(Mcp::CancelledNotification),
                                   QJsonValue(QJsonValue::Undefined), cancellationParams),
                     QStringLiteral("cancel-instance"));
    const auto notificationHeaders =
        legacy ? legacyRequest(cancellationEndpoint, true, Mcp::LegacyProtocolVersion, sessionId)
               : baseRequest(cancellationEndpoint, QString::fromLatin1(Mcp::CancelledNotification));
    const auto canceled = send(cancellationNotificationManager, notificationHeaders,
                               QJsonDocument(notification).toJson(QJsonDocument::Compact));
    const auto result = finishRequest(reply, 2000);
    QCOMPARE(canceled.status, 202);
    QVERIFY(canceled.body.isEmpty());
    QCOMPARE(result.status, 204);
    QVERIFY(result.body.isEmpty());
    cancellationContextRelease.release();
    gateReleased = true;
    QVERIFY(!canceledHandlerEntered.tryAcquire(1, 250));
    cancellationServer.stop();
}
QTEST_GUILESS_MAIN(TestMcpHttpServer)
#include "main.moc"
