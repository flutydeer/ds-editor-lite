#include "BootstrapWatcher.h"
#include "ConnectorOptions.h"
#include "ConnectorRuntime.h"
#include "DownstreamMcpServer.h"
#include "ExposurePolicy.h"

#include <lite/AutomationWire/McpProtocol.h>
#include <lite/AutomationWire/JsonSchema.h>

#include <QCoreApplication>
#include <QtTest>
#include "../TestSupport/TestAssertions.h"
#include <QDir>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QFile>
#include <QProcess>
#include <QPointer>
#include <QQueue>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUuid>

#include <functional>
#include <algorithm>
#include <optional>
#include <utility>

namespace {
    QByteArray g_fakeHttpLog;

    using TestSupport::expect;

    QString uniqueBootstrapServiceName() {
        return QStringLiteral("dsc-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    }

    QSet<QString> toolNames(const QJsonArray &tools) {
        QSet<QString> result;
        for (const auto &entry : tools)
            result.insert(entry.toObject().value(QStringLiteral("name")).toString());
        return result;
    }

    QSet<QString> contractNames(const QList<AutomationWire::ToolContract> &contracts) {
        QSet<QString> result;
        for (const auto &contract : contracts)
            result.insert(contract.operationId);
        return result;
    }

    std::optional<QJsonObject> toolMetadata(const QJsonObject &object) {
        const auto minimum = object.contains(QStringLiteral("minimum_toolset_version"))
                                 ? object.value(QStringLiteral("minimum_toolset_version"))
                                 : object.value(QStringLiteral("minimumToolsetVersion"));
        if (minimum.isDouble()) {
            return object;
        }
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (!it.value().isObject())
                continue;
            if (const auto nested = toolMetadata(it.value().toObject()))
                return nested;
        }
        return std::nullopt;
    }

    qint64 metadataInteger(const QJsonObject &metadata, const QString &snake,
                           const QString &camel) {
        const auto value = metadata.contains(snake) ? metadata.value(snake) : metadata.value(camel);
        return value.isDouble() ? value.toInteger() : 0;
    }

    bool hasToolMetadata(const QJsonObject &tool) {
        const auto metadata = toolMetadata(tool.value(QStringLiteral("_meta")).toObject());
        return metadata &&
               metadataInteger(*metadata, QStringLiteral("minimum_toolset_version"),
                               QStringLiteral("minimumToolsetVersion")) >= 1 &&
               !metadata->value(QStringLiteral("category")).toString().isEmpty() &&
               !metadata->value(QStringLiteral("minimum_control_level")).toString().isEmpty();
    }

    bool isToolSummary(const QJsonObject &tool) {
        return !tool.value(QStringLiteral("name")).toString().isEmpty() &&
               !tool.value(QStringLiteral("category")).toString().isEmpty() &&
               !tool.value(QStringLiteral("minimum_control_level")).toString().isEmpty() &&
               tool.value(QStringLiteral("minimum_toolset_version")).toInteger() >= 1 &&
               !tool.value(QStringLiteral("availability")).toString().isEmpty() &&
               !tool.contains(QStringLiteral("inputSchema")) &&
               !tool.contains(QStringLiteral("outputSchema"));
    }

    bool waitUntil(const std::function<bool()> &condition, const int timeoutMs = 3000) {
        QElapsedTimer timer;
        timer.start();
        while (!condition() && timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents();
            QThread::msleep(1);
        }
        return condition();
    }

    std::optional<QJsonObject> takeResponseById(QQueue<QByteArray> &responses, const QJsonValue &id,
                                                const int timeoutMs = 3000) {
        const auto findIndex = [&] {
            for (qsizetype index = 0; index < responses.size(); ++index) {
                const auto response = QJsonDocument::fromJson(responses.at(index)).object();
                if (response.value(QStringLiteral("id")) == id)
                    return index;
            }
            return qsizetype{-1};
        };
        if (!waitUntil([&] { return findIndex() >= 0; }, timeoutMs))
            return std::nullopt;
        return QJsonDocument::fromJson(responses.takeAt(findIndex())).object();
    }

    int runSlowStdioSink(const QString &outputPath) {
        QFile input;
        if (!input.open(stdin, QIODevice::ReadOnly))
            return 2;
        QThread::msleep(250);
        QByteArray received;
        while (true) {
            const auto chunk = input.read(4 * 1024);
            if (chunk.isEmpty()) {
                if (input.atEnd())
                    break;
                return 2;
            }
            received.append(chunk);
            QThread::msleep(10);
        }
        QFile output(outputPath);
        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
            output.write(received) != received.size()) {
            return 2;
        }
        return 0;
    }

    AutomationWire::Mcp::RequestContext clientContext() {
        return {
            .clientCapabilities = QJsonObject{{QStringLiteral("tools"), QJsonObject{}}},
            .clientInfo =
                AutomationWire::Mcp::ImplementationInfo{
                                              .name = QStringLiteral("connector-test"),
                                              .version = QStringLiteral("1"),
                                              },
        };
    }

    class FakeBootstrap final {
    public:
        explicit FakeBootstrap(QString serviceName) : m_serviceName(std::move(serviceName)) {
            QLocalServer::removeServer(m_serviceName);
            QObject::connect(&m_server, &QLocalServer::newConnection, &m_server, [this] {
                while (auto *socket = m_server.nextPendingConnection()) {
                    m_buffers.insert(socket, {});
                    QObject::connect(socket, &QLocalSocket::readyRead, socket,
                                     [this, socket] { readClient(socket); });
                    QObject::connect(socket, &QLocalSocket::disconnected, socket, [this, socket] {
                        m_watchers.removeAll(socket);
                        m_buffers.remove(socket);
                        socket->deleteLater();
                    });
                }
            });
        }

        ~FakeBootstrap() {
            for (auto *watcher : std::as_const(m_watchers))
                watcher->abort();
            m_server.close();
            QLocalServer::removeServer(m_serviceName);
        }

        bool listen() {
            if (m_server.listen(m_serviceName))
                return true;
            qWarning().noquote() << "Bootstrap socket listen failed:" << m_serviceName
                                 << "temporary directory:" << QDir::tempPath()
                                 << "error:" << m_server.errorString();
            return false;
        }

        qsizetype watcherCount() const {
            return m_watchers.size();
        }

        void publish(SingleInstanceAutomationStatus status) {
            m_status = std::move(status);
            const SingleInstanceAutomationSnapshot snapshot{
                {},
                QCoreApplication::applicationPid(),
                m_status,
            };
            const auto frame = SingleInstanceProtocol::frame(
                SingleInstanceProtocol::encodeAutomationSnapshot(snapshot));
            for (auto *watcher : std::as_const(m_watchers))
                watcher->write(frame);
        }

        bool validWatchRequest = false;
        bool emptyFirstRequestId = false;

    private:
        void readClient(QLocalSocket *socket) {
            auto &buffer = m_buffers[socket];
            buffer.append(socket->readAll());
            while (true) {
                QByteArray payload;
                QString frameError;
                if (!SingleInstanceProtocol::takeFrame(buffer, payload, frameError))
                    return;
                SingleInstanceRequest request;
                QString decodeError;
                if (!SingleInstanceProtocol::decodeRequest(payload, request, decodeError) ||
                    request.command != SingleInstanceCommand::AutomationWatch) {
                    socket->abort();
                    return;
                }
                validWatchRequest =
                    !request.connector.instanceId.isEmpty() && !request.connector.version.isEmpty();
                if (!m_watchers.contains(socket))
                    m_watchers.append(socket);
                const SingleInstanceAutomationSnapshot snapshot{
                    emptyFirstRequestId ? QString() : request.requestId,
                    QCoreApplication::applicationPid(),
                    m_status,
                };
                socket->write(SingleInstanceProtocol::frame(
                    SingleInstanceProtocol::encodeAutomationSnapshot(snapshot)));
            }
        }

        QString m_serviceName;
        QLocalServer m_server;
        QHash<QLocalSocket *, QByteArray> m_buffers;
        QList<QLocalSocket *> m_watchers;
        SingleInstanceAutomationStatus m_status;
    };

    class FakeHttpEditor final {
    public:
        enum class ApplicationResponseMode {
            Success,
            Hold,
            BusinessError,
            InvalidOutput,
            ProtocolError,
            TransportError,
            Redirect,
            Sse,
            RepeatedSse,
            MalformedJson,
            UnsupportedContentType,
            Oversized,
        };

        FakeHttpEditor() {
            QObject::connect(&m_server, &QTcpServer::newConnection, &m_server, [this] {
                while (auto *socket = m_server.nextPendingConnection()) {
                    m_buffers.insert(socket, {});
                    QObject::connect(socket, &QTcpSocket::readyRead, &m_server,
                                     [this, socket] { readRequest(socket); });
                    QObject::connect(socket, &QTcpSocket::disconnected, &m_server, [this, socket] {
                        m_buffers.remove(socket);
                        socket->deleteLater();
                    });
                }
            });
        }

        ~FakeHttpEditor() {
            g_fakeHttpLog.append(m_rawLog);
        }

        bool listen() {
            return m_server.listen(QHostAddress::LocalHost, 0);
        }

        QString endpoint() const {
            return QStringLiteral("http://127.0.0.1:%1/mcp").arg(m_server.serverPort());
        }

        QByteArray rawLog() const {
            return m_rawLog;
        }

        QSet<QString> advertisedToolNames() const {
            return toolNames(allTools());
        }

        qsizetype connectionCount() const {
            return m_buffers.size();
        }

        bool headersValid = true;
        bool exposeNotes = false;
        bool exposeFilteredTool = false;
        bool applicationSchemaVariant = false;
        bool annotatedApplicationHeaders = false;
        bool exposeInvalidAnnotatedTool = false;
        bool exposeCommandTool = false;
        bool exposeForwardCompatibleTools = false;
        bool legacyOnly = false;
        QByteArray legacySessionId;
        QString negotiatedLegacyProtocolVersion =
            QString::fromLatin1(AutomationWire::Mcp::LegacyProtocolVersion);
        QString applicationAvailability;
        ApplicationResponseMode applicationResponseMode = ApplicationResponseMode::Success;
        int applicationTransportStatus = 429;
        QString applicationTransportCode = QStringLiteral("too_many_requests");
        QString applicationTransportMessage = QStringLiteral("fake request limit reached");
        bool applicationTransportExtraField = false;
        int extraToolCount = 0;
        int pageSize = 0;
        AutomationWire::ControlLevel editorControlLevel = AutomationWire::ControlLevel::L1;
        int editorToolsetVersion = static_cast<int>(AutomationWire::PublicToolsetVersion);
        int applicationMinimumToolsetVersion = 1;
        int discoverResponseDelayMs = 0;
        int discoverRateLimitFailuresRemaining = 0;
        int discoverCount = 0;
        int initializeCount = 0;
        int initializedNotificationCount = 0;
        int cancelledNotificationCount = 0;
        int toolsListCount = 0;
        int statusCallCount = 0;
        QList<QJsonValue> requestIds;
        QList<QJsonObject> cancelledNotificationParams;
        QStringList calledTools;
        QHash<QByteArray, QByteArray> lastParameterHeaders;

    private:
        QJsonObject annotatedInputSchema(const bool valid = true) const {
            return {
                {QStringLiteral("$schema"),
                 QStringLiteral("https://json-schema.org/draft/2020-12/schema")  },
                {QStringLiteral("type"),                 QStringLiteral("object")},
                {QStringLiteral("properties"),
                 QJsonObject{
                     {QStringLiteral("route"),
                      QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                  {QStringLiteral("x-mcp-header"),
                                   valid ? QStringLiteral("Route") : QStringLiteral("Bad Name")}}},
                     {QStringLiteral("retry"),
                      QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")},
                                  {QStringLiteral("x-mcp-header"), QStringLiteral("Retry")}}},
                     {QStringLiteral("enabled"),
                      QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")},
                                  {QStringLiteral("x-mcp-header"), QStringLiteral("Enabled")}}},
                     {QStringLiteral("nested"),
                      QJsonObject{
                          {QStringLiteral("type"), QStringLiteral("object")},
                          {QStringLiteral("properties"),
                           QJsonObject{
                               {QStringLiteral("region"),
                                QJsonObject{
                                    {QStringLiteral("type"), QStringLiteral("string")},
                                    {QStringLiteral("x-mcp-header"), QStringLiteral("Region")}}}}},
                          {QStringLiteral("required"), QJsonArray{QStringLiteral("region")}},
                          {QStringLiteral("additionalProperties"), false},
                      }},
                 }                                                               },
                {QStringLiteral("required"),
                 QJsonArray{QStringLiteral("route"), QStringLiteral("retry"),
                            QStringLiteral("enabled"), QStringLiteral("nested")} },
                {QStringLiteral("additionalProperties"), false                   },
            };
        }

        QJsonObject flexibleInputSchema() const {
            return {
                {QStringLiteral("type"),                 QStringLiteral("object")           },
                {QStringLiteral("properties"),
                 QJsonObject{
                     {QStringLiteral("shape"),
                      QJsonObject{
                          {QStringLiteral("type"), QStringLiteral("string")},
                          {QStringLiteral("enum"),
                           QJsonArray{QStringLiteral("string"), QStringLiteral("array"),
                                      QStringLiteral("null"), QStringLiteral("invalid")}}}}}},
                {QStringLiteral("required"),             QJsonArray{QStringLiteral("shape")}},
                {QStringLiteral("additionalProperties"), false                              },
            };
        }

        QJsonObject flexibleOutputSchema() const {
            return {
                {QStringLiteral("oneOf"),
                 QJsonArray{
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}},
                     QJsonObject{
                         {QStringLiteral("type"), QStringLiteral("array")},
                         {QStringLiteral("items"),
                          QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}}}},
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("null")}},
                 }},
            };
        }

        QJsonArray allTools() const {
            const auto cacheKey = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9|%10")
                                      .arg(exposeNotes)
                                      .arg(exposeFilteredTool)
                                      .arg(applicationSchemaVariant)
                                      .arg(annotatedApplicationHeaders)
                                      .arg(exposeInvalidAnnotatedTool)
                                      .arg(exposeCommandTool)
                                      .arg(applicationAvailability)
                                      .arg(extraToolCount)
                                      .arg(exposeForwardCompatibleTools)
                                      .arg(applicationMinimumToolsetVersion);
            if (cacheKey == m_toolsCacheKey)
                return m_cachedTools;
            QJsonArray tools;
            auto applicationTool =
                AutomationWire::findPublicTool(QStringLiteral("application.get_info"))
                    ->toMcpToolJson();
            auto applicationMetadata = applicationTool.value(QStringLiteral("_meta"))
                                           .toObject()
                                           .value(QStringLiteral("org.openvpi.ds-editor-lite/tool"))
                                           .toObject();
            applicationMetadata.insert(QStringLiteral("minimum_toolset_version"),
                                       applicationMinimumToolsetVersion);
            auto applicationMeta = applicationTool.value(QStringLiteral("_meta")).toObject();
            applicationMeta.insert(QStringLiteral("org.openvpi.ds-editor-lite/tool"),
                                   applicationMetadata);
            applicationTool.insert(QStringLiteral("_meta"), applicationMeta);
            if (!applicationAvailability.isEmpty())
                applicationTool.insert(QStringLiteral("availability"), applicationAvailability);
            if (annotatedApplicationHeaders)
                applicationTool.insert(QStringLiteral("inputSchema"), annotatedInputSchema());
            if (applicationSchemaVariant) {
                applicationTool.insert(
                    QStringLiteral("outputSchema"),
                    QJsonObject{
                        {QStringLiteral("type"),                 QStringLiteral("object")                  },
                        {QStringLiteral("properties"),
                         QJsonObject{
                             {QStringLiteral("incompatible"),
                              QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}}}           },
                        {QStringLiteral("required"),             QJsonArray{QStringLiteral("incompatible")}},
                        {QStringLiteral("additionalProperties"), false                                     },
                });
            }
            tools.append(applicationTool);
            tools.append(AutomationWire::findPublicTool(QStringLiteral("application.get_status"))
                             ->toMcpToolJson());
            tools.append(AutomationWire::findPublicTool(QStringLiteral("application.request_exit"))
                             ->toMcpToolJson());
            tools.append(
                AutomationWire::findPublicTool(QStringLiteral("application.request_restart"))
                    ->toMcpToolJson());
            if (exposeNotes) {
                tools.append(
                    AutomationWire::findPublicTool(QStringLiteral("notes.list"))->toMcpToolJson());
            }
            if (exposeFilteredTool) {
                tools.append(AutomationWire::findPublicTool(QStringLiteral("documents.get"))
                                 ->toMcpToolJson());
            }
            if (exposeInvalidAnnotatedTool) {
                auto invalid = applicationTool;
                invalid.insert(QStringLiteral("name"), QStringLiteral("fake.invalid_header"));
                invalid.insert(QStringLiteral("title"), QStringLiteral("Invalid header mapping"));
                invalid.insert(QStringLiteral("inputSchema"), annotatedInputSchema(false));
                tools.append(invalid);
            }
            if (exposeCommandTool) {
                auto command = applicationTool;
                command.insert(QStringLiteral("name"), QStringLiteral("fake.command"));
                command.insert(QStringLiteral("title"), QStringLiteral("Fake command"));
                auto metadata = command.value(QStringLiteral("_meta"))
                                    .toObject()
                                    .value(QStringLiteral("org.openvpi.ds-editor-lite/tool"))
                                    .toObject();
                metadata.insert(QStringLiteral("kind"), QStringLiteral("command"));
                auto meta = command.value(QStringLiteral("_meta")).toObject();
                meta.insert(QStringLiteral("org.openvpi.ds-editor-lite/tool"), metadata);
                command.insert(QStringLiteral("_meta"), meta);
                tools.append(command);
            }
            if (exposeForwardCompatibleTools) {
                tools.append(QJsonObject{
                    {QStringLiteral("name"),         QStringLiteral("fake.flexible_output")},
                    {QStringLiteral("inputSchema"),  flexibleInputSchema()                 },
                    {QStringLiteral("outputSchema"), flexibleOutputSchema()                },
                    {QStringLiteral("icons"),
                     QJsonArray{QJsonObject{
                         {QStringLiteral("src"),
                          QStringLiteral("https://example.invalid/tool.svg")},
                         {QStringLiteral("mimeType"), QStringLiteral("image/svg+xml")},
                     }}                                                                    },
                    {QStringLiteral("_meta"),
                     QJsonObject{
                         {QStringLiteral("org.openvpi.ds-editor-lite/fixture"), true},
                         {QStringLiteral("org.openvpi.ds-editor-lite/tool"),
                          QJsonObject{
                              {QStringLiteral("minimum_toolset_version"), 1},
                              {QStringLiteral("category"), QStringLiteral("fake")},
                              {QStringLiteral("minimum_control_level"), QStringLiteral("l0")},
                              {QStringLiteral("sync_mode"), QStringLiteral("synchronous")},
                              {QStringLiteral("value_sources"), QJsonArray{}},
                              {QStringLiteral("kind"), QStringLiteral("query")},
                              {QStringLiteral("host_availability"), QStringLiteral("gui")},
                          }},
                     }                                                                     },
                });
                tools.append(QJsonObject{
                    {QStringLiteral("name"),        QStringLiteral("fake.minimal")},
                    {QStringLiteral("inputSchema"),
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                                 {QStringLiteral("additionalProperties"), false}} },
                    {QStringLiteral("icons"),
                     QJsonArray{QJsonObject{
                         {QStringLiteral("src"),
                          QStringLiteral("https://example.invalid/minimal.svg")},
                     }}                                                           },
                    {QStringLiteral("_meta"),
                     QJsonObject{{QStringLiteral("org.openvpi.ds-editor-lite/fixture"),
                                  QStringLiteral("minimal")}}                     },
                });
            }
            for (auto index = 0; index < extraToolCount; ++index) {
                auto tool = applicationTool;
                const auto id = QStringLiteral("fake.tool.%1").arg(index, 3, 10, QLatin1Char('0'));
                tool.insert(QStringLiteral("name"), id);
                tool.insert(QStringLiteral("title"), id);
                auto metadata = tool.value(QStringLiteral("_meta"))
                                    .toObject()
                                    .value(QStringLiteral("org.openvpi.ds-editor-lite/tool"))
                                    .toObject();
                metadata.insert(QStringLiteral("category"), QStringLiteral("fake"));
                metadata.insert(QStringLiteral("minimum_control_level"), QStringLiteral("l0"));
                auto meta = tool.value(QStringLiteral("_meta")).toObject();
                meta.insert(QStringLiteral("org.openvpi.ds-editor-lite/tool"), metadata);
                tool.insert(QStringLiteral("_meta"), meta);
                tools.append(tool);
            }
            m_toolsCacheKey = cacheKey;
            m_cachedTools = tools;
            return m_cachedTools;
        }

        QJsonArray page(const QJsonArray &items, const int offset, QString &nextCursor) const {
            const auto count = pageSize > 0 ? pageSize : items.size();
            QJsonArray result;
            for (auto index = offset; index < items.size() && index < offset + count; ++index)
                result.append(items.at(index));
            nextCursor = offset + result.size() < items.size()
                             ? QString::number(offset + result.size())
                             : QString();
            return result;
        }

        void readRequest(QTcpSocket *socket) {
            auto &buffer = m_buffers[socket];
            buffer.append(socket->readAll());
            const auto headerEnd = buffer.indexOf("\r\n\r\n");
            if (headerEnd < 0)
                return;
            const auto headerBlock = buffer.left(headerEnd);
            QHash<QByteArray, QByteArray> headers;
            const auto lines = headerBlock.split('\n');
            for (qsizetype index = 1; index < lines.size(); ++index) {
                const auto line = lines.at(index).trimmed();
                const auto separator = line.indexOf(':');
                if (separator > 0)
                    headers.insert(line.left(separator).trimmed().toLower(),
                                   line.sliced(separator + 1).trimmed());
            }
            bool lengthOk = false;
            const auto contentLength = headers.value("content-length").toInt(&lengthOk);
            if (!lengthOk || buffer.size() < headerEnd + 4 + contentLength)
                return;
            const auto body = buffer.mid(headerEnd + 4, contentLength);
            m_rawLog.append("=== request ===\n");
            m_rawLog.append(buffer.first(headerEnd + 4 + contentLength));
            m_rawLog.append('\n');
            buffer.remove(0, headerEnd + 4 + contentLength);

            const auto requestObject = QJsonDocument::fromJson(body).object();
            const auto impliedProtocolVersion =
                QString::fromLatin1(headers.value("mcp-protocol-version"));
            const auto validation = AutomationWire::Mcp::validateRequest(
                requestObject, AutomationWire::Mcp::supportedProtocolVersions(),
                impliedProtocolVersion);
            if (!validation.valid()) {
                respond(socket, AutomationWire::Mcp::makeErrorResponse(
                                    requestObject.value(QStringLiteral("id")), validation.error));
                return;
            }
            const auto &request = *validation.request;
            const auto modern =
                AutomationWire::Mcp::isModernProtocolVersion(request.protocolVersion);
            headersValid &= headers.value("accept").contains("application/json") &&
                            headers.value("accept").contains("text/event-stream");
            if (modern) {
                headersValid &= headers.value("mcp-protocol-version") ==
                                    QByteArray(AutomationWire::Mcp::ProtocolVersion) &&
                                headers.value("mcp-method") == request.method.toUtf8() &&
                                !request.meta
                                     .value(QString::fromLatin1(
                                         AutomationWire::Mcp::ConnectorInstanceIdMetaKey))
                                     .toString()
                                     .isEmpty();
            } else {
                const auto initialize =
                    request.method == QString::fromLatin1(AutomationWire::Mcp::InitializeMethod);
                headersValid &= (initialize ? headers.value("mcp-protocol-version").isEmpty()
                                            : headers.value("mcp-protocol-version") ==
                                                  request.protocolVersion.toLatin1()) &&
                                headers.value("mcp-method").isEmpty() &&
                                headers.value("mcp-name").isEmpty();
                if (!legacySessionId.isEmpty()) {
                    headersValid &= initialize ? headers.value("mcp-session-id").isEmpty()
                                               : headers.value("mcp-session-id") == legacySessionId;
                }
            }
            if (modern &&
                request.method == QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod)) {
                QString nameHeaderError;
                const auto nameHeader = AutomationWire::Mcp::decodeHeaderValue(
                    QString::fromUtf8(headers.value("mcp-name")), &nameHeaderError);
                headersValid &= nameHeader && *nameHeader == request.name;
                calledTools.append(request.name);
                lastParameterHeaders.clear();
                for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
                    if (it.key().startsWith("mcp-param-"))
                        lastParameterHeaders.insert(it.key(), it.value());
                }
            } else if (request.method ==
                       QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod)) {
                calledTools.append(request.name);
                lastParameterHeaders.clear();
                for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
                    if (it.key().startsWith("mcp-param-"))
                        lastParameterHeaders.insert(it.key(), it.value());
                }
            }

            AutomationWire::Mcp::ImplementationInfo info{
                .name = QStringLiteral("fake-editor"),
                .version = QStringLiteral("1"),
            };
            if (request.method == QString::fromLatin1(AutomationWire::Mcp::InitializeMethod)) {
                ++initializeCount;
                requestIds.append(request.id);
                respond(socket,
                        AutomationWire::Mcp::makeResultResponse(
                            request.id,
                            AutomationWire::Mcp::makeInitializeResult(
                                negotiatedLegacyProtocolVersion, info),
                            {}, negotiatedLegacyProtocolVersion),
                        legacySessionId.isEmpty()
                            ? QByteArray{}
                            : QByteArrayLiteral("MCP-Session-Id: ") + legacySessionId + "\r\n");
                return;
            }
            if (request.method ==
                QString::fromLatin1(AutomationWire::Mcp::InitializedNotification)) {
                ++initializedNotificationCount;
                respondAccepted(socket);
                return;
            }
            if (request.method == QString::fromLatin1(AutomationWire::Mcp::CancelledNotification)) {
                ++cancelledNotificationCount;
                cancelledNotificationParams.append(request.params);
                respondAccepted(socket);
                return;
            }
            if (request.notification) {
                respondAccepted(socket);
                return;
            }
            requestIds.append(request.id);
            QJsonObject result;
            if (request.method == QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod)) {
                ++discoverCount;
                if (legacyOnly) {
                    respond(socket,
                            AutomationWire::Mcp::makeErrorResponse(
                                request.id, {AutomationWire::Mcp::ServerNotInitialized,
                                             QStringLiteral("MCP server is not initialized")}));
                    return;
                }
                if (discoverRateLimitFailuresRemaining > 0) {
                    --discoverRateLimitFailuresRemaining;
                    respondTransportError(socket, 429, QStringLiteral("too_many_requests"),
                                          QStringLiteral("fake handshake request limit reached"));
                    return;
                }
                result = AutomationWire::Mcp::makeDiscoverResult(info);
                if (discoverResponseDelayMs > 0) {
                    const auto response = AutomationWire::Mcp::makeResultResponse(
                        request.id, result, info, request.protocolVersion);
                    const QPointer<QTcpSocket> guardedSocket(socket);
                    QTimer::singleShot(discoverResponseDelayMs, &m_server,
                                       [this, guardedSocket, response] {
                                           if (guardedSocket)
                                               respond(guardedSocket, response);
                                       });
                    return;
                }
            } else if (request.method ==
                       QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod)) {
                ++toolsListCount;
                bool cursorValid = true;
                const auto cursorText = request.params.value(QStringLiteral("cursor")).toString();
                const auto offset = cursorText.isEmpty() ? 0 : cursorText.toInt(&cursorValid);
                QString nextCursor;
                const auto tools = page(allTools(), cursorValid ? offset : 0, nextCursor);
                result = AutomationWire::Mcp::makeToolsListResult(
                    tools, nextCursor, 0, QStringLiteral("private"), info, request.protocolVersion);
            } else if (request.name == QStringLiteral("application.get_status")) {
                ++statusCallCount;
                result = AutomationWire::Mcp::makeToolCallResult(
                    QJsonObject{
                        {QStringLiteral("editor_instance_id"),
                         QStringLiteral("11111111-1111-4111-8111-111111111111")     },
                        {QStringLiteral("host_mode"),          QStringLiteral("gui")},
                        {QStringLiteral("control_level"),
                         AutomationWire::controlLevelName(editorControlLevel)       },
                        {QStringLiteral("toolset_version"),    editorToolsetVersion },
                        {QStringLiteral("documents"),          QJsonArray{}         },
                        {QStringLiteral("windows"),            QJsonArray{}         },
                },
                    false, {}, {}, request.protocolVersion);
            } else if (request.name == QStringLiteral("application.request_exit") ||
                       request.name == QStringLiteral("application.request_restart")) {
                const auto discardChanges = request.params.value(QStringLiteral("arguments"))
                                                .toObject()
                                                .value(QStringLiteral("discard_changes"))
                                                .toBool(false);
                result = AutomationWire::Mcp::makeToolCallResult(
                    QJsonObject{
                        {QStringLiteral("accepted"),        true          },
                        {QStringLiteral("action"),
                         request.name.endsWith(QStringLiteral("request_exit"))
                             ? QStringLiteral("exit")
                             : QStringLiteral("restart")                  },
                        {QStringLiteral("discard_changes"), discardChanges},
                },
                    false, {}, {}, request.protocolVersion);
            } else if (request.name == QStringLiteral("fake.flexible_output")) {
                const auto shape = request.params.value(QStringLiteral("arguments"))
                                       .toObject()
                                       .value(QStringLiteral("shape"))
                                       .toString();
                QJsonValue structuredContent;
                if (shape == QStringLiteral("string"))
                    structuredContent = QStringLiteral("flexible");
                else if (shape == QStringLiteral("array"))
                    structuredContent = QJsonArray{1, 2, 3};
                else if (shape == QStringLiteral("null"))
                    structuredContent = QJsonValue(QJsonValue::Null);
                else
                    structuredContent = QJsonObject{
                        {QStringLiteral("unexpected"), true}
                    };
                respond(socket, AutomationWire::Mcp::makeResultResponse(
                                    request.id,
                                    AutomationWire::Mcp::makeToolCallResult(
                                        structuredContent, false, {}, {}, request.protocolVersion),
                                    info, request.protocolVersion));
                return;
            } else if (request.name == QStringLiteral("fake.minimal")) {
                respond(socket, AutomationWire::Mcp::makeResultResponse(
                                    request.id,
                                    AutomationWire::Mcp::makeToolCallResult(
                                        QJsonObject{}, false, {}, {}, request.protocolVersion),
                                    info, request.protocolVersion));
                return;
            } else if (request.name == QStringLiteral("application.get_info") ||
                       request.name == QStringLiteral("fake.command")) {
                const auto mode = applicationResponseMode;
                if (mode == ApplicationResponseMode::Hold)
                    return;
                applicationResponseMode = ApplicationResponseMode::Success;
                if (mode == ApplicationResponseMode::ProtocolError) {
                    respond(socket, AutomationWire::Mcp::makeErrorResponse(
                                        request.id, {AutomationWire::Mcp::InvalidParams,
                                                     QStringLiteral("fake protocol error")}));
                    return;
                }
                if (mode == ApplicationResponseMode::TransportError) {
                    respondTransportError(socket, applicationTransportStatus,
                                          applicationTransportCode, applicationTransportMessage,
                                          applicationTransportExtraField);
                    return;
                }
                if (mode == ApplicationResponseMode::Oversized) {
                    respondOversized(socket);
                    return;
                }
                if (mode == ApplicationResponseMode::Redirect) {
                    respondRedirect(socket, request.id);
                    return;
                }
                result = AutomationWire::Mcp::makeToolCallResult(
                    mode == ApplicationResponseMode::BusinessError
                        ? QJsonObject{{QStringLiteral("code"),
                                       QStringLiteral("fake_business_error")},
                                      {QStringLiteral("message"),
                                       QStringLiteral("fake business error")}}
                        : mode == ApplicationResponseMode::InvalidOutput
                            ? QJsonObject{{QStringLiteral("leaked_secret"),
                                           QStringLiteral("must-not-pass")}}
                        : QJsonObject{{QStringLiteral("name"),
                                       QStringLiteral("DS Editor Lite")},
                                      {QStringLiteral("version"), QStringLiteral("test")},
                                      {QStringLiteral("platform"),
                                       QStringLiteral("windows")},
                                      {QStringLiteral("build_id"),
                                       QStringLiteral("fake-build")}},
                    mode == ApplicationResponseMode::BusinessError);
                const auto response = AutomationWire::Mcp::makeResultResponse(
                    request.id, result, info, request.protocolVersion);
                if (mode == ApplicationResponseMode::Sse ||
                    mode == ApplicationResponseMode::RepeatedSse)
                    respondSse(socket, response, mode == ApplicationResponseMode::RepeatedSse);
                else if (mode == ApplicationResponseMode::MalformedJson)
                    respondBody(socket, "{incomplete", "application/json");
                else if (mode == ApplicationResponseMode::UnsupportedContentType)
                    respondBody(socket, "<html>Proxy failure</html>", "text/html");
                else
                    respond(socket, response);
                return;
            } else {
                respond(socket, AutomationWire::Mcp::makeErrorResponse(
                                    request.id, {AutomationWire::Mcp::InvalidParams,
                                                 QStringLiteral("unknown fake tool")}));
                return;
            }
            respond(socket, AutomationWire::Mcp::makeResultResponse(request.id, result, info,
                                                                    request.protocolVersion));
        }

        void respond(QTcpSocket *socket, const QJsonObject &response,
                     const QByteArray &extraHeaders = {}) {
            respondBody(socket, QJsonDocument(response).toJson(QJsonDocument::Compact),
                        "application/json", extraHeaders);
        }

        void respondBody(QTcpSocket *socket, const QByteArray &body, const QByteArray &contentType,
                         const QByteArray &extraHeaders = {}) {
            QByteArray message =
                "HTTP/1.1 200 OK\r\nContent-Type: " + contentType + "\r\nConnection: close\r\n";
            message.append(extraHeaders);
            message.append("Content-Length: ");
            message.append(QByteArray::number(body.size()));
            message.append("\r\n\r\n");
            message.append(body);
            m_rawLog.append("=== response ===\n");
            m_rawLog.append(message);
            m_rawLog.append('\n');
            socket->write(message);
            socket->disconnectFromHost();
        }

        void respondAccepted(QTcpSocket *socket) {
            const QByteArray message = "HTTP/1.1 202 Accepted\r\nConnection: close\r\n"
                                       "Content-Length: 0\r\n\r\n";
            m_rawLog.append("=== accepted ===\n");
            m_rawLog.append(message);
            m_rawLog.append('\n');
            socket->write(message);
            socket->disconnectFromHost();
        }

        void respondSse(QTcpSocket *socket, const QJsonObject &response,
                        bool repeatResponse = false) {
            const auto body = QJsonDocument(response).toJson(QJsonDocument::Compact);
            const auto split = body.indexOf(",\"result\"");
            QByteArray events = ": keep-alive\r\n\r\n"
                                "event: message\r\n"
                                "data: {\"jsonrpc\":\"2.0\",\"method\":"
                                "\"notifications/progress\",\"params\":{}}\r\n\r\n"
                                "event: message\r\n";
            if (split > 0) {
                events.append("data: ");
                events.append(body.first(split + 1));
                events.append("\r\ndata: ");
                events.append(body.sliced(split + 1));
                events.append("\r\n\r\n");
            } else {
                events.append("data: ");
                events.append(body);
                events.append("\r\n\r\n");
            }
            if (repeatResponse)
                events.append("data: " + body + "\r\n\r\n");
            respondBody(socket, events, "Text/Event-Stream; Charset=UTF-8");
        }

        void respondOversized(QTcpSocket *socket) {
            const QByteArray body(16 * 1024 * 1024 + 1024, 'x');
            QByteArray message = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                                 "Connection: close\r\nContent-Length: ";
            message.append(QByteArray::number(body.size()));
            message.append("\r\n\r\n");
            socket->write(message);
            socket->write(body);
            socket->disconnectFromHost();
        }

        void respondRedirect(QTcpSocket *socket, const QJsonValue &id) {
            const auto body = QJsonDocument(AutomationWire::Mcp::makeResultResponse(
                                                id, AutomationWire::Mcp::makeToolCallResult({})))
                                  .toJson(QJsonDocument::Compact);
            QByteArray message = "HTTP/1.1 302 Found\r\nContent-Type: application/json\r\n"
                                 "Location: http://127.0.0.1:1/mcp\r\n"
                                 "Connection: close\r\nContent-Length: ";
            message.append(QByteArray::number(body.size()));
            message.append("\r\n\r\n");
            message.append(body);
            socket->write(message);
            socket->disconnectFromHost();
        }

        void respondTransportError(QTcpSocket *socket, const int status, const QString &code,
                                   const QString &errorMessage, const bool extraField = false) {
            QJsonObject envelope{
                {QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), code},
                                                      {QStringLiteral("message"), errorMessage}}},
            };
            if (extraField)
                envelope.insert(QStringLiteral("unexpected"), true);
            const auto body = QJsonDocument(envelope).toJson(QJsonDocument::Compact);
            QByteArray reason = "Error";
            if (status == 429)
                reason = "Too Many Requests";
            else if (status == 503)
                reason = "Service Unavailable";
            else if (status == 504)
                reason = "Gateway Timeout";
            QByteArray message = "HTTP/1.1 ";
            message.append(QByteArray::number(status));
            message.append(' ');
            message.append(reason);
            message.append("\r\nContent-Type: application/json\r\n"
                           "Connection: close\r\nContent-Length: ");
            message.append(QByteArray::number(body.size()));
            message.append("\r\n\r\n");
            message.append(body);
            m_rawLog.append("=== transport response ===\n");
            m_rawLog.append(message);
            m_rawLog.append('\n');
            socket->write(message);
            socket->disconnectFromHost();
        }

        QHash<QTcpSocket *, QByteArray> m_buffers;
        QByteArray m_rawLog;
        mutable QString m_toolsCacheKey;
        mutable QJsonArray m_cachedTools;
        QTcpServer m_server;
    };

    class ConnectedEditorFixture final {
    public:
        FakeHttpEditor http;
        const QString serviceName = uniqueBootstrapServiceName();
        FakeBootstrap bootstrap{serviceName};
        SingleInstanceAutomationStatus ready;
        DsConnector::ConnectorOptions options{
            .exposure = {.controlLevel = AutomationWire::ExposureLevel::L0,
                         .includes = {QStringLiteral("id:application.get_info"),
                                      QStringLiteral("id:application.get_status"),
                                      QStringLiteral("id:notes.list")}},
            .upstreamTimeoutMs = 2000,
        };
        DsConnector::ConnectorRuntime runtime{options, serviceName};
        QQueue<QByteArray> responses;
        DsConnector::DownstreamMcpServer server{&runtime};
        const AutomationWire::Mcp::RequestContext context = clientContext();

        ConnectedEditorFixture() {
            QObject::connect(&server, &DsConnector::DownstreamMcpServer::responseLine, &server,
                             [this](const QByteArray &line) { responses.enqueue(line); });
        }

        ~ConnectedEditorFixture() {
            runtime.stop();
        }

        bool start() {
            if (!http.listen() || !bootstrap.listen())
                return false;
            http.exposeFilteredTool = true;
            ready = {
                .state = SingleInstanceAutomationState::ServerReady,
                .editorInstanceId = QUuid::createUuid().toString(QUuid::WithoutBraces),
                .executablePath = QCoreApplication::applicationFilePath(),
                .applicationVersion = QStringLiteral("test"),
                .buildId = QStringLiteral("fake-build"),
                .hostMode = QStringLiteral("gui"),
                .serverEnabled = true,
                .serverEndpoint = http.endpoint(),
            };
            bootstrap.publish(ready);
            runtime.start();
            return waitUntil(
                [this] {
                    const auto status = runtime.status();
                    return status.value(QStringLiteral("mcp"))
                               .toObject()
                               .value(QStringLiteral("connected"))
                               .toBool() &&
                           status.value(QStringLiteral("toolset"))
                                   .toObject()
                                   .value(QStringLiteral("compatibility")) ==
                               QStringLiteral("compatible");
                },
                10000);
        }

        void sendTool(const QString &id, const QString &name, const QJsonObject &arguments = {}) {
            server.processLine(
                QJsonDocument(AutomationWire::Mcp::makeRequest(
                                  QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                                  {
                                      {QStringLiteral("name"),      name     },
                                      {QStringLiteral("arguments"), arguments}
            },
                                  context, id))
                    .toJson(QJsonDocument::Compact));
        }

        std::optional<QJsonObject> response(const QString &id, int timeoutMs = 5000) {
            return takeResponseById(responses, id, timeoutMs);
        }
    };

    class TestConnector final : public QObject {
        Q_OBJECT

    private slots:

        void cleanup() {
            g_fakeHttpLog.clear();
        }

        void optionsAndExposure();
        void offlineDownstream();
        void offlineBootstrapError();
        void unavailableEditorStates();
        void bootstrapCorrelation();
        void handshakeCoordination();
        void connectedIdentity();
        void forwardedCalls_data();
        void forwardedCalls();
        void genericDiscovery();
        void genericExposure_data();
        void genericExposure();
        void duplicateRequestAndCancellation();
        void upstreamResponses_data();
        void upstreamResponses();
        void editorPolicyRefresh();
        void schemaRefreshKeepsVersionCompatibility();
        void backpressureAndCancellation();
        void upstreamDisabledKeepsDownstreamTools();
        void forwardCompatibleGenericTools();
        void compatibilityVersions();
        void headlessHostAvailability();
        void parameterHeaders();
        void commandTransportOutcome();
        void paginatedHandshake();
        void concurrentConnectors();
        void stdioFraming();
    };

    void TestConnector::optionsAndExposure() {
        DsConnector::ConnectorOptions options;
        QString error;
        expect(
            DsConnector::parseConnectorOptions(
                {QStringLiteral("--control-level"), QStringLiteral("l0"),
                 QStringLiteral("--include-tool=id:notes.insert"), QStringLiteral("--include-tool"),
                 QStringLiteral("notes.insert"), QStringLiteral("--exclude-tool=category:notes")},
                options, error),
            "valid connector options must parse");
        expect(options.exposure.controlLevel == AutomationWire::ExposureLevel::L0 &&
                   options.exposure.includes.size() == 1 && options.exposure.excludes.size() == 1,
               "connector options must normalize control levels and duplicate selectors");
        DsConnector::ExposurePolicy policy(options);
        const QSet<QString> intrinsicIds{
            QStringLiteral("application.get_info"),
            QStringLiteral("application.get_status"),
            QStringLiteral("application.request_exit"),
            QStringLiteral("application.request_restart"),
        };
        expect(contractNames(policy.typedContracts()) == intrinsicIds,
               "exclude must win over an exact include while retaining L0 tools");

        DsConnector::ConnectorOptions protectedOptions;
        expect(DsConnector::parseConnectorOptions(
                   {QStringLiteral("--control-level=l0"),
                    QStringLiteral("--exclude-tool=category:application")},
                   protectedOptions, error),
               "an exclusion matching an L0 category must remain a valid connector option");
        DsConnector::ExposurePolicy protectedPolicy(protectedOptions);
        expect(contractNames(protectedPolicy.typedContracts()) == intrinsicIds,
               "connector exclusions must never remove L0 tools");

        expect(!DsConnector::parseConnectorOptions(
                   {QStringLiteral("--include-tool"), QStringLiteral("regex:notes.*")}, options,
                   error) &&
                   !error.isEmpty(),
               "unsupported selector syntax must fail startup parsing");

        DsConnector::ExposurePolicy l0(DsConnector::ConnectorOptions{
            .exposure = {.controlLevel = AutomationWire::ExposureLevel::L0},
        });
        DsConnector::ExposurePolicy l1(DsConnector::ConnectorOptions{
            .exposure = {.controlLevel = AutomationWire::ExposureLevel::L1},
        });
        DsConnector::ExposurePolicy l2(DsConnector::ConnectorOptions{
            .exposure = {.controlLevel = AutomationWire::ExposureLevel::L2},
        });
        DsConnector::ExposurePolicy l3(DsConnector::ConnectorOptions{
            .exposure = {.controlLevel = AutomationWire::ExposureLevel::L3},
        });
        const auto l0Ids = contractNames(l0.typedContracts());
        const auto l1Ids = contractNames(l1.typedContracts());
        const auto l2Ids = contractNames(l2.typedContracts());
        const auto l3Ids = contractNames(l3.typedContracts());
        const auto includesAll = [](const QSet<QString> &superset, const QSet<QString> &subset) {
            return std::all_of(subset.cbegin(), subset.cend(),
                               [&superset](const QString &id) { return superset.contains(id); });
        };
        const auto declaredIds = AutomationWire::publicToolIds();
        expect(l0Ids == intrinsicIds && includesAll(l1Ids, l0Ids) && includesAll(l2Ids, l1Ids) &&
                   includesAll(l3Ids, l2Ids) &&
                   l3Ids == QSet<QString>(declaredIds.cbegin(), declaredIds.cend()),
               "connector control levels must be intrinsic, cumulative, and complete");
    }

    void TestConnector::offlineDownstream() {
        DsConnector::ConnectorRuntime runtime(
            DsConnector::ConnectorOptions{
                .exposure = {.controlLevel = AutomationWire::ExposureLevel::L0},
            },
            QStringLiteral("DsConnectorLite-No-Such-Editor"));
        DsConnector::DownstreamMcpServer server(&runtime);
        QQueue<QByteArray> responses;
        QObject::connect(&server, &DsConnector::DownstreamMcpServer::responseLine, &server,
                         [&responses](const QByteArray &line) { responses.enqueue(line); });

        const auto bridgeTools = DsConnector::ConnectorRuntime::bridgeToolDefinitions();
        const auto &bridgeNames = DsConnector::ConnectorRuntime::bridgeToolNames();
        const QSet<QString> bridgeNameSet(bridgeNames.cbegin(), bridgeNames.cend());
        expect(bridgeNameSet.size() == bridgeNames.size() &&
                   bridgeTools.size() == bridgeNames.size() &&
                   toolNames(bridgeTools) == bridgeNameSet,
               "connector bridge names and definitions must be unique and match");
        for (const auto &entry : bridgeTools) {
            const auto tool = entry.toObject();
            const auto metadata = toolMetadata(tool.value(QStringLiteral("_meta")).toObject());
            expect(AutomationWire::checkJsonSchema(tool.value(QStringLiteral("inputSchema")))
                           .valid() &&
                       AutomationWire::checkJsonSchema(tool.value(QStringLiteral("outputSchema")))
                           .valid(),
                   "every bridge tool must publish valid input and output schemas");
            expect(metadata &&
                       metadata->value(QStringLiteral("minimum_toolset_version")).toInteger() == 1,
                   "every connector tool must publish its minimum toolset version in "
                   "namespaced _meta");
            const auto annotations = tool.value(QStringLiteral("annotations")).toObject();
            expect(!annotations.contains(QStringLiteral("toolsetVersion")) &&
                       !annotations.contains(QStringLiteral("minimumToolsetVersion")),
                   "DS tool metadata must not occupy standard MCP safety annotations");
        }
        const auto statusTool =
            DsConnector::ConnectorRuntime::findBridgeTool(QStringLiteral("connector.get_status"));
        expect(statusTool &&
                   AutomationWire::validateJsonValue(
                       runtime.status(), statusTool->value(QStringLiteral("outputSchema")))
                       .valid(),
               "offline connector status must satisfy its fixed output schema");
        const auto offlineStatus = runtime.status();
        expect(offlineStatus.value(QStringLiteral("connector"))
                       .toObject()
                       .contains(QStringLiteral("instance_id")) &&
                   !offlineStatus.value(QStringLiteral("connector"))
                        .toObject()
                        .contains(QStringLiteral("instanceId")) &&
                   offlineStatus.value(QStringLiteral("mcp"))
                       .toObject()
                       .contains(QStringLiteral("pending_request_count")),
               "all connector status structured fields must use snake_case");

        const AutomationWire::Mcp::RequestContext context{
            .clientCapabilities = QJsonObject{{QStringLiteral("tools"), QJsonObject{}}},
            .clientInfo =
                AutomationWire::Mcp::ImplementationInfo{
                                              .name = QStringLiteral("connector-test"),
                                              .version = QStringLiteral("1"),
                                              },
        };
        {
            DsConnector::DownstreamMcpServer legacyServer(&runtime);
            QObject::connect(&legacyServer, &DsConnector::DownstreamMcpServer::responseLine,
                             &legacyServer,
                             [&responses](const QByteArray &line) { responses.enqueue(line); });
            const AutomationWire::Mcp::RequestContext legacyContext{
                .protocolVersion = QString::fromLatin1(AutomationWire::Mcp::LegacyProtocolVersion),
                .clientCapabilities = QJsonObject{                                                },
                .clientInfo =
                    AutomationWire::Mcp::ImplementationInfo{
                                                  .name = QStringLiteral("codex-compatible-client"),
                                                  .version = QStringLiteral("1"),
                                                  },
            };
            legacyServer.processLine(
                QJsonDocument(AutomationWire::Mcp::makeInitializeRequest(
                                  legacyContext, QStringLiteral("legacy-initialize")))
                    .toJson(QJsonDocument::Compact));
            expect(responses.size() == 1, "MCP 2025-11-25 initialize must receive a response");
            if (!responses.isEmpty()) {
                const auto result = QJsonDocument::fromJson(responses.dequeue())
                                        .object()
                                        .value(QStringLiteral("result"))
                                        .toObject();
                expect(result.value(QStringLiteral("protocolVersion")).toString() ==
                               QString::fromLatin1(AutomationWire::Mcp::LegacyProtocolVersion) &&
                           result.value(QStringLiteral("capabilities"))
                               .toObject()
                               .value(QStringLiteral("tools"))
                               .isObject() &&
                           !result.contains(QStringLiteral("resultType")),
                       "legacy initialize must negotiate tools without modern result fields");
            }
            legacyServer.processLine(
                QJsonDocument(AutomationWire::Mcp::makeRequest(
                                  QString::fromLatin1(AutomationWire::Mcp::PingMethod), {},
                                  legacyContext, QStringLiteral("legacy-ping")))
                    .toJson(QJsonDocument::Compact));
            expect(!responses.isEmpty() && QJsonDocument::fromJson(responses.dequeue())
                                               .object()
                                               .value(QStringLiteral("result"))
                                               .isObject(),
                   "legacy ping must work between initialize and initialized");
            legacyServer.processLine(
                QJsonDocument(AutomationWire::Mcp::makeRequest(
                                  QString::fromLatin1(AutomationWire::Mcp::InitializedNotification),
                                  {}, legacyContext, QStringLiteral("invalid-initialized-request")))
                    .toJson(QJsonDocument::Compact));
            expect(!responses.isEmpty() && QJsonDocument::fromJson(responses.dequeue())
                                                   .object()
                                                   .value(QStringLiteral("error"))
                                                   .toObject()
                                                   .value(QStringLiteral("code"))
                                                   .toInt() == AutomationWire::Mcp::InvalidRequest,
                   "notifications/initialized with an id must be rejected as a request");
            legacyServer.processLine(
                QJsonDocument(AutomationWire::Mcp::makeRequest(
                                  QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod), {},
                                  legacyContext, QStringLiteral("legacy-before-initialized")))
                    .toJson(QJsonDocument::Compact));
            expect(!responses.isEmpty() &&
                       QJsonDocument::fromJson(responses.dequeue())
                               .object()
                               .value(QStringLiteral("error"))
                               .toObject()
                               .value(QStringLiteral("code"))
                               .toInt() == AutomationWire::Mcp::ServerNotInitialized,
                   "legacy tools must remain unavailable until notifications/initialized");
            legacyServer.processLine(
                QJsonDocument(AutomationWire::Mcp::makeRequest(
                                  QString::fromLatin1(AutomationWire::Mcp::InitializedNotification),
                                  {}, legacyContext))
                    .toJson(QJsonDocument::Compact));
            expect(responses.isEmpty(),
                   "legacy initialized notification must remain response-free");
            legacyServer.processLine(
                QJsonDocument(AutomationWire::Mcp::makeRequest(
                                  QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod), {},
                                  legacyContext, QStringLiteral("legacy-list")))
                    .toJson(QJsonDocument::Compact));
            if (!responses.isEmpty()) {
                const auto result = QJsonDocument::fromJson(responses.dequeue())
                                        .object()
                                        .value(QStringLiteral("result"))
                                        .toObject();
                const auto tools = result.value(QStringLiteral("tools")).toArray();
                expect(
                    toolNames(tools) == toolNames(runtime.downstreamTools()) &&
                        !result.contains(QStringLiteral("resultType")) &&
                        !result.contains(QStringLiteral("ttlMs")),
                    "legacy tools/list must expose bridges and intrinsic L0 tools using the 2025 "
                    "result shape");
            } else {
                expect(false, "legacy tools/list must respond while the editor is offline");
            }
            legacyServer.processLine(
                QJsonDocument(
                    AutomationWire::Mcp::makeRequest(
                        QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                        QJsonObject{
                            {QStringLiteral("name"),      QStringLiteral("connector.get_status")},
                            {QStringLiteral("arguments"), QJsonObject{}                         },
            },
                        legacyContext, QStringLiteral("legacy-call")))
                    .toJson(QJsonDocument::Compact));
            if (!responses.isEmpty()) {
                const auto result = QJsonDocument::fromJson(responses.dequeue())
                                        .object()
                                        .value(QStringLiteral("result"))
                                        .toObject();
                expect(result.value(QStringLiteral("content")).isArray() &&
                           result.value(QStringLiteral("structuredContent")).isObject() &&
                           !result.contains(QStringLiteral("resultType")),
                       "legacy tools/call must retain structured and text results");
            } else {
                expect(false, "legacy tools/call must return the offline status result");
            }
        }
        {
            DsConnector::DownstreamMcpServer compatibilityServer(&runtime);
            QObject::connect(&compatibilityServer, &DsConnector::DownstreamMcpServer::responseLine,
                             &compatibilityServer,
                             [&responses](const QByteArray &line) { responses.enqueue(line); });
            const AutomationWire::Mcp::RequestContext compatibilityContext{
                .protocolVersion =
                    QString::fromLatin1(AutomationWire::Mcp::CompatibilityProtocolVersion),
                .clientCapabilities = QJsonObject{                                                        },
                .clientInfo =
                    AutomationWire::Mcp::ImplementationInfo{
                                                  .name = QStringLiteral("codex-2025-06-compatible-client"),
                                                  .version = QStringLiteral("1"),
                                                  },
            };
            compatibilityServer.processLine(
                QJsonDocument(AutomationWire::Mcp::makeInitializeRequest(
                                  compatibilityContext, QStringLiteral("compatibility-initialize")))
                    .toJson(QJsonDocument::Compact));
            const auto initializeResult = responses.isEmpty()
                                              ? QJsonObject{}
                                              : QJsonDocument::fromJson(responses.dequeue())
                                                    .object()
                                                    .value(QStringLiteral("result"))
                                                    .toObject();
            expect(initializeResult.value(QStringLiteral("protocolVersion")) ==
                           QString::fromLatin1(AutomationWire::Mcp::CompatibilityProtocolVersion) &&
                       !initializeResult.contains(QStringLiteral("resultType")),
                   "MCP 2025-06-18 initialize must echo the compatibility version");
            compatibilityServer.processLine(
                QJsonDocument(AutomationWire::Mcp::makeRequest(
                                  QString::fromLatin1(AutomationWire::Mcp::InitializedNotification),
                                  {}, compatibilityContext))
                    .toJson(QJsonDocument::Compact));
            compatibilityServer.processLine(
                QJsonDocument(AutomationWire::Mcp::makeRequest(
                                  QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod), {},
                                  compatibilityContext, QStringLiteral("compatibility-list")))
                    .toJson(QJsonDocument::Compact));
            const auto listResult = responses.isEmpty()
                                        ? QJsonObject{}
                                        : QJsonDocument::fromJson(responses.dequeue())
                                              .object()
                                              .value(QStringLiteral("result"))
                                              .toObject();
            const auto tools = listResult.value(QStringLiteral("tools")).toArray();
            expect(toolNames(tools) == toolNames(runtime.downstreamTools()) &&
                       !listResult.contains(QStringLiteral("resultType")),
                   "MCP 2025-06-18 tools/list must expose the legacy-compatible surface");
        }
        server.processLine(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod), {}, context,
                              QStringLiteral("discover")))
                .toJson(QJsonDocument::Compact));
        expect(responses.size() == 1, "server/discover must respond while editor is offline");
        if (!responses.isEmpty()) {
            const auto response = QJsonDocument::fromJson(responses.dequeue()).object();
            const auto result = response.value(QStringLiteral("result")).toObject();
            expect(response.value(QStringLiteral("id")).toString() == QStringLiteral("discover") &&
                       result.value(QStringLiteral("resultType")) == QStringLiteral("complete") &&
                       result.value(QStringLiteral("supportedVersions")).toArray() ==
                           QJsonArray{QString::fromLatin1(AutomationWire::Mcp::ProtocolVersion)},
                   "downstream discovery must use the modern envelope and advertise only 2026");
        }

        server.processLine(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod), {},
                              context, QStringLiteral("list")))
                .toJson(QJsonDocument::Compact));
        expect(responses.size() == 1, "tools/list must respond while editor is offline");
        if (!responses.isEmpty()) {
            const auto response = QJsonDocument::fromJson(responses.dequeue()).object();
            const auto result = response.value(QStringLiteral("result")).toObject();
            const auto tools = result.value(QStringLiteral("tools")).toArray();
            expect(toolNames(tools) == toolNames(runtime.downstreamTools()) &&
                       !result.contains(QStringLiteral("nextCursor")),
                   "l0 downstream list must retain bridges and intrinsic tools without a cursor");
        }

        server.processLine(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod),
                              QJsonObject{
                                  {QStringLiteral("cursor"), QStringLiteral("1")}
        },
                              context, QStringLiteral("forged-list-cursor")))
                .toJson(QJsonDocument::Compact));
        expect(responses.size() == 1 && QJsonDocument::fromJson(responses.dequeue())
                                                .object()
                                                .value(QStringLiteral("error"))
                                                .toObject()
                                                .value(QStringLiteral("code"))
                                                .toInt() == AutomationWire::Mcp::InvalidParams,
               "downstream tools/list must reject unsigned and forged cursors");

        server.processLine(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod),
                              QJsonObject{
                                  {QStringLiteral("limit"), 1}
        },
                              context, QStringLiteral("invalid-list")))
                .toJson(QJsonDocument::Compact));
        expect(responses.size() == 1 && QJsonDocument::fromJson(responses.dequeue())
                                                .object()
                                                .value(QStringLiteral("error"))
                                                .toObject()
                                                .value(QStringLiteral("code"))
                                                .toInt() == AutomationWire::Mcp::InvalidParams,
               "tools/list must reject non-standard limit and additional params");

        server.processLine(
            QJsonDocument(
                AutomationWire::Mcp::makeRequest(
                    QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod), {}, context))
                .toJson(QJsonDocument::Compact));
        expect(responses.isEmpty(), "JSON-RPC core notifications must never produce a response");

        server.processLine(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QStringLiteral("notifications/cancelled"),
                              QJsonObject{
                                  {QStringLiteral("request_id"), QStringLiteral("legacy-id")}
        },
                              context))
                .toJson(QJsonDocument::Compact));
        expect(responses.isEmpty(),
               "invalid and legacy cancellation notifications must remain silent");

        std::optional<QJsonValue> stableStatus;
        for (int read = 0; read < 2; ++read) {
            const auto statusCall = AutomationWire::Mcp::makeRequest(
                QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                QJsonObject{
                    {QStringLiteral("name"),      QStringLiteral("connector.get_status")},
                    {QStringLiteral("arguments"), QJsonObject{}                         }
            },
                context, QStringLiteral("reusable-sync-id"));
            server.processLine(QJsonDocument(statusCall).toJson(QJsonDocument::Compact));
            QCOMPARE(responses.size(), 1);
            const auto response = QJsonDocument::fromJson(responses.dequeue()).object();
            QCOMPARE(response.value(QStringLiteral("id")).toString(),
                     QStringLiteral("reusable-sync-id"));
            QVERIFY(!response.contains(QStringLiteral("error")));
            const auto structured = response.value(QStringLiteral("result"))
                                        .toObject()
                                        .value(QStringLiteral("structuredContent"));
            QVERIFY(structured.isObject());
            if (!stableStatus)
                stableStatus = structured;
            else
                QCOMPARE(structured, *stableStatus);
        }

        DsConnector::ConnectorRuntime l3Runtime(
            DsConnector::ConnectorOptions{
                .exposure = {.controlLevel = AutomationWire::ExposureLevel::L3},
            },
            QStringLiteral("DsConnectorLite-No-Such-Editor-L3"));
        DsConnector::DownstreamMcpServer l3Server(&l3Runtime);
        QObject::connect(&l3Server, &DsConnector::DownstreamMcpServer::responseLine, &l3Server,
                         [&responses](const QByteArray &line) { responses.enqueue(line); });
        l3Server.processLine(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod), {},
                              context, QStringLiteral("l3-list")))
                .toJson(QJsonDocument::Compact));
        if (!responses.isEmpty()) {
            const auto result = QJsonDocument::fromJson(responses.dequeue())
                                    .object()
                                    .value(QStringLiteral("result"))
                                    .toObject();
            const auto tools = result.value(QStringLiteral("tools")).toArray();
            expect(toolNames(tools) == toolNames(l3Runtime.downstreamTools()) &&
                       !result.contains(QStringLiteral("nextCursor")),
                   "the complete L3 surface must fit one downstream tools/list page");
        } else {
            expect(false, "the frozen L3 downstream list must respond");
        }

        DsConnector::DownstreamMcpServer l3LegacyServer(&l3Runtime);
        QObject::connect(&l3LegacyServer, &DsConnector::DownstreamMcpServer::responseLine,
                         &l3LegacyServer,
                         [&responses](const QByteArray &line) { responses.enqueue(line); });
        const AutomationWire::Mcp::RequestContext l3LegacyContext{
            .protocolVersion = QString::fromLatin1(AutomationWire::Mcp::LegacyProtocolVersion),
            .clientCapabilities = QJsonObject{                                         },
            .clientInfo =
                AutomationWire::Mcp::ImplementationInfo{
                                              .name = QStringLiteral("l3-legacy-client"),
                                              .version = QStringLiteral("1"),
                                              },
        };
        l3LegacyServer.processLine(
            QJsonDocument(AutomationWire::Mcp::makeInitializeRequest(
                              l3LegacyContext, QStringLiteral("l3-legacy-initialize")))
                .toJson(QJsonDocument::Compact));
        if (!responses.isEmpty())
            responses.dequeue();
        l3LegacyServer.processLine(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::InitializedNotification), {},
                              l3LegacyContext))
                .toJson(QJsonDocument::Compact));
        l3LegacyServer.processLine(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod), {},
                              l3LegacyContext, QStringLiteral("l3-legacy-list")))
                .toJson(QJsonDocument::Compact));
        if (!responses.isEmpty()) {
            const auto result = QJsonDocument::fromJson(responses.dequeue())
                                    .object()
                                    .value(QStringLiteral("result"))
                                    .toObject();
            const auto tools = result.value(QStringLiteral("tools")).toArray();
            expect(toolNames(tools) == toolNames(l3Runtime.downstreamTools()) &&
                       !result.contains(QStringLiteral("nextCursor")) &&
                       !result.contains(QStringLiteral("resultType")),
                   "the first legacy L3 tools/list must expose the complete surface without a "
                   "cursor");
        } else {
            expect(false, "the first legacy L3 tools/list must respond");
        }
    }

    void TestConnector::offlineBootstrapError() {
        const auto serviceName = uniqueBootstrapServiceName();
        DsConnector::BootstrapWatcher watcher(QStringLiteral("connector-test"), QStringLiteral("1"),
                                              serviceName);
        watcher.start();

        expect(waitUntil([&] {
                   return watcher.observation().error == QStringLiteral("editor_not_running");
               }),
               "a missing editor must be reported as editor_not_running");
        const auto changedToTimeout = waitUntil(
            [&] { return watcher.observation().error == QStringLiteral("bootstrap_timeout"); },
            4500);
        expect(!changedToTimeout &&
                   watcher.observation().error == QStringLiteral("editor_not_running"),
               "a missing editor must not be reclassified as bootstrap_timeout");
        watcher.stop();
    }

    void TestConnector::unavailableEditorStates() {
        const auto serviceName = uniqueBootstrapServiceName();
        FakeBootstrap bootstrap(serviceName);
        expect(bootstrap.listen(), "state fake bootstrap endpoint must listen");
        if (QTest::currentTestFailed())
            return;

        DsConnector::ConnectorRuntime runtime(
            DsConnector::ConnectorOptions{
                .exposure = {.controlLevel = AutomationWire::ExposureLevel::L1},
            },
            serviceName);
        const auto callCode = [&runtime] {
            QString code;
            runtime.callTool(QStringLiteral("application.get_info"), {},
                             [&code](const DsConnector::ToolCallOutcome &outcome) {
                                 code = outcome.result.value(QStringLiteral("structuredContent"))
                                            .toObject()
                                            .value(QStringLiteral("code"))
                                            .toString();
                             });
            return code;
        };
        const auto fixedToolCount = runtime.downstreamTools().size();
        expect(callCode() == QStringLiteral("editor_not_running"),
               "an unavailable editor must report editor_not_running");
        runtime.start();

        const QList<QPair<SingleInstanceAutomationState, QString>> states{
            {SingleInstanceAutomationState::EditorStarting, QStringLiteral("editor_starting")     },
            {SingleInstanceAutomationState::ServerDisabled, QStringLiteral("server_disabled")     },
            {SingleInstanceAutomationState::ServerStarting, QStringLiteral("server_starting")     },
            {SingleInstanceAutomationState::ServerStopping, QStringLiteral("server_stopping")     },
            {SingleInstanceAutomationState::EditorStopping, QStringLiteral("editor_not_connected")},
            {SingleInstanceAutomationState::Error,          QStringLiteral("editor_error")        },
        };
        for (const auto &[state, expectedCode] : states) {
            SingleInstanceAutomationStatus status{
                .state = state,
                .editorInstanceId = QStringLiteral("state-editor"),
                .executablePath = QCoreApplication::applicationFilePath(),
                .applicationVersion = QStringLiteral("test"),
                .buildId = QStringLiteral("fake-build"),
                .hostMode = QStringLiteral("gui"),
                .error = state == SingleInstanceAutomationState::Error
                             ? QStringLiteral("fake editor error")
                             : QString(),
            };
            bootstrap.publish(status);
            expect(waitUntil([&] {
                       return runtime.status()
                                  .value(QStringLiteral("editor"))
                                  .toObject()
                                  .value(QStringLiteral("state"))
                                  .toString() == SingleInstanceProtocol::automationStateName(state);
                   }),
                   "connector must observe each non-ready editor state");
            expect(callCode() == expectedCode,
                   "typed calls must return the exact non-ready editor state code");
            expect(runtime.downstreamTools().size() == fixedToolCount,
                   "non-ready editor states must preserve the fixed typed tool surface");
        }
        runtime.stop();
    }

    void TestConnector::bootstrapCorrelation() {
        const auto serviceName = uniqueBootstrapServiceName();
        FakeBootstrap bootstrap(serviceName);
        expect(bootstrap.listen(), "correlation fake bootstrap must listen");
        if (QTest::currentTestFailed())
            return;
        bootstrap.emptyFirstRequestId = true;
        bootstrap.publish(SingleInstanceAutomationStatus{
            .state = SingleInstanceAutomationState::EditorStarting,
            .editorInstanceId = QStringLiteral("correlation-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
        });
        DsConnector::ConnectorRuntime runtime({}, serviceName);
        runtime.start();
        expect(waitUntil(
                   [&] {
                       return runtime.status()
                           .value(QStringLiteral("bootstrap"))
                           .toObject()
                           .value(QStringLiteral("error"))
                           .toString()
                           .contains(QStringLiteral("bootstrap_request_id_mismatch"));
                   },
                   5000),
               "the first watch snapshot must carry the exact request correlation id");
        runtime.stop();
    }

    void TestConnector::handshakeCoordination() {
        const auto options = DsConnector::ConnectorOptions{
            .exposure =
                {
                           .controlLevel = AutomationWire::ExposureLevel::L0,
                           .includes = {QStringLiteral("id:application.get_info"),
                                 QStringLiteral("id:application.get_status"),
                                 QStringLiteral("id:notes.list")},
                           },
            .upstreamTimeoutMs = 2000,
        };
        const auto readyStatus = [](const QString &editorId, const QString &endpoint) {
            return SingleInstanceAutomationStatus{
                .state = SingleInstanceAutomationState::ServerReady,
                .editorInstanceId = editorId,
                .executablePath = QCoreApplication::applicationFilePath(),
                .applicationVersion = QStringLiteral("test"),
                .buildId = QStringLiteral("fake-build"),
                .hostMode = QStringLiteral("gui"),
                .serverEnabled = true,
                .serverEndpoint = endpoint,
            };
        };
        const auto readyRuntime = [](const DsConnector::ConnectorRuntime &runtime,
                                     const int targetCount) {
            const auto status = runtime.status();
            const auto compatibility = status.value(QStringLiteral("toolset"))
                                           .toObject()
                                           .value(QStringLiteral("compatibility"))
                                           .toString();
            return status.value(QStringLiteral("mcp"))
                       .toObject()
                       .value(QStringLiteral("connected"))
                       .toBool() &&
                   compatibility == QStringLiteral("compatible") &&
                   status.value(QStringLiteral("exposure"))
                           .toObject()
                           .value(QStringLiteral("generic_target_count"))
                           .toInt() == targetCount;
        };

        {
            FakeHttpEditor http;
            expect(http.listen(), "handshake-coalescing fake editor must listen");
            if (QTest::currentTestFailed())
                return;
            http.discoverResponseDelayMs = 300;
            const auto serviceName = uniqueBootstrapServiceName();
            FakeBootstrap bootstrap(serviceName);
            expect(bootstrap.listen(), "handshake-coalescing bootstrap must listen");
            if (QTest::currentTestFailed())
                return;
            const auto ready = readyStatus(QStringLiteral("coalescing-editor"), http.endpoint());
            bootstrap.publish(ready);
            DsConnector::ConnectorRuntime runtime(options, serviceName);
            runtime.start();
            expect(
                waitUntil([&] { return bootstrap.watcherCount() == 1 && http.discoverCount == 1; },
                          5000),
                "the initial delayed handshake must be in flight before the ready burst");
            for (auto index = 0; index < 64; ++index)
                bootstrap.publish(ready);
            expect(waitUntil([&] { return http.discoverCount >= 2; }, 10000),
                   "duplicate ready snapshots must start one trailing refresh");
            http.exposeNotes = true;
            bootstrap.publish(ready);
            expect(waitUntil([&] { return http.discoverCount >= 3 && readyRuntime(runtime, 5); },
                             10000),
                   "a snapshot received during the trailing refresh must also be applied");
            const auto unexpectedExtraHandshake =
                waitUntil([&] { return http.discoverCount > 3; }, 700);
            expect(!unexpectedExtraHandshake && http.discoverCount == 3 &&
                       http.toolsListCount == 3 && http.statusCallCount == 3 &&
                       http.requestIds.size() == 9 && http.requestIds.size() < 20,
                   "a same-target ready burst must stay below the default client request budget");
            runtime.stop();
        }

        {
            FakeHttpEditor http;
            expect(http.listen(), "handshake-retry fake editor must listen");
            if (QTest::currentTestFailed())
                return;
            http.discoverRateLimitFailuresRemaining = 1;
            const auto serviceName = uniqueBootstrapServiceName();
            FakeBootstrap bootstrap(serviceName);
            expect(bootstrap.listen(), "handshake-retry bootstrap must listen");
            if (QTest::currentTestFailed())
                return;
            const auto ready = readyStatus(QStringLiteral("retry-editor"), http.endpoint());
            bootstrap.publish(ready);
            DsConnector::ConnectorRuntime runtime(options, serviceName);
            runtime.start();
            expect(waitUntil([&] { return http.discoverCount == 2 && readyRuntime(runtime, 4); },
                             10000),
                   "a first HTTP 429 handshake response must recover automatically");

            http.discoverRateLimitFailuresRemaining = 5;
            bootstrap.publish(ready);
            expect(waitUntil(
                       [&] {
                           const auto status = runtime.status();
                           return http.discoverCount == 7 &&
                                  status.value(QStringLiteral("mcp"))
                                          .toObject()
                                          .value(QStringLiteral("error"))
                                          .toString() == QStringLiteral("too_many_requests") &&
                                  status.value(QStringLiteral("toolset"))
                                          .toObject()
                                          .value(QStringLiteral("compatibility"))
                                          .toString() == QStringLiteral("not_loaded");
                       },
                       10000),
                   "transient handshake retries must stop after the bounded backoff budget");
            expect(!waitUntil([&] { return http.discoverCount > 7; }, 1000),
                   "an exhausted handshake must not retry forever without a new event");

            http.discoverRateLimitFailuresRemaining = 1;
            bootstrap.publish(ready);
            expect(waitUntil([&] { return http.discoverCount == 9 && readyRuntime(runtime, 4); },
                             10000),
                   "a later external ready event must receive a fresh bounded retry budget");
            runtime.stop();
        }

        {
            FakeHttpEditor http;
            http.legacyOnly = true;
            http.legacySessionId = QByteArrayLiteral("fixture-session-id");
            http.negotiatedLegacyProtocolVersion =
                QString::fromLatin1(AutomationWire::Mcp::CompatibilityProtocolVersion);
            expect(http.listen(), "legacy-only fake editor must listen");
            if (QTest::currentTestFailed())
                return;
            const auto serviceName = uniqueBootstrapServiceName();
            FakeBootstrap bootstrap(serviceName);
            expect(bootstrap.listen(), "legacy-only bootstrap must listen");
            if (QTest::currentTestFailed())
                return;
            bootstrap.publish(readyStatus(QStringLiteral("legacy-editor"), http.endpoint()));
            DsConnector::ConnectorRuntime runtime(options, serviceName);
            runtime.start();
            expect(waitUntil(
                       [&] {
                           const auto status = runtime.status();
                           return readyRuntime(runtime, 4) &&
                                  status.value(QStringLiteral("mcp"))
                                          .toObject()
                                          .value(QStringLiteral("protocol_version")) ==
                                      QString::fromLatin1(
                                          AutomationWire::Mcp::CompatibilityProtocolVersion);
                       },
                       10000),
                   "a 2025-06-18 editor must connect after the preferred 2026 probe fails");
            expect(http.discoverCount == 1 && http.initializeCount == 1 &&
                       http.initializedNotificationCount == 1 && http.toolsListCount == 1 &&
                       http.statusCallCount == 1 && http.headersValid,
                   "legacy fallback must preserve the session while adopting negotiated "
                   "2025-06-18 transport metadata");

            bool callFinished = false;
            DsConnector::ToolCallOutcome callOutcome;
            runtime.callTool(QStringLiteral("application.get_info"), {},
                             [&](DsConnector::ToolCallOutcome outcome) {
                                 callOutcome = std::move(outcome);
                                 callFinished = true;
                             });
            expect(waitUntil([&] { return callFinished; }, 5000) && !callOutcome.protocolError &&
                       callOutcome.result.value(QStringLiteral("structuredContent"))
                               .toObject()
                               .value(QStringLiteral("name")) == QStringLiteral("DS Editor Lite"),
                   "typed calls must continue through the negotiated 2025 upstream");

            http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Hold;
            auto applicationCallCount =
                http.calledTools.count(QStringLiteral("application.get_info"));
            bool cancellationFinished = false;
            const auto cancellationToken =
                runtime.callTool(QStringLiteral("application.get_info"), {},
                                 [&cancellationFinished](const DsConnector::ToolCallOutcome &) {
                                     cancellationFinished = true;
                                 });
            expect(cancellationToken > 0 && waitUntil(
                                                [&] {
                                                    return http.calledTools.count(QStringLiteral(
                                                               "application.get_info")) >
                                                           applicationCallCount;
                                                },
                                                250),
                   "the legacy cancellation fixture request must reach the editor");
            expect(runtime.cancel(cancellationToken, QStringLiteral("fixture_cancelled")) &&
                       waitUntil([&] { return http.cancelledNotificationCount == 1; }, 500),
                   "a cancelled 2025 request must forward notifications/cancelled");
            const auto cancellationParams = http.cancelledNotificationParams.value(0);
            expect(
                cancellationParams.value(QStringLiteral("requestId")) ==
                        QStringLiteral("%1:%2").arg(runtime.instanceId()).arg(cancellationToken) &&
                    cancellationParams.value(QStringLiteral("reason")) ==
                        QStringLiteral("fixture_cancelled") &&
                    waitUntil([&] { return cancellationFinished; }, 250),
                "legacy cancellation must preserve the upstream id and reason");
            expect(waitUntil(
                       [&] {
                           return runtime.status()
                                      .value(QStringLiteral("mcp"))
                                      .toObject()
                                      .value(QStringLiteral("pending_request_count"))
                                      .toInt() == 0;
                       },
                       250),
                   "the forwarded legacy cancellation notification must settle promptly");

            const auto initializationsBeforeExpiry = http.initializeCount;
            http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::TransportError;
            http.applicationTransportStatus = 404;
            http.applicationTransportCode = QStringLiteral("session_expired");
            http.applicationTransportMessage = QStringLiteral("The session has expired");
            bool expiryDelivered = false;
            DsConnector::ToolCallOutcome expiredOutcome;
            runtime.callTool(QStringLiteral("application.get_info"), {},
                             [&](DsConnector::ToolCallOutcome outcome) {
                                 expiredOutcome = std::move(outcome);
                                 http.legacySessionId = QByteArrayLiteral("replacement-session-id");
                                 expiryDelivered = true;
                             });
            QVERIFY(waitUntil([&] { return expiryDelivered; }, 5000));
            QVERIFY(expiredOutcome.result.value(QStringLiteral("isError")).toBool());
            QVERIFY(waitUntil(
                [&] {
                    return http.initializeCount == initializationsBeforeExpiry + 1 &&
                           runtime.status()
                                   .value(QStringLiteral("toolset"))
                                   .toObject()
                                   .value(QStringLiteral("compatibility"))
                                   .toString() == QStringLiteral("compatible");
                },
                10000));
            callFinished = false;
            runtime.callTool(QStringLiteral("application.get_info"), {},
                             [&](DsConnector::ToolCallOutcome outcome) {
                                 callOutcome = std::move(outcome);
                                 callFinished = true;
                             });
            QVERIFY(waitUntil([&] { return callFinished; }, 5000));
            QVERIFY(!callOutcome.result.value(QStringLiteral("isError")).toBool());
            QCOMPARE(callOutcome.result.value(QStringLiteral("structuredContent"))
                         .toObject()
                         .value(QStringLiteral("name"))
                         .toString(),
                     QStringLiteral("DS Editor Lite"));
            QVERIFY(http.headersValid);

            http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Hold;
            applicationCallCount = http.calledTools.count(QStringLiteral("application.get_info"));
            runtime.callTool(QStringLiteral("application.get_info"), {},
                             [](const DsConnector::ToolCallOutcome &) {});
            expect(waitUntil(
                       [&] {
                           return http.calledTools.count(QStringLiteral("application.get_info")) >
                                  applicationCallCount;
                       },
                       250),
                   "the teardown fixture request must reach the editor");
            const auto cancellationCountBeforeStop = http.cancelledNotificationCount;
            runtime.stop();
            expect(waitUntil([&] { return http.connectionCount() == 0; }, 250) &&
                       http.cancelledNotificationCount == cancellationCountBeforeStop,
                   "connector teardown must abort requests without emitting cancellation "
                   "notifications");
        }
    }

    void TestConnector::connectedIdentity() {
        ConnectedEditorFixture fixture;
        QVERIFY(fixture.start());
        auto &http = fixture.http;
        auto &bootstrap = fixture.bootstrap;
        auto &runtime = fixture.runtime;
        auto &server = fixture.server;
        auto &responses = fixture.responses;
        const auto &context = fixture.context;
        const auto &ready = fixture.ready;
        const auto connectedStatus = runtime.status();
        expect(bootstrap.validWatchRequest && http.headersValid,
               "bootstrap watch identity and modern HTTP MCP headers must be valid");
        expect(connectedStatus.value(QStringLiteral("editor"))
                       .toObject()
                       .value(QStringLiteral("editor_instance_id"))
                       .toString() == ready.editorInstanceId,
               "connector status must report the actual editor identity");
        const auto toolsetCompatibility = connectedStatus.value(QStringLiteral("toolset"))
                                              .toObject()
                                              .value(QStringLiteral("compatibility"))
                                              .toString();
        if (toolsetCompatibility != QStringLiteral("compatible"))
            QTextStream(stderr) << "Observed compatibility: " << toolsetCompatibility << Qt::endl;
        expect(toolsetCompatibility == QStringLiteral("compatible"),
               "connector status must report version-compatible contracts");
    }

    void TestConnector::forwardedCalls_data() {
        QTest::addColumn<QString>("tool");
        QTest::addColumn<QJsonObject>("arguments");
        QTest::newRow("typed-query") << QStringLiteral("application.get_info") << QJsonObject{};
        QTest::newRow("intrinsic-lifecycle") << QStringLiteral("application.request_restart")
                                             << QJsonObject{
                                                    {QStringLiteral("discard_changes"), true}
        };
        QTest::newRow("generic-query")
            << QStringLiteral("editor.tools.invoke")
            << QJsonObject{
                   {QStringLiteral("name"),      QStringLiteral("application.get_info")},
                   {QStringLiteral("arguments"), QJsonObject{}                         }
        };
    }

    void TestConnector::forwardedCalls() {
        QFETCH(QString, tool);
        QFETCH(QJsonObject, arguments);
        ConnectedEditorFixture fixture;
        QVERIFY(fixture.start());
        const QString id = QStringLiteral("downstream-request");
        fixture.sendTool(id, tool, arguments);
        const auto response = fixture.response(id);
        QVERIFY(response);
        QCOMPARE(response->value(QStringLiteral("id")).toString(), id);
        const auto result = response->value(QStringLiteral("result")).toObject();
        QVERIFY(!result.value(QStringLiteral("isError")).toBool());
        const auto content = result.value(QStringLiteral("structuredContent")).toObject();
        if (tool == QStringLiteral("application.request_restart")) {
            QVERIFY(content.value(QStringLiteral("accepted")).toBool());
            QCOMPARE(content.value(QStringLiteral("action")).toString(), QStringLiteral("restart"));
            QVERIFY(content.value(QStringLiteral("discard_changes")).toBool());
            QVERIFY(fixture.http.calledTools.contains(tool));
        } else {
            QCOMPARE(content.value(QStringLiteral("name")).toString(),
                     QStringLiteral("DS Editor Lite"));
        }
        QVERIFY(std::none_of(
            fixture.http.requestIds.cbegin(), fixture.http.requestIds.cend(),
            [&](const QJsonValue &upstreamId) { return upstreamId.toString() == id; }));
    }

    void TestConnector::genericDiscovery() {
        ConnectedEditorFixture fixture;
        QVERIFY(fixture.start());
        auto &http = fixture.http;
        auto &bootstrap = fixture.bootstrap;
        auto &runtime = fixture.runtime;
        auto &server = fixture.server;
        auto &responses = fixture.responses;
        const auto &context = fixture.context;
        const auto &ready = fixture.ready;
        server.processLine(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                              QJsonObject{
                                  {QStringLiteral("name"),      QStringLiteral("editor.tools.list")},
                                  {QStringLiteral("arguments"), QJsonObject{}                      },
        },
                              context, QStringLiteral("generic-list")))
                .toJson(QJsonDocument::Compact));
        expect(!responses.isEmpty(), "generic list must complete synchronously");
        if (!responses.isEmpty()) {
            const auto tools = QJsonDocument::fromJson(responses.dequeue())
                                   .object()
                                   .value(QStringLiteral("result"))
                                   .toObject()
                                   .value(QStringLiteral("structuredContent"))
                                   .toObject()
                                   .value(QStringLiteral("tools"))
                                   .toArray();
            auto expectedNames = contractNames(runtime.exposurePolicy().typedContracts());
            expectedNames.intersect(http.advertisedToolNames());
            bool containsFiltered = false;
            bool summariesValid = true;
            for (const auto &entry : tools) {
                const auto descriptor = entry.toObject();
                containsFiltered |= descriptor.value(QStringLiteral("name")).toString() ==
                                    QStringLiteral("documents.get");
                summariesValid &= isToolSummary(descriptor);
            }
            expect(toolNames(tools) == expectedNames && !containsFiltered && summariesValid,
                   "generic list must preserve exposure and return compact versioned summaries");
        }

        server.processLine(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                              QJsonObject{
                                  {QStringLiteral("name"),      QStringLiteral("editor.tools.search")},
                                  {QStringLiteral("arguments"),
                                   QJsonObject{{QStringLiteral("query"),
                                                QStringLiteral("application.get_info")}}             },
        },
                              context, QStringLiteral("versioned-search")))
                .toJson(QJsonDocument::Compact));
        if (!responses.isEmpty()) {
            const auto tools = QJsonDocument::fromJson(responses.dequeue())
                                   .object()
                                   .value(QStringLiteral("result"))
                                   .toObject()
                                   .value(QStringLiteral("structuredContent"))
                                   .toObject()
                                   .value(QStringLiteral("tools"))
                                   .toArray();
            expect(tools.size() == 1 && isToolSummary(tools.first().toObject()),
                   "generic search must return a compact versioned summary");
        } else {
            expect(false, "versioned generic search must return a result");
        }

        server.processLine(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                              QJsonObject{
                                  {QStringLiteral("name"),      QStringLiteral("editor.tools.describe")},
                                  {QStringLiteral("arguments"),
                                   QJsonObject{{QStringLiteral("name"),
                                                QStringLiteral("application.get_info")}}               },
        },
                              context, QStringLiteral("versioned-describe")))
                .toJson(QJsonDocument::Compact));
        if (!responses.isEmpty()) {
            const auto structured = QJsonDocument::fromJson(responses.dequeue())
                                        .object()
                                        .value(QStringLiteral("result"))
                                        .toObject()
                                        .value(QStringLiteral("structuredContent"))
                                        .toObject();
            expect(structured.value(QStringLiteral("toolset_version")).toInteger() >= 1 &&
                       structured.value(QStringLiteral("typed_compatibility")) ==
                           QStringLiteral("compatible") &&
                       hasToolMetadata(structured.value(QStringLiteral("tool")).toObject()),
                   "generic describe must return the complete actual descriptor and compatibility");
        } else {
            expect(false, "versioned generic describe must return a result");
        }
    }

    void TestConnector::genericExposure_data() {
        QTest::addColumn<QString>("action");
        QTest::newRow("search") << QStringLiteral("search");
        QTest::newRow("describe") << QStringLiteral("describe");
        QTest::newRow("invoke") << QStringLiteral("invoke");
    }

    void TestConnector::genericExposure() {
        QFETCH(QString, action);
        ConnectedEditorFixture fixture;
        QVERIFY(fixture.start());
        const QString filtered = QStringLiteral("documents.get");
        QJsonObject arguments{
            {action == QStringLiteral("search") ? QStringLiteral("query") : QStringLiteral("name"),
             filtered}
        };
        if (action == QStringLiteral("invoke"))
            arguments.insert(QStringLiteral("arguments"), QJsonObject{});
        fixture.sendTool(QStringLiteral("filtered"), QStringLiteral("editor.tools.") + action,
                         arguments);
        const auto response = fixture.response(QStringLiteral("filtered"));
        QVERIFY(response);
        const auto content = response->value(QStringLiteral("result"))
                                 .toObject()
                                 .value(QStringLiteral("structuredContent"))
                                 .toObject();
        if (action == QStringLiteral("search"))
            QVERIFY(content.value(QStringLiteral("tools")).toArray().isEmpty());
        else
            QCOMPARE(content.value(QStringLiteral("code")).toString(),
                     QStringLiteral("connector_tool_filtered"));
        QVERIFY(!fixture.http.calledTools.contains(filtered));
    }

    void TestConnector::duplicateRequestAndCancellation() {
        ConnectedEditorFixture fixture;
        QVERIFY(fixture.start());
        auto &http = fixture.http;
        auto &bootstrap = fixture.bootstrap;
        auto &runtime = fixture.runtime;
        auto &server = fixture.server;
        auto &responses = fixture.responses;
        const auto &context = fixture.context;
        const auto &ready = fixture.ready;
        const auto callsBefore = http.calledTools.count(QStringLiteral("application.get_info"));
        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Hold;
        const auto heldRequest = AutomationWire::Mcp::makeRequest(
            QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
            QJsonObject{
                {QStringLiteral("name"),      QStringLiteral("application.get_info")},
                {QStringLiteral("arguments"), QJsonObject{}                         }
        },
            context, QStringLiteral("cancel-me"));
        server.processLine(QJsonDocument(heldRequest).toJson(QJsonDocument::Compact));
        server.processLine(QJsonDocument(heldRequest).toJson(QJsonDocument::Compact));
        expect(!responses.isEmpty() && QJsonDocument::fromJson(responses.dequeue())
                                               .object()
                                               .value(QStringLiteral("error"))
                                               .toObject()
                                               .value(QStringLiteral("code"))
                                               .toInt() == AutomationWire::Mcp::InvalidRequest,
               "duplicate in-flight downstream request IDs must be rejected");
        expect(waitUntil([&] {
                   return http.calledTools.count(QStringLiteral("application.get_info")) >
                          callsBefore;
               }),
               "cancellation test request must reach the fake editor");
        server.processLine(
            QJsonDocument(
                QJsonObject{
                    {QStringLiteral("jsonrpc"), QStringLiteral("2.0")                       },
                    {QStringLiteral("method"),  QStringLiteral("notifications/cancelled")   },
                    {QStringLiteral("params"),
                     QJsonObject{{QStringLiteral("requestId"), QStringLiteral("cancel-me")}}},
        })
                .toJson(QJsonDocument::Compact));
        expect(waitUntil([&] {
                   return runtime.status()
                              .value(QStringLiteral("mcp"))
                              .toObject()
                              .value(QStringLiteral("pending_request_count"))
                              .toInt() == 0;
               }) &&
                   responses.isEmpty() && http.cancelledNotificationCount == 1,
               "modern cancellation must close the upstream request, notify the editor, and "
               "emit no downstream response");
        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Success;

        server.processLine(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QStringLiteral("notifications/cancelled"),
                              QJsonObject{
                                  {QStringLiteral("requestId"), QStringLiteral("cancel-me")}
        },
                              context))
                .toJson(QJsonDocument::Compact));
        expect(responses.isEmpty(),
               "cancelling an already completed request must not emit another response");
    }

    void TestConnector::upstreamResponses_data() {
        using Mode = FakeHttpEditor::ApplicationResponseMode;
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("field");
        QTest::addColumn<QString>("expected");
        QTest::addColumn<int>("protocolError");
        const auto add = [](const char *name, Mode mode, const char *field, const char *expected,
                            int protocolError = 0) {
            QTest::newRow(name) << int(mode) << QString::fromLatin1(field)
                                << QString::fromLatin1(expected) << protocolError;
        };
        add("sse", Mode::Sse, "name", "DS Editor Lite");
        add("duplicate-sse-result", Mode::RepeatedSse, "message",
            "multiple_upstream_sse_responses");
        add("malformed-json", Mode::MalformedJson, "message", "invalid_upstream_json_response");
        add("proxy-html-response", Mode::UnsupportedContentType, "message",
            "unsupported_upstream_content_type");
        add("business-error", Mode::BusinessError, "code", "fake_business_error");
        add("protocol-error", Mode::ProtocolError, "", "", AutomationWire::Mcp::InvalidParams);
        add("editor-owns-output-validation", Mode::InvalidOutput, "leaked_secret", "must-not-pass");
        add("redirect", Mode::Redirect, "code", "upstream_redirect_rejected");
        add("timeout", Mode::Hold, "code", "upstream_timeout");
        add("oversized-response", Mode::Oversized, "message", "upstream_response_too_large");
    }

    void TestConnector::upstreamResponses() {
        QFETCH(int, mode);
        QFETCH(QString, field);
        QFETCH(QString, expected);
        QFETCH(int, protocolError);
        ConnectedEditorFixture fixture;
        QVERIFY(fixture.start());
        fixture.http.applicationResponseMode =
            static_cast<FakeHttpEditor::ApplicationResponseMode>(mode);
        const QString id = QStringLiteral("response-case");
        fixture.sendTool(id, QStringLiteral("application.get_info"));
        const auto response = fixture.response(id, 10000);
        QVERIFY(response);
        QCOMPARE(response->value(QStringLiteral("id")).toString(), id);
        if (protocolError) {
            QCOMPARE(response->value(QStringLiteral("error"))
                         .toObject()
                         .value(QStringLiteral("code"))
                         .toInt(),
                     protocolError);
            return;
        }
        const auto result = response->value(QStringLiteral("result")).toObject();
        const auto content = result.value(QStringLiteral("structuredContent")).toObject();
        QCOMPARE(content.value(field).toString(), expected);
        if (mode == int(FakeHttpEditor::ApplicationResponseMode::BusinessError))
            QVERIFY(result.value(QStringLiteral("isError")).toBool());
        if (mode == int(FakeHttpEditor::ApplicationResponseMode::Sse) ||
            mode == int(FakeHttpEditor::ApplicationResponseMode::InvalidOutput))
            QVERIFY(!result.value(QStringLiteral("isError")).toBool());
        if (mode == int(FakeHttpEditor::ApplicationResponseMode::Hold))
            QCOMPARE(content.value(QStringLiteral("message")).toString(),
                     QStringLiteral("upstream_timeout"));
        if (mode == int(FakeHttpEditor::ApplicationResponseMode::RepeatedSse) ||
            mode == int(FakeHttpEditor::ApplicationResponseMode::MalformedJson) ||
            mode == int(FakeHttpEditor::ApplicationResponseMode::UnsupportedContentType)) {
            QVERIFY(result.value(QStringLiteral("isError")).toBool());
            fixture.sendTool(QStringLiteral("after-invalid-response"),
                             QStringLiteral("application.get_info"));
            const auto recovered =
                fixture.response(QStringLiteral("after-invalid-response"), 10000);
            QVERIFY(recovered);
            const auto recoveredResult = recovered->value(QStringLiteral("result")).toObject();
            QVERIFY(!recoveredResult.value(QStringLiteral("isError")).toBool());
            QCOMPARE(recoveredResult.value(QStringLiteral("structuredContent"))
                         .toObject()
                         .value(QStringLiteral("name"))
                         .toString(),
                     QStringLiteral("DS Editor Lite"));
        }
    }

    void TestConnector::editorPolicyRefresh() {
        ConnectedEditorFixture fixture;
        QVERIFY(fixture.start());
        auto &http = fixture.http;
        auto &bootstrap = fixture.bootstrap;
        auto &runtime = fixture.runtime;
        auto &server = fixture.server;
        auto &responses = fixture.responses;
        const auto &context = fixture.context;
        const auto &ready = fixture.ready;
        const auto fixedToolCount = runtime.downstreamTools().size();
        const auto policyRefreshCount = http.toolsListCount;
        http.applicationAvailability = QStringLiteral("control_level_disabled");
        bootstrap.publish(ready);
        expect(waitUntil(
                   [&] {
                       return http.toolsListCount > policyRefreshCount &&
                              runtime.status()
                                      .value(QStringLiteral("toolset"))
                                      .toObject()
                                      .value(QStringLiteral("compatibility"))
                                      .toString() != QStringLiteral("refreshing");
                   },
                   10000),
               "same-endpoint editor policy refresh must finish");
        const auto callsBeforePolicyReject = http.calledTools.size();
        QJsonArray policyList;
        runtime.callTool(QStringLiteral("editor.tools.list"), {},
                         [&policyList](const DsConnector::ToolCallOutcome &outcome) {
                             policyList = outcome.result.value(QStringLiteral("structuredContent"))
                                              .toObject()
                                              .value(QStringLiteral("tools"))
                                              .toArray();
                         });
        QString forgedCursorCode;
        runtime.callTool(QStringLiteral("editor.tools.list"),
                         QJsonObject{
                             {QStringLiteral("cursor"), QStringLiteral("1")}
        },
                         [&forgedCursorCode](const DsConnector::ToolCallOutcome &outcome) {
                             forgedCursorCode =
                                 outcome.result.value(QStringLiteral("structuredContent"))
                                     .toObject()
                                     .value(QStringLiteral("code"))
                                     .toString();
                         });
        expect(forgedCursorCode == QStringLiteral("invalid_cursor"),
               "generic list must reject forged numeric cursors");
        const auto policyListContainsApplication = std::any_of(
            policyList.constBegin(), policyList.constEnd(), [](const QJsonValue &entry) {
                return entry.toObject().value(QStringLiteral("name")) ==
                       QStringLiteral("application.get_info");
            });
        QString policyDescribeCode;
        QJsonArray policySearch;
        runtime.callTool(QStringLiteral("editor.tools.search"),
                         QJsonObject{
                             {QStringLiteral("query"), QStringLiteral("application.get_info")}
        },
                         [&policySearch](const DsConnector::ToolCallOutcome &outcome) {
                             policySearch =
                                 outcome.result.value(QStringLiteral("structuredContent"))
                                     .toObject()
                                     .value(QStringLiteral("tools"))
                                     .toArray();
                         });
        runtime.callTool(QStringLiteral("editor.tools.describe"),
                         QJsonObject{
                             {QStringLiteral("name"), QStringLiteral("application.get_info")}
        },
                         [&policyDescribeCode](const DsConnector::ToolCallOutcome &outcome) {
                             policyDescribeCode =
                                 outcome.result.value(QStringLiteral("structuredContent"))
                                     .toObject()
                                     .value(QStringLiteral("code"))
                                     .toString();
                         });
        expect(!policyListContainsApplication && policySearch.isEmpty() &&
                   policyDescribeCode == QStringLiteral("control_level_disabled"),
               "generic list, search, and describe must enforce editor availability policy");
        server.processLine(
            QJsonDocument(
                AutomationWire::Mcp::makeRequest(
                    QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                    QJsonObject{
                        {QStringLiteral("name"),      QStringLiteral("editor.tools.invoke")},
                        {QStringLiteral("arguments"),
                         QJsonObject{
                             {QStringLiteral("name"), QStringLiteral("application.get_info")},
                             {QStringLiteral("arguments"), QJsonObject{}},
                         }                                                                 },
        },
                    context, QStringLiteral("policy-filtered-invoke")))
                .toJson(QJsonDocument::Compact));
        if (!responses.isEmpty()) {
            const auto code = QJsonDocument::fromJson(responses.dequeue())
                                  .object()
                                  .value(QStringLiteral("result"))
                                  .toObject()
                                  .value(QStringLiteral("structuredContent"))
                                  .toObject()
                                  .value(QStringLiteral("code"))
                                  .toString();
            expect(code == QStringLiteral("control_level_disabled") &&
                       http.calledTools.size() == callsBeforePolicyReject,
                   "generic invoke must not bypass actual editor availability policy");
        } else {
            expect(false, "editor-policy-rejected generic invoke must return a result");
        }
    }

    void TestConnector::schemaRefreshKeepsVersionCompatibility() {
        ConnectedEditorFixture fixture;
        QVERIFY(fixture.start());
        auto &http = fixture.http;
        auto &bootstrap = fixture.bootstrap;
        auto &runtime = fixture.runtime;
        auto &server = fixture.server;
        auto &responses = fixture.responses;
        const auto &context = fixture.context;
        const auto &ready = fixture.ready;
        const auto fixedToolCount = runtime.downstreamTools().size();
        http.applicationAvailability.clear();
        http.exposeNotes = true;
        http.applicationSchemaVariant = true;
        bootstrap.publish(ready);
        expect(waitUntil(
                   [&] {
                       return runtime.status()
                                      .value(QStringLiteral("exposure"))
                                      .toObject()
                                      .value(QStringLiteral("generic_target_count"))
                                      .toInt() == 5 &&
                              http.toolsListCount >= 2 &&
                              runtime.status()
                                      .value(QStringLiteral("toolset"))
                                      .toObject()
                                      .value(QStringLiteral("compatibility"))
                                      .toString() == QStringLiteral("compatible");
                   },
                   10000),
               "schema drift must not change version-based contract compatibility");
        expect(runtime.downstreamTools().size() == fixedToolCount,
               "same-endpoint refresh must not mutate the fixed typed downstream set");

        const auto typedCallsBefore =
            http.calledTools.count(QStringLiteral("application.get_info"));
        server.processLine(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                              QJsonObject{
                                  {QStringLiteral("name"),      QStringLiteral("application.get_info")},
                                  {QStringLiteral("arguments"), QJsonObject{}                         }
        },
                              context, QStringLiteral("schema-variant-typed")))
                .toJson(QJsonDocument::Compact));
        const auto schemaVariantTyped =
            takeResponseById(responses, QStringLiteral("schema-variant-typed"));
        expect(schemaVariantTyped.has_value(),
               "a version-compatible typed wrapper must reach the editor");
        if (schemaVariantTyped) {
            const auto result = schemaVariantTyped->value(QStringLiteral("result")).toObject();
            const auto structured = result.value(QStringLiteral("structuredContent")).toObject();
            expect(!result.value(QStringLiteral("isError")).toBool() &&
                       structured.value(QStringLiteral("name")) ==
                           QStringLiteral("DS Editor Lite") &&
                       http.calledTools.count(QStringLiteral("application.get_info")) >
                           typedCallsBefore,
                   "schema differences must be treated as implementation bugs, not a "
                   "compatibility gate");
        }

        server.processLine(
            QJsonDocument(
                AutomationWire::Mcp::makeRequest(
                    QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                    QJsonObject{
                        {QStringLiteral("name"),      QStringLiteral("editor.tools.invoke")},
                        {QStringLiteral("arguments"),
                         QJsonObject{
                             {QStringLiteral("name"), QStringLiteral("application.get_info")},
                             {QStringLiteral("arguments"), QJsonObject{}},
                         }                                                                 },
        },
                    context, QStringLiteral("incompatible-generic")))
                .toJson(QJsonDocument::Compact));
        expect(waitUntil([&] { return !responses.isEmpty(); }),
               "generic invoke must remain available across schema drift");
        if (!responses.isEmpty()) {
            const auto result = QJsonDocument::fromJson(responses.dequeue())
                                    .object()
                                    .value(QStringLiteral("result"))
                                    .toObject();
            expect(!result.value(QStringLiteral("isError")).toBool() &&
                       result.value(QStringLiteral("structuredContent"))
                               .toObject()
                               .value(QStringLiteral("name")) == QStringLiteral("DS Editor Lite"),
                   "generic proxying must leave business output validation to the editor");
        }
    }

    void TestConnector::backpressureAndCancellation() {
        ConnectedEditorFixture fixture;
        QVERIFY(fixture.start());
        auto &http = fixture.http;
        auto &bootstrap = fixture.bootstrap;
        auto &runtime = fixture.runtime;
        auto &server = fixture.server;
        auto &responses = fixture.responses;
        const auto &context = fixture.context;
        const auto &ready = fixture.ready;
        const auto sendApplication = [&](const QString &id) {
            fixture.sendTool(id, QStringLiteral("application.get_info"));
        };
        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Hold;
        QStringList saturatedRequestIds;
        for (auto index = 0; index < 32; ++index) {
            const auto id = QStringLiteral("saturated-%1").arg(index);
            saturatedRequestIds.append(id);
            sendApplication(id);
        }
        expect(waitUntil([&] {
                   return runtime.status()
                              .value(QStringLiteral("mcp"))
                              .toObject()
                              .value(QStringLiteral("pending_request_count"))
                              .toInt() == 32;
               }),
               "the connector must admit up to 32 concurrent downstream calls");
        sendApplication(QStringLiteral("saturated-overflow"));
        const auto overflow =
            takeResponseById(responses, QStringLiteral("saturated-overflow"), 1000);
        expect(overflow.has_value() &&
                   overflow->value(QStringLiteral("result"))
                       .toObject()
                       .value(QStringLiteral("isError"))
                       .toBool() &&
                   overflow->value(QStringLiteral("result"))
                           .toObject()
                           .value(QStringLiteral("structuredContent"))
                           .toObject()
                           .value(QStringLiteral("code")) == QStringLiteral("busy") &&
                   runtime.status()
                           .value(QStringLiteral("mcp"))
                           .toObject()
                           .value(QStringLiteral("pending_request_count"))
                           .toInt() == 32,
               "the thirty-third downstream call must be rejected without forwarding or queuing");
        for (const auto &id : std::as_const(saturatedRequestIds)) {
            server.processLine(
                QJsonDocument(
                    QJsonObject{
                        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")                         },
                        {QStringLiteral("method"),  QStringLiteral("notifications/cancelled")     },
                        {QStringLiteral("params"),  QJsonObject{{QStringLiteral("requestId"), id}}},
            })
                    .toJson(QJsonDocument::Compact));
        }
        expect(waitUntil(
                   [&] {
                       return runtime.status()
                                  .value(QStringLiteral("mcp"))
                                  .toObject()
                                  .value(QStringLiteral("pending_request_count"))
                                  .toInt() == 0;
                   },
                   10000),
               "cancelling the saturated downstream calls must release every slot");
        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Success;
    }

    void TestConnector::upstreamDisabledKeepsDownstreamTools() {
        ConnectedEditorFixture fixture;
        QVERIFY(fixture.start());
        auto &http = fixture.http;
        auto &bootstrap = fixture.bootstrap;
        auto &runtime = fixture.runtime;
        auto &server = fixture.server;
        auto &responses = fixture.responses;
        const auto &context = fixture.context;
        const auto &ready = fixture.ready;
        const auto fixedToolCount = runtime.downstreamTools().size();
        auto disabled = ready;
        disabled.state = SingleInstanceAutomationState::ServerDisabled;
        disabled.serverEnabled = false;
        disabled.serverEndpoint.clear();
        bootstrap.publish(disabled);
        expect(waitUntil([&] {
                   return !runtime.status()
                               .value(QStringLiteral("mcp"))
                               .toObject()
                               .value(QStringLiteral("connected"))
                               .toBool();
               }),
               "connector must drop only the upstream MCP state when editor disables MCP");
        expect(runtime.downstreamTools().size() == fixedToolCount,
               "editor state changes must not mutate the fixed downstream tool list");
        runtime.stop();
    }

    void TestConnector::forwardCompatibleGenericTools() {
        FakeHttpEditor http;
        expect(http.listen(), "forward-compatible fake editor must listen");
        if (QTest::currentTestFailed())
            return;
        http.exposeForwardCompatibleTools = true;

        const auto serviceName = uniqueBootstrapServiceName();
        FakeBootstrap bootstrap(serviceName);
        expect(bootstrap.listen(), "forward-compatible bootstrap must listen");
        if (QTest::currentTestFailed())
            return;
        bootstrap.publish(SingleInstanceAutomationStatus{
            .state = SingleInstanceAutomationState::ServerReady,
            .editorInstanceId = QStringLiteral("forward-compatible-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
            .serverEnabled = true,
            .serverEndpoint = http.endpoint(),
        });

        DsConnector::ConnectorRuntime runtime(
            DsConnector::ConnectorOptions{
                .exposure =
                    {
                               .controlLevel = AutomationWire::ExposureLevel::L0,
                               .includes = {QStringLiteral("id:fake.flexible_output"),
                                     QStringLiteral("id:fake.minimal")},
                               },
                .upstreamTimeoutMs = 2000,
        },
            serviceName);
        runtime.start();
        const auto ready = waitUntil(
            [&] {
                const auto status = runtime.status();
                return status.value(QStringLiteral("mcp"))
                           .toObject()
                           .value(QStringLiteral("connected"))
                           .toBool() &&
                       status.value(QStringLiteral("exposure"))
                               .toObject()
                               .value(QStringLiteral("generic_target_count"))
                               .toInt() == 6 &&
                       status.value(QStringLiteral("toolset"))
                               .toObject()
                               .value(QStringLiteral("compatibility"))
                               .toString() != QStringLiteral("refreshing");
            },
            10000);
        if (!ready)
            QTextStream(stderr) << QJsonDocument(runtime.status()).toJson(QJsonDocument::Compact)
                                << Qt::endl;
        expect(ready,
               "optional and additive upstream tool descriptor fields must not fail handshake");

        DsConnector::DownstreamMcpServer server(&runtime);
        QQueue<QByteArray> responses;
        QObject::connect(&server, &DsConnector::DownstreamMcpServer::responseLine, &server,
                         [&responses](const QByteArray &line) { responses.enqueue(line); });
        const auto context = clientContext();
        server.processLine(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                              QJsonObject{
                                  {QStringLiteral("name"),      QStringLiteral("editor.tools.list")},
                                  {QStringLiteral("arguments"), QJsonObject{}                      }
        },
                              context, QStringLiteral("forward-list")))
                .toJson(QJsonDocument::Compact));
        const auto listResponse = takeResponseById(responses, QStringLiteral("forward-list"));
        QJsonObject minimalDescriptor;
        QJsonObject flexibleDescriptor;
        if (listResponse) {
            const auto result = listResponse->value(QStringLiteral("result")).toObject();
            const auto tools = result.value(QStringLiteral("structuredContent"))
                                   .toObject()
                                   .value(QStringLiteral("tools"))
                                   .toArray();
            for (const auto &entry : tools) {
                const auto descriptor = entry.toObject();
                if (descriptor.value(QStringLiteral("name")) == QStringLiteral("fake.minimal"))
                    minimalDescriptor = descriptor;
                if (descriptor.value(QStringLiteral("name")) ==
                    QStringLiteral("fake.flexible_output")) {
                    flexibleDescriptor = descriptor;
                }
            }
            expect(!result.value(QStringLiteral("isError")).toBool() && tools.size() == 6 &&
                       !minimalDescriptor.isEmpty() &&
                       !minimalDescriptor.contains(QStringLiteral("title")) &&
                       !minimalDescriptor.contains(QStringLiteral("description")) &&
                       !minimalDescriptor.contains(QStringLiteral("outputSchema")) &&
                       !minimalDescriptor.contains(QStringLiteral("inputSchema")) &&
                       minimalDescriptor.value(QStringLiteral("category")) ==
                           QStringLiteral("editor") &&
                       minimalDescriptor.value(QStringLiteral("minimum_control_level")) ==
                           QStringLiteral("l3") &&
                       minimalDescriptor.value(QStringLiteral("minimum_toolset_version"))
                               .toInteger() == 1 &&
                       flexibleDescriptor.value(QStringLiteral("category")) ==
                           QStringLiteral("fake") &&
                       flexibleDescriptor.value(QStringLiteral("minimum_toolset_version"))
                               .toInteger() == 1,
                   "generic list must return compact summaries and synthesize safe defaults");
        } else {
            expect(false, "forward-compatible generic list must return a result");
        }

        server.processLine(
            QJsonDocument(
                AutomationWire::Mcp::makeRequest(
                    QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                    QJsonObject{
                        {QStringLiteral("name"),      QStringLiteral("editor.tools.search")},
                        {QStringLiteral("arguments"),
                         QJsonObject{{QStringLiteral("query"), QStringLiteral("flexible")},
                                     {QStringLiteral("category"), QStringLiteral("fake")}} },
        },
                    context, QStringLiteral("forward-search")))
                .toJson(QJsonDocument::Compact));
        const auto searchResponse = takeResponseById(responses, QStringLiteral("forward-search"));
        const auto searchTools = searchResponse ? searchResponse->value(QStringLiteral("result"))
                                                      .toObject()
                                                      .value(QStringLiteral("structuredContent"))
                                                      .toObject()
                                                      .value(QStringLiteral("tools"))
                                                      .toArray()
                                                : QJsonArray{};
        expect(searchTools.size() == 1 && searchTools.first().toObject().value(QStringLiteral(
                                              "name")) == QStringLiteral("fake.flexible_output"),
               "generic search category filtering must use synthesized tool metadata");

        server.processLine(
            QJsonDocument(
                AutomationWire::Mcp::makeRequest(
                    QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                    QJsonObject{
                        {QStringLiteral("name"),      QStringLiteral("editor.tools.describe") },
                        {QStringLiteral("arguments"),
                         QJsonObject{{QStringLiteral("name"), QStringLiteral("fake.minimal")}}},
        },
                    context, QStringLiteral("forward-describe")))
                .toJson(QJsonDocument::Compact));
        const auto describeResponse =
            takeResponseById(responses, QStringLiteral("forward-describe"));
        if (describeResponse) {
            const auto result = describeResponse->value(QStringLiteral("result")).toObject();
            const auto structured = result.value(QStringLiteral("structuredContent")).toObject();
            expect(
                !result.value(QStringLiteral("isError")).toBool() &&
                    structured.value(QStringLiteral("toolset_version")).toInteger() == 1 &&
                    structured.value(QStringLiteral("typed_compatibility")) ==
                        QStringLiteral("generic_only") &&
                    structured.value(QStringLiteral("tool"))
                        .toObject()
                        .value(QStringLiteral("icons"))
                        .isArray() &&
                    !structured.value(QStringLiteral("tool"))
                         .toObject()
                         .contains(QStringLiteral("outputSchema")),
                "generic describe must return the complete descriptor with omitted outputSchema");
        } else {
            expect(false, "forward-compatible generic describe must return a result");
        }

        const auto invoke = [&](const QString &shape, const QString &id) {
            server.processLine(
                QJsonDocument(
                    AutomationWire::Mcp::makeRequest(
                        QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                        QJsonObject{
                            {QStringLiteral("name"),      QStringLiteral("editor.tools.invoke")},
                            {QStringLiteral("arguments"),
                             QJsonObject{
                                 {QStringLiteral("name"), QStringLiteral("fake.flexible_output")},
                                 {QStringLiteral("arguments"),
                                  QJsonObject{{QStringLiteral("shape"), shape}}},
                             }                                                                 },
            },
                        context, id))
                    .toJson(QJsonDocument::Compact));
            return takeResponseById(responses, id, 5000);
        };
        const auto scalar = invoke(QStringLiteral("string"), QStringLiteral("forward-string"));
        const auto array = invoke(QStringLiteral("array"), QStringLiteral("forward-array"));
        const auto null = invoke(QStringLiteral("null"), QStringLiteral("forward-null"));
        const auto invalid = invoke(QStringLiteral("invalid"), QStringLiteral("forward-invalid"));
        const auto structured = [](const std::optional<QJsonObject> &response) {
            return response ? response->value(QStringLiteral("result"))
                                  .toObject()
                                  .value(QStringLiteral("structuredContent"))
                            : QJsonValue(QJsonValue::Undefined);
        };
        expect(scalar &&
                   !scalar->value(QStringLiteral("result"))
                        .toObject()
                        .value(QStringLiteral("isError"))
                        .toBool() &&
                   structured(scalar).isString() && array && structured(array).isArray() && null &&
                   structured(null).isNull(),
               "generic invoke must preserve valid scalar, array, and null structuredContent");
        expect(invalid &&
                   !invalid->value(QStringLiteral("result"))
                        .toObject()
                        .value(QStringLiteral("isError"))
                        .toBool() &&
                   structured(invalid).toObject().value(QStringLiteral("unexpected")) == true,
               "generic proxying must leave output-schema validation to the editor");
        runtime.stop();
    }

    void TestConnector::compatibilityVersions() {
        FakeHttpEditor http;
        expect(http.listen(), "compatibility fake editor must listen");
        if (QTest::currentTestFailed())
            return;
        http.editorToolsetVersion = 2;
        http.applicationMinimumToolsetVersion = 1;
        http.exposeNotes = true;

        const auto serviceName = uniqueBootstrapServiceName();
        FakeBootstrap bootstrap(serviceName);
        expect(bootstrap.listen(), "compatibility fake bootstrap must listen");
        if (QTest::currentTestFailed())
            return;
        SingleInstanceAutomationStatus ready{
            .state = SingleInstanceAutomationState::ServerReady,
            .editorInstanceId = QStringLiteral("compatibility-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
            .serverEnabled = true,
            .serverEndpoint = http.endpoint(),
        };
        bootstrap.publish(ready);

        DsConnector::ConnectorRuntime runtime(
            DsConnector::ConnectorOptions{
                .exposure =
                    {
                               .controlLevel = AutomationWire::ExposureLevel::L0,
                               .includes = {QStringLiteral("id:application.get_info"),
                                     QStringLiteral("id:notes.list")},
                               },
                .upstreamTimeoutMs = 2000,
        },
            serviceName);
        runtime.start();
        const auto compatibility = [&] {
            return runtime.status()
                .value(QStringLiteral("toolset"))
                .toObject()
                .value(QStringLiteral("compatibility"))
                .toString();
        };
        expect(waitUntil([&] { return compatibility() == QStringLiteral("compatible"); }, 10000),
               "a newer editor toolset must remain compatible when every tool allows it");

        auto refreshCount = http.toolsListCount;
        http.applicationMinimumToolsetVersion = 2;
        bootstrap.publish(ready);
        expect(waitUntil(
                   [&] {
                       const auto toolset =
                           runtime.status().value(QStringLiteral("toolset")).toObject();
                       return http.toolsListCount > refreshCount &&
                              compatibility() == QStringLiteral("contract_incompatible") &&
                              toolset.value(QStringLiteral("compatible_count")).toInt() == 4 &&
                              toolset.value(QStringLiteral("incompatible_count")).toInt() == 1;
                   },
                   10000),
               "compatibility aggregation must retain the compatible tool while marking the "
               "newer per-tool contract incompatible");
        QJsonObject applicationDescription;
        QJsonObject notesDescription;
        runtime.callTool(
            QStringLiteral("editor.tools.describe"),
            QJsonObject{
                {QStringLiteral("name"), QStringLiteral("application.get_info")}
        },
            [&applicationDescription](const DsConnector::ToolCallOutcome &outcome) {
                applicationDescription =
                    outcome.result.value(QStringLiteral("structuredContent")).toObject();
            });
        runtime.callTool(
            QStringLiteral("editor.tools.describe"),
            QJsonObject{
                {QStringLiteral("name"), QStringLiteral("notes.list")}
        },
            [&notesDescription](const DsConnector::ToolCallOutcome &outcome) {
                notesDescription =
                    outcome.result.value(QStringLiteral("structuredContent")).toObject();
            });
        const auto applicationMetadata =
            toolMetadata(applicationDescription.value(QStringLiteral("tool"))
                             .toObject()
                             .value(QStringLiteral("_meta"))
                             .toObject());
        const auto notesMetadata = toolMetadata(notesDescription.value(QStringLiteral("tool"))
                                                    .toObject()
                                                    .value(QStringLiteral("_meta"))
                                                    .toObject());
        expect(applicationDescription.value(QStringLiteral("toolset_version")).toInteger() == 2 &&
                   applicationMetadata &&
                   metadataInteger(*applicationMetadata, QStringLiteral("minimum_toolset_version"),
                                   QStringLiteral("minimumToolsetVersion")) == 2 &&
                   applicationDescription.value(QStringLiteral("typed_compatibility")) ==
                       QStringLiteral("contract_incompatible") &&
                   notesDescription.value(QStringLiteral("toolset_version")).toInteger() == 2 &&
                   notesMetadata &&
                   metadataInteger(*notesMetadata, QStringLiteral("minimum_toolset_version"),
                                   QStringLiteral("minimumToolsetVersion")) == 1 &&
                   notesDescription.value(QStringLiteral("typed_compatibility")) ==
                       QStringLiteral("compatible"),
               "global and minimum toolset versions must be evaluated for each tool");
        QString incompatibleCode;
        runtime.callTool(QStringLiteral("application.get_info"), {},
                         [&incompatibleCode](const DsConnector::ToolCallOutcome &outcome) {
                             incompatibleCode =
                                 outcome.result.value(QStringLiteral("structuredContent"))
                                     .toObject()
                                     .value(QStringLiteral("code"))
                                     .toString();
                         });
        expect(incompatibleCode == QStringLiteral("contract_incompatible"),
               "newer incompatible typed tools must fail before forwarding");

        refreshCount = http.toolsListCount;
        http.applicationMinimumToolsetVersion = 1;
        http.editorToolsetVersion = 1;
        bootstrap.publish(ready);
        expect(waitUntil(
                   [&] {
                       return http.toolsListCount > refreshCount &&
                              compatibility() == QStringLiteral("compatible");
                   },
                   10000),
               "a matching L1 editor contract must be fully compatible");

        refreshCount = http.toolsListCount;
        http.editorControlLevel = AutomationWire::ControlLevel::L3;
        bootstrap.publish(ready);
        expect(waitUntil(
                   [&] {
                       const auto toolset =
                           runtime.status().value(QStringLiteral("toolset")).toObject();
                       return http.toolsListCount > refreshCount &&
                              compatibility() == QStringLiteral("compatible") &&
                              toolset.value(QStringLiteral("connector_version")).toInteger() == 1 &&
                              toolset.value(QStringLiteral("editor_version")).toInteger() == 1 &&
                              !toolset.contains(QStringLiteral("connector_digest")) &&
                              !toolset.contains(QStringLiteral("editor_digest"));
                   },
                   10000),
               "editor control-level refresh must preserve version-only compatibility without "
               "digests");
        runtime.stop();
    }

    void TestConnector::headlessHostAvailability() {
        FakeHttpEditor http;
        expect(http.listen(), "headless availability fake editor must listen");
        if (QTest::currentTestFailed())
            return;

        const auto serviceName = uniqueBootstrapServiceName();
        FakeBootstrap bootstrap(serviceName);
        expect(bootstrap.listen(), "headless availability bootstrap must listen");
        if (QTest::currentTestFailed())
            return;
        bootstrap.publish(SingleInstanceAutomationStatus{
            .state = SingleInstanceAutomationState::ServerReady,
            .editorInstanceId = QStringLiteral("headless-availability-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("headless"),
            .serverEnabled = true,
            .serverEndpoint = http.endpoint(),
        });

        DsConnector::ConnectorRuntime runtime(
            DsConnector::ConnectorOptions{
                .exposure =
                    {
                               .controlLevel = AutomationWire::ExposureLevel::L3,
                               .includes =
                            {
                                QStringLiteral("id:notes.list"),
                                QStringLiteral("id:track_panel.get_state"),
                            }, },
                .upstreamTimeoutMs = 2000,
        },
            serviceName);
        runtime.start();
        expect(waitUntil(
                   [&] {
                       return runtime.status()
                                  .value(QStringLiteral("mcp"))
                                  .toObject()
                                  .value(QStringLiteral("connected"))
                                  .toBool() &&
                              runtime.status()
                                      .value(QStringLiteral("toolset"))
                                      .toObject()
                                      .value(QStringLiteral("compatibility")) ==
                                  QStringLiteral("compatible");
                   },
                   10000),
               "headless availability connector must complete its handshake");

        const auto describeCode = [&runtime](const QString &name) {
            QString code;
            runtime.callTool(QStringLiteral("editor.tools.describe"),
                             QJsonObject{
                                 {QStringLiteral("name"), name}
            },
                             [&code](const DsConnector::ToolCallOutcome &outcome) {
                                 code = outcome.result.value(QStringLiteral("structuredContent"))
                                            .toObject()
                                            .value(QStringLiteral("code"))
                                            .toString();
                             });
            return code;
        };
        expect(describeCode(QStringLiteral("notes.list")) == QStringLiteral("tool_unavailable"),
               "a missing both-host tool must remain tool_unavailable in headless mode");
        expect(describeCode(QStringLiteral("track_panel.get_state")) ==
                   QStringLiteral("host_capability_unavailable"),
               "a missing GUI-only tool must report host_capability_unavailable");
        const auto forwardedBeforeHostReject = http.calledTools.size();
        QString typedHostCode;
        runtime.callTool(QStringLiteral("track_panel.get_state"), {},
                         [&typedHostCode](const DsConnector::ToolCallOutcome &outcome) {
                             typedHostCode =
                                 outcome.result.value(QStringLiteral("structuredContent"))
                                     .toObject()
                                     .value(QStringLiteral("code"))
                                     .toString();
                         });
        expect(typedHostCode == QStringLiteral("host_capability_unavailable") &&
                   http.calledTools.size() == forwardedBeforeHostReject,
               "a fixed GUI wrapper must reject the headless host without forwarding");

        runtime.stop();
    }

    void TestConnector::parameterHeaders() {
        FakeHttpEditor http;
        expect(http.listen(), "parameter-header fake editor must listen");
        if (QTest::currentTestFailed())
            return;
        http.annotatedApplicationHeaders = true;
        http.exposeInvalidAnnotatedTool = true;

        const auto serviceName = uniqueBootstrapServiceName();
        FakeBootstrap bootstrap(serviceName);
        expect(bootstrap.listen(), "parameter-header bootstrap must listen");
        if (QTest::currentTestFailed())
            return;
        const SingleInstanceAutomationStatus ready{
            .state = SingleInstanceAutomationState::ServerReady,
            .editorInstanceId = QStringLiteral("parameter-header-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
            .serverEnabled = true,
            .serverEndpoint = http.endpoint(),
        };
        bootstrap.publish(ready);

        DsConnector::ConnectorRuntime runtime(
            DsConnector::ConnectorOptions{
                .exposure =
                    {
                               .controlLevel = AutomationWire::ExposureLevel::L0,
                               .includes = {QStringLiteral("id:application.get_info"),
                                     QStringLiteral("id:fake.invalid_header")},
                               },
                .upstreamTimeoutMs = 2000,
        },
            serviceName);
        runtime.start();
        expect(waitUntil(
                   [&] {
                       const auto status = runtime.status();
                       return status.value(QStringLiteral("toolset"))
                                      .toObject()
                                      .value(QStringLiteral("compatibility"))
                                      .toString() != QStringLiteral("refreshing") &&
                              status.value(QStringLiteral("exposure"))
                                      .toObject()
                                      .value(QStringLiteral("generic_target_count"))
                                      .toInt() == 4;
                   },
                   10000),
               "invalid x-mcp-header tools must be excluded while valid tools remain");

        const auto arguments = QJsonObject{
            {QStringLiteral("route"),   QStringLiteral("华东 路由")         },
            {QStringLiteral("retry"),   7                                   },
            {QStringLiteral("enabled"), true                                },
            {QStringLiteral("nested"),
             QJsonObject{{QStringLiteral("region"), QStringLiteral("west")}}},
        };
        bool completed = false;
        QJsonObject result;
        runtime.callTool(QStringLiteral("editor.tools.invoke"),
                         QJsonObject{
                             {QStringLiteral("name"),      QStringLiteral("application.get_info")},
                             {QStringLiteral("arguments"), arguments                             }
        },
                         [&](const DsConnector::ToolCallOutcome &outcome) {
                             completed = true;
                             result = outcome.result;
                         });
        expect(waitUntil([&] { return completed; }, 5000) &&
                   !result.value(QStringLiteral("isError")).toBool(),
               "an annotated generic invocation must complete through HTTP");

        const auto decoded = [&](const QByteArray &name) {
            QString error;
            const auto value = AutomationWire::Mcp::decodeHeaderValue(
                QString::fromUtf8(http.lastParameterHeaders.value(name)), &error);
            expect(value.has_value() && error.isEmpty(),
                   "Mcp-Param values must use the standard reversible encoding");
            return value.value_or(QString());
        };
        expect(decoded("mcp-param-route") == QStringLiteral("华东 路由") &&
                   decoded("mcp-param-retry") == QStringLiteral("7") &&
                   decoded("mcp-param-enabled") == QStringLiteral("true") &&
                   decoded("mcp-param-region") == QStringLiteral("west") &&
                   http.lastParameterHeaders.value("mcp-param-route") !=
                       QStringLiteral("华东 路由").toUtf8(),
               "primitive and nested x-mcp-header values must be extracted and encoded");

        const auto callsBeforeInvalid = http.calledTools.size();
        bool invalidCompleted = false;
        bool invalidIsError = true;
        runtime.callTool(QStringLiteral("editor.tools.invoke"),
                         QJsonObject{
                             {QStringLiteral("name"),      QStringLiteral("application.get_info")},
                             {QStringLiteral("arguments"),
                              QJsonObject{{QStringLiteral("route"), QStringLiteral("missing")}}  }
        },
                         [&](const DsConnector::ToolCallOutcome &outcome) {
                             invalidCompleted = true;
                             invalidIsError =
                                 outcome.result.value(QStringLiteral("isError")).toBool();
                         });
        expect(waitUntil([&] { return invalidCompleted; }, 5000) && !invalidIsError &&
                   http.calledTools.size() > callsBeforeInvalid,
               "generic proxying must leave business argument validation to the editor");
        runtime.stop();
    }

    void TestConnector::commandTransportOutcome() {
        FakeHttpEditor http;
        expect(http.listen(), "command-transport fake editor must listen");
        if (QTest::currentTestFailed())
            return;
        http.exposeCommandTool = true;
        const auto serviceName = uniqueBootstrapServiceName();
        FakeBootstrap bootstrap(serviceName);
        expect(bootstrap.listen(), "command-transport bootstrap must listen");
        if (QTest::currentTestFailed())
            return;
        bootstrap.publish(SingleInstanceAutomationStatus{
            .state = SingleInstanceAutomationState::ServerReady,
            .editorInstanceId = QStringLiteral("command-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
            .serverEnabled = true,
            .serverEndpoint = http.endpoint(),
        });
        DsConnector::ConnectorRuntime runtime(
            DsConnector::ConnectorOptions{
                .exposure =
                    {
                               .controlLevel = AutomationWire::ExposureLevel::L0,
                               .includes = {QStringLiteral("id:fake.command")},
                               },
                .upstreamTimeoutMs = 2000,
        },
            serviceName);
        runtime.start();
        expect(waitUntil(
                   [&] {
                       const auto status = runtime.status();
                       return status.value(QStringLiteral("exposure"))
                                      .toObject()
                                      .value(QStringLiteral("generic_target_count"))
                                      .toInt() == 5 &&
                              status.value(QStringLiteral("toolset"))
                                      .toObject()
                                      .value(QStringLiteral("compatibility"))
                                      .toString() == QStringLiteral("compatible");
                   },
                   10000),
               "the fake command must become available after handshake");
        const auto invokeCommand = [&] {
            QPair<QString, QString> result;
            runtime.callTool(
                QStringLiteral("editor.tools.invoke"),
                QJsonObject{
                    {QStringLiteral("name"),      QStringLiteral("fake.command")},
                    {QStringLiteral("arguments"), QJsonObject{}                 }
            },
                [&](const DsConnector::ToolCallOutcome &outcome) {
                    const auto structured =
                        outcome.result.value(QStringLiteral("structuredContent")).toObject();
                    result.first = structured.value(QStringLiteral("code")).toString();
                    result.second = structured.value(QStringLiteral("message")).toString();
                });
            waitUntil([&] { return !result.first.isEmpty(); }, 5000);
            return result;
        };

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::TransportError;
        http.applicationTransportStatus = 429;
        http.applicationTransportCode = QStringLiteral("too_many_requests");
        http.applicationTransportMessage = QStringLiteral("fake request limit reached");
        const auto rateLimited = invokeCommand();
        expect(rateLimited.first == QStringLiteral("too_many_requests") &&
                   rateLimited.second == QStringLiteral("fake request limit reached"),
               "a trusted HTTP 429 envelope must remain too_many_requests for commands");

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::TransportError;
        http.applicationTransportStatus = 503;
        http.applicationTransportCode = QStringLiteral("busy");
        http.applicationTransportMessage = QStringLiteral("fake global concurrency limit reached");
        const auto busy = invokeCommand();
        expect(busy.first == QStringLiteral("busy") &&
                   busy.second == QStringLiteral("fake global concurrency limit reached"),
               "a trusted HTTP 503 busy envelope must not become outcome_unknown");

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::TransportError;
        http.applicationTransportStatus = 503;
        http.applicationTransportCode = QStringLiteral("mcp_stopping");
        http.applicationTransportMessage = QStringLiteral("fake MCP server is stopping");
        const auto stopping = invokeCommand();
        expect(stopping.first == QStringLiteral("mcp_stopping") &&
                   stopping.second == QStringLiteral("fake MCP server is stopping"),
               "a trusted HTTP 503 stopping envelope must remain deterministic");

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::TransportError;
        http.applicationTransportStatus = 504;
        http.applicationTransportCode = QStringLiteral("request_timeout");
        http.applicationTransportMessage = QStringLiteral("fake request deadline elapsed");
        const auto serverTimeout = invokeCommand();
        expect(serverTimeout.first == QStringLiteral("outcome_unknown") &&
                   serverTimeout.second == QStringLiteral("request_timeout"),
               "an HTTP request deadline must retain unknown command outcome semantics");

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::TransportError;
        http.applicationTransportStatus = 429;
        http.applicationTransportCode = QStringLiteral("busy");
        http.applicationTransportMessage = QStringLiteral("forged status/code pair");
        const auto mismatched = invokeCommand();
        expect(mismatched.first == QStringLiteral("outcome_unknown") &&
                   mismatched.second == QStringLiteral("invalid_upstream_response"),
               "a mismatched transport status and code must not be trusted");

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::TransportError;
        http.applicationTransportStatus = 429;
        http.applicationTransportCode = QStringLiteral("too_many_requests");
        http.applicationTransportMessage = QStringLiteral("forged envelope shape");
        http.applicationTransportExtraField = true;
        const auto extendedEnvelope = invokeCommand();
        http.applicationTransportExtraField = false;
        expect(extendedEnvelope.first == QStringLiteral("outcome_unknown") &&
                   extendedEnvelope.second == QStringLiteral("invalid_upstream_response"),
               "a transport envelope with undeclared fields must not be trusted");

        for (const auto &response :
             {qMakePair(FakeHttpEditor::ApplicationResponseMode::MalformedJson,
                        QStringLiteral("invalid_upstream_json_response")),
              qMakePair(FakeHttpEditor::ApplicationResponseMode::RepeatedSse,
                        QStringLiteral("multiple_upstream_sse_responses"))}) {
            const auto callsBefore = http.calledTools.count(QStringLiteral("fake.command"));
            http.applicationResponseMode = response.first;
            const auto unknown = invokeCommand();
            QCOMPARE(unknown.first, QStringLiteral("outcome_unknown"));
            QCOMPARE(unknown.second, response.second);
            QCOMPARE(http.calledTools.count(QStringLiteral("fake.command")), callsBefore + 1);
        }

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Hold;
        const auto timedOut = invokeCommand();
        if (timedOut.first != QStringLiteral("outcome_unknown") ||
            timedOut.second != QStringLiteral("upstream_timeout")) {
            QTextStream(stderr) << "Command timeout observed code=" << timedOut.first
                                << " message=" << timedOut.second << Qt::endl;
        }
        expect(timedOut.first == QStringLiteral("outcome_unknown") &&
                   timedOut.second == QStringLiteral("upstream_timeout"),
               "command timeout must report outcome_unknown while preserving the cause");
        waitUntil([&] { return http.connectionCount() == 0; }, 3000);
        runtime.stop();
    }

    void TestConnector::paginatedHandshake() {
        FakeHttpEditor http;
        QVERIFY2(http.listen(), "pagination fake editor must listen");
        http.extraToolCount = 3;
        http.pageSize = 2;
        const auto lastToolName = QStringLiteral("fake.tool.002");

        const auto serviceName = uniqueBootstrapServiceName();
        FakeBootstrap bootstrap(serviceName);
        QVERIFY2(bootstrap.listen(), "pagination fake bootstrap must listen");
        SingleInstanceAutomationStatus ready{
            .state = SingleInstanceAutomationState::ServerReady,
            .editorInstanceId = QStringLiteral("pagination-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
            .serverEnabled = true,
            .serverEndpoint = http.endpoint(),
        };
        bootstrap.publish(ready);

        DsConnector::ConnectorRuntime runtime(
            DsConnector::ConnectorOptions{
                .exposure =
                    {
                               .controlLevel = AutomationWire::ExposureLevel::L0,
                               .includes = {QStringLiteral("prefix:fake.")},
                               },
                .upstreamTimeoutMs = 2000,
        },
            serviceName);
        runtime.start();
        const auto handshakeComplete = [&] {
            const auto status = runtime.status();
            const auto mcp = status.value(QStringLiteral("mcp")).toObject();
            return mcp.value(QStringLiteral("connected")).toBool() &&
                   mcp.value(QStringLiteral("pending_request_count")).toInt() == 0 &&
                   status.value(QStringLiteral("toolset"))
                           .toObject()
                           .value(QStringLiteral("compatibility"))
                           .toString() != QStringLiteral("refreshing");
        };
        QVERIFY2(waitUntil([&] { return http.statusCallCount == 1 && handshakeComplete(); }, 15000),
                 qPrintable(QString::fromUtf8(
                     QJsonDocument(runtime.status()).toJson(QJsonDocument::Compact))));
        QVERIFY2(http.toolsListCount > 1, "the handshake must consume multiple tools/list pages");

        const auto pageRequests = http.toolsListCount;
        const auto statusRequests = http.statusCallCount;
        const auto downstream = runtime.downstreamTools();
        const auto stableStatus = runtime.status();
        QCOMPARE(runtime.status(), stableStatus);
        QJsonObject bridgedStatus;
        runtime.callTool(
            QStringLiteral("connector.get_status"), {},
            [&bridgedStatus](const DsConnector::ToolCallOutcome &outcome) {
                bridgedStatus =
                    outcome.result.value(QStringLiteral("structuredContent")).toObject();
            });
        QCOMPARE(bridgedStatus, stableStatus);

        QJsonObject described;
        runtime.callTool(QStringLiteral("editor.tools.describe"),
                         {
                             {QStringLiteral("name"), lastToolName}
        },
                         [&described](const DsConnector::ToolCallOutcome &outcome) {
                             described = outcome.result;
                         });
        QVERIFY(!described.value(QStringLiteral("isError")).toBool());
        const auto describedTool = described.value(QStringLiteral("structuredContent"))
                                       .toObject()
                                       .value(QStringLiteral("tool"))
                                       .toObject();
        QCOMPARE(describedTool.value(QStringLiteral("name")).toString(), lastToolName);
        QVERIFY(describedTool.value(QStringLiteral("inputSchema")).isObject());
        QVERIFY(!toolNames(downstream).contains(lastToolName));
        QCOMPARE(http.toolsListCount, pageRequests);
        QCOMPARE(http.statusCallCount, statusRequests);

        http.extraToolCount = 2;
        ready.buildId = QStringLiteral("refreshed-build");
        bootstrap.publish(ready);
        QVERIFY2(waitUntil(
                     [&] {
                         return http.statusCallCount == statusRequests + 1 &&
                                http.toolsListCount > pageRequests && handshakeComplete();
                     },
                     15000),
                 "a new editor snapshot must refresh the paginated cache once");
        const auto refreshedPages = http.toolsListCount;
        const auto refreshedStatusRequests = http.statusCallCount;
        const auto refreshedStatus = runtime.status();
        QVERIFY(refreshedStatus != stableStatus);
        QCOMPARE(refreshedStatus.value(QStringLiteral("editor"))
                     .toObject()
                     .value(QStringLiteral("build_id"))
                     .toString(),
                 ready.buildId);
        QCOMPARE(runtime.downstreamTools(), downstream);

        QJsonObject removed;
        runtime.callTool(
            QStringLiteral("editor.tools.describe"),
            {
                {QStringLiteral("name"), lastToolName}
        },
            [&removed](const DsConnector::ToolCallOutcome &outcome) { removed = outcome.result; });
        QVERIFY(removed.value(QStringLiteral("isError")).toBool());

        QJsonObject retained;
        runtime.callTool(QStringLiteral("editor.tools.describe"),
                         {
                             {QStringLiteral("name"), QStringLiteral("fake.tool.000")}
        },
                         [&retained](const DsConnector::ToolCallOutcome &outcome) {
                             retained = outcome.result;
                         });
        QVERIFY(!retained.value(QStringLiteral("isError")).toBool());
        QCOMPARE(retained.value(QStringLiteral("structuredContent"))
                     .toObject()
                     .value(QStringLiteral("tool"))
                     .toObject()
                     .value(QStringLiteral("name"))
                     .toString(),
                 QStringLiteral("fake.tool.000"));
        QCOMPARE(runtime.status(), refreshedStatus);
        QCOMPARE(http.toolsListCount, refreshedPages);
        QCOMPARE(http.statusCallCount, refreshedStatusRequests);
        runtime.stop();
    }

    void TestConnector::concurrentConnectors() {
        FakeHttpEditor http;
        expect(http.listen(), "concurrent fake editor must listen");
        if (QTest::currentTestFailed())
            return;
        const auto serviceName = uniqueBootstrapServiceName();
        FakeBootstrap bootstrap(serviceName);
        expect(bootstrap.listen(), "concurrent fake bootstrap must listen");
        if (QTest::currentTestFailed())
            return;
        SingleInstanceAutomationStatus ready{
            .state = SingleInstanceAutomationState::ServerReady,
            .editorInstanceId = QStringLiteral("concurrent-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
            .serverEnabled = true,
            .serverEndpoint = http.endpoint(),
        };
        bootstrap.publish(ready);
        const DsConnector::ConnectorOptions options{
            .exposure =
                {
                           .controlLevel = AutomationWire::ExposureLevel::L0,
                           .includes = {QStringLiteral("id:application.get_info")},
                           },
            .upstreamTimeoutMs = 2000,
        };
        DsConnector::ConnectorRuntime first(options, serviceName);
        DsConnector::ConnectorRuntime second(options, serviceName);
        first.start();
        second.start();
        const auto readyRuntime = [](const DsConnector::ConnectorRuntime &runtime) {
            const auto status = runtime.status();
            return status.value(QStringLiteral("mcp"))
                       .toObject()
                       .value(QStringLiteral("connected"))
                       .toBool() &&
                   status.value(QStringLiteral("toolset"))
                           .toObject()
                           .value(QStringLiteral("compatibility"))
                           .toString() != QStringLiteral("refreshing");
        };
        expect(waitUntil(
                   [&] {
                       return readyRuntime(first) && readyRuntime(second) &&
                              bootstrap.watcherCount() == 2;
                   },
                   15000),
               "two connectors must discover and watch the same editor independently");
        expect(first.instanceId() != second.instanceId(),
               "concurrent connectors must use distinct connector identities");
        const auto fixedCount = first.downstreamTools().size();
        first.reconnect();
        expect(waitUntil(
                   [&] {
                       return readyRuntime(first) && readyRuntime(second) &&
                              bootstrap.watcherCount() == 2;
                   },
                   15000),
               "one connector reconnect must not disturb the other connector");
        expect(first.downstreamTools().size() == fixedCount &&
                   second.downstreamTools().size() == fixedCount,
               "concurrent reconnect must preserve both fixed tool surfaces");

        const auto refreshCount = http.toolsListCount;
        bootstrap.publish(ready);
        expect(waitUntil(
                   [&] {
                       return http.toolsListCount >= refreshCount + 2 && readyRuntime(first) &&
                              readyRuntime(second);
                   },
                   15000),
               "one ready snapshot must independently refresh both connectors");
        first.stop();
        expect(waitUntil([&] { return bootstrap.watcherCount() == 1; }) && readyRuntime(second),
               "stopping one connector must leave the other watch and MCP session alive");
        second.stop();
    }

    void TestConnector::stdioFraming() {
        const auto executable = QString::fromUtf8(TEST_CONNECTOR_EXECUTABLE_PATH);
        expect(QFile::exists(executable), "connector executable must exist for stdio E2E");
        if (QTest::currentTestFailed())
            return;

        QProcess process;
        process.setProgram(executable);
        process.setArguments({QStringLiteral("--control-level"), QStringLiteral("l0")});
        process.start();
        expect(process.waitForStarted(5000), "stdio connector process must start");
        const auto context = clientContext();
        const auto discover =
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod), {}, context,
                              QStringLiteral("stdio-discover")))
                .toJson(QJsonDocument::Compact);
        const auto list =
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod), {},
                              context, QStringLiteral("stdio-list")))
                .toJson(QJsonDocument::Compact);
        process.write(discover + "\r\n" + list + "\r\n");
        process.closeWriteChannel();
        expect(process.waitForFinished(15000),
               "multi-frame CRLF stdio connector must finish under watchdog");
        const auto standardOutput = process.readAllStandardOutput();
        const auto standardError = process.readAllStandardError();
        auto lines = standardOutput.split('\n');
        if (!lines.isEmpty() && lines.back().isEmpty())
            lines.removeLast();
        bool allJson = lines.size() == 2;
        for (const auto &line : std::as_const(lines)) {
            QJsonParseError error;
            allJson &= !line.trimmed().isEmpty() &&
                       QJsonDocument::fromJson(line, &error).isObject() &&
                       error.error == QJsonParseError::NoError;
        }
        expect(allJson && standardError.isEmpty(),
               "stdout must contain exactly one JSON object per line and no logs or blanks");

        QProcess unterminated;
        unterminated.setProgram(executable);
        unterminated.setArguments({QStringLiteral("--control-level"), QStringLiteral("l0")});
        unterminated.start();
        expect(unterminated.waitForStarted(5000), "unterminated-final-frame connector must start");
        unterminated.write(discover);
        unterminated.closeWriteChannel();
        expect(unterminated.waitForFinished(15000) &&
                   unterminated.readAllStandardOutput().split('\n').size() == 2 &&
                   unterminated.readAllStandardError().isEmpty() && unterminated.exitCode() == 0,
               "a valid final EOF frame without newline must be processed exactly once");

        QProcess notification;
        notification.setProgram(executable);
        notification.setArguments({QStringLiteral("--control-level"), QStringLiteral("l0")});
        notification.start();
        expect(notification.waitForStarted(5000), "stdio notification connector must start");
        notification.write(
            QJsonDocument(
                AutomationWire::Mcp::makeRequest(
                    QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod), {}, context))
                .toJson(QJsonDocument::Compact) +
            '\n');
        notification.closeWriteChannel();
        expect(notification.waitForFinished(15000) &&
                   notification.readAllStandardOutput().isEmpty() &&
                   notification.readAllStandardError().isEmpty(),
               "a recognizable JSON-RPC notification must produce zero stdout bytes");

        QProcess rapidInput;
        rapidInput.setProgram(executable);
        rapidInput.setArguments({QStringLiteral("--control-level"), QStringLiteral("l0")});
        rapidInput.start();
        expect(rapidInput.waitForStarted(5000), "rapid-input connector must start");
        const auto notificationFrame =
            QJsonDocument(
                AutomationWire::Mcp::makeRequest(
                    QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod), {}, context))
                .toJson(QJsonDocument::Compact) +
            '\n';
        QByteArray notificationFlood;
        notificationFlood.reserve(notificationFrame.size() * 20000);
        for (auto index = 0; index < 20000; ++index)
            notificationFlood.append(notificationFrame);
        rapidInput.write(notificationFlood);
        rapidInput.closeWriteChannel();
        expect(rapidInput.waitForFinished(20000) && rapidInput.exitCode() == 0 &&
                   rapidInput.readAllStandardOutput().isEmpty() &&
                   rapidInput.readAllStandardError().isEmpty(),
               "bounded stdin delivery must drain a rapid notification flood without output");

        const auto validCompleteToolList = [](const QByteArray &output) {
            QJsonParseError error;
            const auto response = QJsonDocument::fromJson(output.trimmed(), &error).object();
            const auto tools =
                response.value(QStringLiteral("result")).toObject().value(QStringLiteral("tools"));
            return error.error == QJsonParseError::NoError &&
                   response.value(QStringLiteral("id")) == QStringLiteral("stdio-list") &&
                   tools.isArray() && !tools.toArray().isEmpty();
        };
        QProcess largeOutput;
        largeOutput.setProgram(executable);
        largeOutput.setArguments({QStringLiteral("--control-level"), QStringLiteral("l2")});
        largeOutput.start();
        expect(largeOutput.waitForStarted(5000), "large-output connector must start");
        largeOutput.write(list + '\n');
        largeOutput.closeWriteChannel();
        const auto largeFinished = largeOutput.waitForFinished(20000);
        const auto largeResponse = largeOutput.readAllStandardOutput();
        expect(largeFinished && largeOutput.exitStatus() == QProcess::NormalExit &&
                   largeOutput.exitCode() == 0 && largeResponse.size() > 64 * 1024 &&
                   validCompleteToolList(largeResponse) &&
                   largeOutput.readAllStandardError().isEmpty(),
               "the complete tool response must drain as one valid large JSON frame");

        const auto slowSinkPath =
            QDir::temp().filePath(QStringLiteral("DsConnectorLite-slow-stdout-%1.json")
                                      .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
        QProcess slowOutput;
        QProcess slowSink;
        slowOutput.setProgram(executable);
        slowOutput.setArguments({QStringLiteral("--control-level"), QStringLiteral("l2")});
        slowSink.setProgram(QCoreApplication::applicationFilePath());
        slowSink.setArguments({QStringLiteral("--slow-stdio-sink"), slowSinkPath});
        slowOutput.setStandardOutputProcess(&slowSink);
        slowOutput.start();
        slowSink.start();
        expect(slowOutput.waitForStarted(5000) && slowSink.waitForStarted(5000),
               "large-output connector and slow stdout reader must start");
        slowOutput.write(list + '\n');
        slowOutput.closeWriteChannel();
        const auto slowOutputFinished = slowOutput.waitForFinished(30000);
        const auto slowSinkFinished = slowSink.waitForFinished(30000);
        QFile slowSinkResult(slowSinkPath);
        QByteArray slowResponse;
        if (slowSinkResult.open(QIODevice::ReadOnly))
            slowResponse = slowSinkResult.readAll();
        slowSinkResult.close();
        QFile::remove(slowSinkPath);
        expect(slowOutputFinished && slowSinkFinished &&
                   slowOutput.exitStatus() == QProcess::NormalExit && slowOutput.exitCode() == 0 &&
                   slowSink.exitStatus() == QProcess::NormalExit && slowSink.exitCode() == 0 &&
                   slowResponse.size() > 64 * 1024 && validCompleteToolList(slowResponse) &&
                   slowOutput.readAllStandardError().isEmpty() &&
                   slowSink.readAllStandardError().isEmpty(),
               "a slow reader must drain the complete large frame through sustained progress");

        QProcess blockedOutput;
        QProcess blockingSink;
        blockedOutput.setProgram(executable);
        blockedOutput.setArguments({QStringLiteral("--control-level"), QStringLiteral("l0")});
        blockingSink.setProgram(QCoreApplication::applicationFilePath());
        blockingSink.setArguments({QStringLiteral("--blocked-stdio-sink")});
        blockedOutput.setStandardOutputProcess(&blockingSink);
        blockedOutput.start();
        blockingSink.start();
        QVERIFY2(blockedOutput.waitForStarted(5000) && blockingSink.waitForStarted(5000),
                 "blocked-output connector and non-reading sink must start");
        QByteArray requestFlood;
        requestFlood.reserve((discover.size() + 1) * 4096);
        for (auto index = 0; index < 4096; ++index) {
            requestFlood.append(discover);
            requestFlood.append('\n');
        }
        QElapsedTimer blockedTimer;
        blockedTimer.start();
        blockedOutput.write(requestFlood);
        blockedOutput.closeWriteChannel();
        const auto blockedFinished = blockedOutput.waitForFinished(15000);
        const auto blockedError = blockedOutput.readAllStandardError();
        if (!blockedFinished || blockedTimer.elapsed() >= 10000 ||
            blockedOutput.exitStatus() != QProcess::NormalExit || blockedOutput.exitCode() != 3 ||
            !blockedError.contains("stdout_backpressure_limit_exceeded")) {
            QTextStream(stderr) << "Blocked stdout observed finished=" << blockedFinished
                                << " elapsed_ms=" << blockedTimer.elapsed()
                                << " exit_status=" << blockedOutput.exitStatus()
                                << " exit_code=" << blockedOutput.exitCode()
                                << " stderr=" << blockedError << Qt::endl;
        }
        expect(blockedFinished && blockedTimer.elapsed() < 10000 &&
                   blockedOutput.exitStatus() == QProcess::NormalExit &&
                   blockedOutput.exitCode() == 3 &&
                   blockedError.contains("stdout_backpressure_limit_exceeded"),
               "a non-reading stdout peer must trip the bounded writer queue without "
               "blocking the main event loop");
        blockingSink.kill();
        blockingSink.waitForFinished(5000);

        QProcess boundary;
        boundary.setProgram(executable);
        boundary.setArguments({QStringLiteral("--control-level"), QStringLiteral("l0")});
        boundary.start();
        expect(boundary.waitForStarted(5000), "boundary-frame connector must start");
        boundary.write(QByteArray(16 * 1024 * 1024, 'x') + '\n');
        boundary.closeWriteChannel();
        expect(boundary.waitForFinished(15000),
               "the exact 16 MiB frame boundary must finish under watchdog");
        const auto boundaryOutput = boundary.readAllStandardOutput();
        const auto boundaryResponse = QJsonDocument::fromJson(boundaryOutput.trimmed()).object();
        expect(boundaryResponse.value(QStringLiteral("error"))
                           .toObject()
                           .value(QStringLiteral("code"))
                           .toInt() == AutomationWire::Mcp::ParseError &&
                   boundary.readAllStandardError().isEmpty() && boundary.exitCode() == 0,
               "an exact-limit frame must be parsed instead of rejected as too large");

        QProcess oversizedLine;
        oversizedLine.setProgram(executable);
        oversizedLine.setArguments({QStringLiteral("--control-level"), QStringLiteral("l0")});
        oversizedLine.start();
        expect(oversizedLine.waitForStarted(5000), "oversized newline-frame connector must start");
        oversizedLine.write(QByteArray(16 * 1024 * 1024 + 1, 'x') + '\n');
        oversizedLine.closeWriteChannel();
        expect(oversizedLine.waitForFinished(15000) &&
                   oversizedLine.readAllStandardOutput().isEmpty() &&
                   oversizedLine.readAllStandardError().contains("frame_too_large") &&
                   oversizedLine.exitCode() == 3,
               "an overlong newline-terminated frame must be rejected without parsing");

        QProcess oversized;
        oversized.setProgram(executable);
        oversized.setArguments({QStringLiteral("--control-level"), QStringLiteral("l0")});
        oversized.start();
        expect(oversized.waitForStarted(5000), "oversized-frame connector must start");
        oversized.write(QByteArray(16 * 1024 * 1024 + 1, 'x'));
        oversized.closeWriteChannel();
        expect(oversized.waitForFinished(15000),
               "oversized no-newline frame must terminate under watchdog");
        expect(oversized.readAllStandardOutput().isEmpty() &&
                   oversized.readAllStandardError().contains("frame_too_large") &&
                   oversized.exitCode() == 3,
               "an overlong partial frame must never be parsed and must report frame_too_large");
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    if (application.arguments().size() == 3 &&
        application.arguments().at(1) == QStringLiteral("--slow-stdio-sink"))
        return runSlowStdioSink(application.arguments().at(2));
    if (application.arguments().size() == 2 &&
        application.arguments().at(1) == QStringLiteral("--blocked-stdio-sink")) {
        QTimer::singleShot(30000, &application, &QCoreApplication::quit);
        return application.exec();
    }
    TestConnector test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_connector.moc"
