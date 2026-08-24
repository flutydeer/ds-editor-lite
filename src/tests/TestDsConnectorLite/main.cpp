#include "ConnectorOptions.h"
#include "ConnectorRuntime.h"
#include "DownstreamMcpServer.h"
#include "ExposurePolicy.h"

#include <lite/AutomationWire/McpProtocol.h>
#include <lite/AutomationWire/JsonSchema.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QFile>
#include <QProcess>
#include <QQueue>
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

    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
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

    std::optional<QJsonObject> takeResponseById(QQueue<QByteArray> &responses,
                                                const QJsonValue &id,
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

    AutomationWire::Mcp::RequestContext clientContext() {
        return {
            .clientCapabilities = QJsonObject{{QStringLiteral("tools"), QJsonObject{}}},
            .clientInfo = AutomationWire::Mcp::ImplementationInfo{
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
            return m_server.listen(m_serviceName);
        }

        qsizetype watcherCount() const {
            return m_watchers.size();
        }

        void publish(SingleInstanceAutomationStatus status) {
            m_status = std::move(status);
            const SingleInstanceAutomationSnapshot snapshot{
                {}, QCoreApplication::applicationPid(), m_status,
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
                validWatchRequest = !request.connector.instanceId.isEmpty() &&
                                    !request.connector.version.isEmpty();
                if (!m_watchers.contains(socket))
                    m_watchers.append(socket);
                const SingleInstanceAutomationSnapshot snapshot{
                    emptyFirstRequestId ? QString() : request.requestId,
                    QCoreApplication::applicationPid(), m_status,
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
            Oversized,
        };

        FakeHttpEditor() {
            QObject::connect(&m_server, &QTcpServer::newConnection, &m_server, [this] {
                while (auto *socket = m_server.nextPendingConnection()) {
                    m_buffers.insert(socket, {});
                    QObject::connect(socket, &QTcpSocket::readyRead, socket,
                                     [this, socket] { readRequest(socket); });
                    QObject::connect(socket, &QTcpSocket::disconnected, socket, [this, socket] {
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

        qsizetype connectionCount() const {
            return m_buffers.size();
        }

        bool headersValid = true;
        bool exposeNotes = false;
        bool exposeFilteredTool = false;
        bool incompatibleApplicationSchema = false;
        bool annotatedApplicationHeaders = false;
        bool exposeInvalidAnnotatedTool = false;
        bool exposeCommandTool = false;
        bool exposeForwardCompatibleTools = false;
        QString applicationAvailability;
        ApplicationResponseMode applicationResponseMode = ApplicationResponseMode::Success;
        int applicationTransportStatus = 429;
        QString applicationTransportCode = QStringLiteral("too_many_requests");
        QString applicationTransportMessage = QStringLiteral("fake request limit reached");
        bool applicationTransportExtraField = false;
        int extraToolCount = 0;
        int pageSize = 0;
        int manifestToolsetVersion = static_cast<int>(AutomationWire::PublicToolsetVersion);
        int applicationVersion = static_cast<int>(AutomationWire::PublicToolsetVersion);
        int applicationMinimumCompatibleVersion =
            static_cast<int>(AutomationWire::PublicMinimumCompatibleVersion);
        int discoverResponseDelayMs = 0;
        int discoverRateLimitFailuresRemaining = 0;
        int discoverCount = 0;
        int toolsListCount = 0;
        int manifestCallCount = 0;
        QList<QJsonValue> requestIds;
        QStringList calledTools;
        QHash<QByteArray, QByteArray> lastParameterHeaders;

    private:
        QJsonObject annotatedInputSchema(const bool valid = true) const {
            return {
                {QStringLiteral("$schema"),
                 QStringLiteral("https://json-schema.org/draft/2020-12/schema")},
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"),
                 QJsonObject{
                     {QStringLiteral("route"),
                      QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                  {QStringLiteral("x-mcp-header"),
                                   valid ? QStringLiteral("Route")
                                         : QStringLiteral("Bad Name")}}},
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
                           QJsonObject{{QStringLiteral("region"),
                                        QJsonObject{
                                            {QStringLiteral("type"),
                                             QStringLiteral("string")},
                                            {QStringLiteral("x-mcp-header"),
                                             QStringLiteral("Region")}}}}},
                          {QStringLiteral("required"),
                           QJsonArray{QStringLiteral("region")}},
                          {QStringLiteral("additionalProperties"), false},
                      }},
                 }},
                {QStringLiteral("required"),
                 QJsonArray{QStringLiteral("route"), QStringLiteral("retry"),
                            QStringLiteral("enabled"), QStringLiteral("nested")}},
                {QStringLiteral("additionalProperties"), false},
            };
        }

        QJsonObject flexibleInputSchema() const {
            return {
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"),
                 QJsonObject{{QStringLiteral("shape"),
                              QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                          {QStringLiteral("enum"),
                                           QJsonArray{QStringLiteral("string"),
                                                      QStringLiteral("array"),
                                                      QStringLiteral("null"),
                                                      QStringLiteral("invalid")}}}}}},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("shape")}},
                {QStringLiteral("additionalProperties"), false},
            };
        }

        QJsonObject flexibleOutputSchema() const {
            return {
                {QStringLiteral("oneOf"),
                 QJsonArray{
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}},
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                                 {QStringLiteral("items"),
                                  QJsonObject{{QStringLiteral("type"),
                                               QStringLiteral("integer")}}}},
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("null")}},
                 }},
            };
        }

        QJsonArray allTools() const {
            const auto cacheKey = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9")
                                      .arg(exposeNotes)
                                      .arg(exposeFilteredTool)
                                      .arg(incompatibleApplicationSchema)
                                      .arg(annotatedApplicationHeaders)
                                      .arg(exposeInvalidAnnotatedTool)
                                      .arg(exposeCommandTool)
                                      .arg(applicationAvailability)
                                      .arg(extraToolCount)
                                      .arg(exposeForwardCompatibleTools);
            if (cacheKey == m_toolsCacheKey)
                return m_cachedTools;
            QJsonArray tools;
            auto applicationTool =
                AutomationWire::findPublicTool(QStringLiteral("application.get_info"))
                    ->toMcpToolJson();
            if (!applicationAvailability.isEmpty())
                applicationTool.insert(QStringLiteral("availability"), applicationAvailability);
            if (annotatedApplicationHeaders)
                applicationTool.insert(QStringLiteral("inputSchema"), annotatedInputSchema());
            if (incompatibleApplicationSchema) {
                applicationTool.insert(
                    QStringLiteral("outputSchema"),
                    QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("object")},
                        {QStringLiteral("properties"),
                         QJsonObject{{QStringLiteral("incompatible"),
                                      QJsonObject{{QStringLiteral("type"),
                                                   QStringLiteral("boolean")}}}}},
                        {QStringLiteral("required"),
                         QJsonArray{QStringLiteral("incompatible")}},
                        {QStringLiteral("additionalProperties"), false},
                    });
            }
            tools.append(applicationTool);
            tools.append(
                AutomationWire::findPublicTool(QStringLiteral("automation.get_manifest"))
                    ->toMcpToolJson());
            if (exposeNotes) {
                tools.append(AutomationWire::findPublicTool(QStringLiteral("notes.get"))
                                 ->toMcpToolJson());
            }
            if (exposeFilteredTool) {
                tools.append(AutomationWire::findPublicTool(QStringLiteral("project.get"))
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
                tools.append(command);
            }
            if (exposeForwardCompatibleTools) {
                tools.append(QJsonObject{
                    {QStringLiteral("name"), QStringLiteral("fake.flexible_output")},
                    {QStringLiteral("inputSchema"), flexibleInputSchema()},
                    {QStringLiteral("outputSchema"), flexibleOutputSchema()},
                    {QStringLiteral("icons"),
                     QJsonArray{QJsonObject{
                         {QStringLiteral("src"), QStringLiteral("https://example.invalid/tool.svg")},
                         {QStringLiteral("mimeType"), QStringLiteral("image/svg+xml")},
                     }}},
                    {QStringLiteral("_meta"),
                     QJsonObject{{QStringLiteral("com.openvpi.ds-editor-lite/fixture"), true}}},
                });
                tools.append(QJsonObject{
                    {QStringLiteral("name"), QStringLiteral("fake.minimal")},
                    {QStringLiteral("inputSchema"),
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                                 {QStringLiteral("additionalProperties"), false}}},
                    {QStringLiteral("icons"),
                     QJsonArray{QJsonObject{
                         {QStringLiteral("src"),
                          QStringLiteral("https://example.invalid/minimal.svg")},
                     }}},
                    {QStringLiteral("_meta"),
                     QJsonObject{{QStringLiteral("com.openvpi.ds-editor-lite/fixture"),
                                  QStringLiteral("minimal")}}},
                });
            }
            for (auto index = 0; index < extraToolCount; ++index) {
                auto tool = applicationTool;
                const auto id = QStringLiteral("fake.tool.%1").arg(index, 3, 10, QLatin1Char('0'));
                tool.insert(QStringLiteral("name"), id);
                tool.insert(QStringLiteral("title"), id);
                auto annotations = tool.value(QStringLiteral("annotations")).toObject();
                annotations.insert(QStringLiteral("category"), QStringLiteral("fake"));
                tool.insert(QStringLiteral("annotations"), annotations);
                tools.append(tool);
            }
            m_toolsCacheKey = cacheKey;
            m_cachedTools = tools;
            return m_cachedTools;
        }

        QJsonObject fullManifest() const {
            const auto cacheKey = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9")
                                      .arg(incompatibleApplicationSchema)
                                      .arg(annotatedApplicationHeaders)
                                      .arg(exposeInvalidAnnotatedTool)
                                      .arg(exposeCommandTool)
                                      .arg(extraToolCount)
                                      .arg(manifestToolsetVersion)
                                      .arg(applicationVersion)
                                      .arg(applicationMinimumCompatibleVersion)
                                      .arg(exposeForwardCompatibleTools);
            if (cacheKey == m_manifestCacheKey)
                return m_cachedManifest;
            auto manifest =
                AutomationWire::buildPublicManifest(AutomationWire::AutomationProfile::L1)
                    .toJson();
            manifest.insert(QStringLiteral("toolset_version"), manifestToolsetVersion);
            auto operations = manifest.value(QStringLiteral("operations")).toArray();
            QJsonObject applicationOperation;
            for (auto index = 0; index < operations.size(); ++index) {
                auto operation = operations.at(index).toObject();
                if (operation.value(QStringLiteral("operation_id")) ==
                    QStringLiteral("application.get_info")) {
                    operation.insert(QStringLiteral("version"), applicationVersion);
                    operation.insert(QStringLiteral("minimum_compatible_version"),
                                     applicationMinimumCompatibleVersion);
                    if (incompatibleApplicationSchema) {
                        operation.insert(
                            QStringLiteral("output_schema"),
                            QJsonObject{
                                {QStringLiteral("type"), QStringLiteral("object")},
                                {QStringLiteral("properties"),
                                 QJsonObject{{QStringLiteral("incompatible"),
                                              QJsonObject{{QStringLiteral("type"),
                                                           QStringLiteral("boolean")}}}}},
                                {QStringLiteral("required"),
                                 QJsonArray{QStringLiteral("incompatible")}},
                                {QStringLiteral("additionalProperties"), false},
                            });
                    }
                    if (annotatedApplicationHeaders)
                        operation.insert(QStringLiteral("input_schema"), annotatedInputSchema());
                    applicationOperation = operation;
                    operations.replace(index, operation);
                }
            }
            if (exposeInvalidAnnotatedTool) {
                auto invalid = applicationOperation;
                invalid.insert(QStringLiteral("operation_id"),
                               QStringLiteral("fake.invalid_header"));
                invalid.insert(QStringLiteral("title"), QStringLiteral("Invalid header mapping"));
                invalid.insert(QStringLiteral("input_schema"), annotatedInputSchema(false));
                operations.append(invalid);
            }
            if (exposeCommandTool) {
                auto command = applicationOperation;
                command.insert(QStringLiteral("operation_id"), QStringLiteral("fake.command"));
                command.insert(QStringLiteral("title"), QStringLiteral("Fake command"));
                command.insert(QStringLiteral("kind"), QStringLiteral("command"));
                operations.append(command);
            }
            if (exposeForwardCompatibleTools) {
                auto flexible = applicationOperation;
                flexible.insert(QStringLiteral("operation_id"),
                                QStringLiteral("fake.flexible_output"));
                flexible.insert(QStringLiteral("title"), QStringLiteral("Flexible output"));
                flexible.insert(QStringLiteral("category"), QStringLiteral("fake"));
                flexible.insert(QStringLiteral("minimum_profile"), QStringLiteral("meta"));
                flexible.insert(QStringLiteral("kind"), QStringLiteral("query"));
                flexible.insert(QStringLiteral("input_schema"), flexibleInputSchema());
                flexible.insert(QStringLiteral("output_schema"), flexibleOutputSchema());
                operations.append(flexible);
            }
            for (auto index = 0; index < extraToolCount; ++index) {
                auto operation = applicationOperation;
                const auto id = QStringLiteral("fake.tool.%1").arg(index, 3, 10, QLatin1Char('0'));
                operation.insert(QStringLiteral("operation_id"), id);
                operation.insert(QStringLiteral("title"), id);
                operation.insert(QStringLiteral("category"), QStringLiteral("fake"));
                operation.insert(QStringLiteral("minimum_profile"), QStringLiteral("meta"));
                operations.append(operation);
            }
            manifest.insert(QStringLiteral("operations"), operations);
            m_manifestCacheKey = cacheKey;
            m_cachedManifest = manifest;
            return m_cachedManifest;
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
            const auto validation = AutomationWire::Mcp::validateRequest(requestObject);
            if (!validation.valid()) {
                respond(socket, AutomationWire::Mcp::makeErrorResponse(
                                    requestObject.value(QStringLiteral("id")), validation.error));
                return;
            }
            const auto &request = *validation.request;
            requestIds.append(request.id);
            headersValid &= headers.value("mcp-protocol-version") ==
                                QByteArray(AutomationWire::Mcp::ProtocolVersion) &&
                            headers.value("mcp-method") == request.method.toUtf8() &&
                            headers.value("accept").contains("application/json") &&
                            headers.value("accept").contains("text/event-stream");
            if (request.method == QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod)) {
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
            }

            AutomationWire::Mcp::ImplementationInfo info{
                .name = QStringLiteral("fake-editor"),
                .version = QStringLiteral("1"),
            };
            QJsonObject result;
            if (request.method == QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod)) {
                ++discoverCount;
                if (discoverRateLimitFailuresRemaining > 0) {
                    --discoverRateLimitFailuresRemaining;
                    respondTransportError(socket, 429, QStringLiteral("too_many_requests"),
                                          QStringLiteral("fake handshake request limit reached"));
                    return;
                }
                result = AutomationWire::Mcp::makeDiscoverResult(info);
                if (discoverResponseDelayMs > 0) {
                    const auto response =
                        AutomationWire::Mcp::makeResultResponse(request.id, result, info);
                    QTimer::singleShot(discoverResponseDelayMs, socket,
                                       [this, socket, response] { respond(socket, response); });
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
                result = AutomationWire::Mcp::makeToolsListResult(tools, nextCursor, 0,
                                                                  QStringLiteral("private"), info);
            } else if (request.name == QStringLiteral("automation.get_manifest")) {
                ++manifestCallCount;
                auto manifest = fullManifest();
                const auto arguments =
                    request.params.value(QStringLiteral("arguments")).toObject();
                bool cursorValid = true;
                const auto cursorText = arguments.value(QStringLiteral("cursor")).toString();
                const auto offset = cursorText.isEmpty() ? 0 : cursorText.toInt(&cursorValid);
                QString nextCursor;
                const auto operations = page(manifest.value(QStringLiteral("operations")).toArray(),
                                             cursorValid ? offset : 0, nextCursor);
                manifest.insert(QStringLiteral("operations"), operations);
                if (nextCursor.isEmpty())
                    manifest.remove(QStringLiteral("next_cursor"));
                else
                    manifest.insert(QStringLiteral("next_cursor"), nextCursor);
                result = AutomationWire::Mcp::makeToolCallResult(manifest);
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
                    structuredContent = QJsonObject{{QStringLiteral("unexpected"), true}};
                respond(socket, AutomationWire::Mcp::makeResultResponse(
                                    request.id,
                                    AutomationWire::Mcp::makeToolCallResult(structuredContent),
                                    info));
                return;
            } else if (request.name == QStringLiteral("fake.minimal")) {
                respond(socket, AutomationWire::Mcp::makeResultResponse(
                                    request.id,
                                    AutomationWire::Mcp::makeToolCallResult(QJsonObject{}), info));
                return;
            } else if (request.name == QStringLiteral("application.get_info") ||
                       request.name == QStringLiteral("fake.command")) {
                const auto mode = applicationResponseMode;
                if (mode == ApplicationResponseMode::Hold)
                    return;
                applicationResponseMode = ApplicationResponseMode::Success;
                if (mode == ApplicationResponseMode::ProtocolError) {
                    respond(socket, AutomationWire::Mcp::makeErrorResponse(
                                        request.id,
                                        {AutomationWire::Mcp::InvalidParams,
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
                                       QStringLiteral("windows")}},
                    mode == ApplicationResponseMode::BusinessError);
                const auto response =
                    AutomationWire::Mcp::makeResultResponse(request.id, result, info);
                if (mode == ApplicationResponseMode::Sse)
                    respondSse(socket, response);
                else
                    respond(socket, response);
                return;
            } else {
                respond(socket, AutomationWire::Mcp::makeErrorResponse(
                                    request.id,
                                    {AutomationWire::Mcp::InvalidParams,
                                     QStringLiteral("unknown fake tool")}));
                return;
            }
            respond(socket, AutomationWire::Mcp::makeResultResponse(request.id, result, info));
        }

        void respond(QTcpSocket *socket, const QJsonObject &response) {
            const auto body = QJsonDocument(response).toJson(QJsonDocument::Compact);
            QByteArray message = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                                 "Connection: close\r\nContent-Length: ";
            message.append(QByteArray::number(body.size()));
            message.append("\r\n\r\n");
            message.append(body);
            m_rawLog.append("=== response ===\n");
            m_rawLog.append(message);
            m_rawLog.append('\n');
            socket->write(message);
            socket->disconnectFromHost();
        }

        void respondSse(QTcpSocket *socket, const QJsonObject &response) {
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
            QByteArray message =
                "HTTP/1.1 200 OK\r\nContent-Type: Text/Event-Stream; Charset=UTF-8\r\n"
                "Connection: close\r\nContent-Length: ";
            message.append(QByteArray::number(events.size()));
            message.append("\r\n\r\n");
            message.append(events);
            m_rawLog.append("=== sse response ===\n");
            m_rawLog.append(message);
            m_rawLog.append('\n');
            socket->write(message);
            socket->disconnectFromHost();
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
                {QStringLiteral("error"),
                 QJsonObject{{QStringLiteral("code"), code},
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

        QTcpServer m_server;
        QHash<QTcpSocket *, QByteArray> m_buffers;
        QByteArray m_rawLog;
        mutable QString m_toolsCacheKey;
        mutable QJsonArray m_cachedTools;
        mutable QString m_manifestCacheKey;
        mutable QJsonObject m_cachedManifest;
    };

    bool verifyOptionsAndExposure() {
        DsConnector::ConnectorOptions options;
        QString error;
        bool ok = true;
        ok &= expect(DsConnector::parseConnectorOptions(
                         {QStringLiteral("--exposure-profile"), QStringLiteral("l0"),
                          QStringLiteral("--include-tool=id:notes.insert"),
                          QStringLiteral("--include-tool"), QStringLiteral("notes.insert"),
                          QStringLiteral("--exclude-tool=category:notes")},
                         options, error),
                     "valid connector options must parse");
        ok &= expect(options.exposure.profile == AutomationWire::ExposureProfile::L0 &&
                         options.exposure.includes.size() == 1 &&
                         options.exposure.excludes.size() == 1,
                     "connector options must normalize profiles and duplicate selectors");
        DsConnector::ExposurePolicy policy(options);
        ok &= expect(policy.typedContracts().isEmpty(),
                     "exclude must win over an exact include selector");

        ok &= expect(!DsConnector::parseConnectorOptions(
                         {QStringLiteral("--include-tool"), QStringLiteral("regex:notes.*")},
                         options, error) && !error.isEmpty(),
                     "unsupported selector syntax must fail startup parsing");

        DsConnector::ExposurePolicy l0(DsConnector::ConnectorOptions{
            .exposure = {.profile = AutomationWire::ExposureProfile::L0},
        });
        DsConnector::ExposurePolicy l1(DsConnector::ConnectorOptions{
            .exposure = {.profile = AutomationWire::ExposureProfile::L1},
        });
        DsConnector::ExposurePolicy l2(DsConnector::ConnectorOptions{
            .exposure = {.profile = AutomationWire::ExposureProfile::L2},
        });
        DsConnector::ExposurePolicy l3(DsConnector::ConnectorOptions{
            .exposure = {.profile = AutomationWire::ExposureProfile::L3},
        });
        ok &= expect(l0.typedContracts().isEmpty(), "l0 must expose no typed editor tools");
        ok &= expect(l1.typedContracts().size() == 55, "l1 must expose the frozen 55 tools");
        ok &= expect(l2.typedContracts().size() == 87, "l2 must expose all 87 tools");
        ok &= expect(l3.typedContracts().size() == 87,
                     "l3 shell must equal l2 until l3 tools are implemented");
        return ok;
    }

    bool verifyOfflineDownstream() {
        DsConnector::ConnectorRuntime runtime(DsConnector::ConnectorOptions{
            .exposure = {.profile = AutomationWire::ExposureProfile::L0},
        }, QStringLiteral("DsConnectorLite-No-Such-Editor"));
        DsConnector::DownstreamMcpServer server(&runtime);
        QQueue<QByteArray> responses;
        QObject::connect(&server, &DsConnector::DownstreamMcpServer::responseLine, &server,
                         [&responses](const QByteArray &line) { responses.enqueue(line); });

        const auto bridgeTools = DsConnector::ConnectorRuntime::bridgeToolDefinitions();
        bool ok = expect(bridgeTools.size() == 6,
                         "connector must publish exactly six fixed bridge definitions");
        for (const auto &entry : bridgeTools) {
            const auto tool = entry.toObject();
            ok &= expect(AutomationWire::checkJsonSchema(
                             tool.value(QStringLiteral("inputSchema")))
                             .valid() &&
                             AutomationWire::checkJsonSchema(
                                 tool.value(QStringLiteral("outputSchema")))
                                 .valid(),
                         "every bridge tool must publish valid input and output schemas");
        }
        const auto statusTool =
            DsConnector::ConnectorRuntime::findBridgeTool(QStringLiteral("connector.get_status"));
        ok &= expect(statusTool &&
                         AutomationWire::validateJsonValue(
                             runtime.status(),
                             statusTool->value(QStringLiteral("outputSchema")))
                             .valid(),
                     "offline connector status must satisfy its fixed output schema");
        const auto offlineStatus = runtime.status();
        ok &= expect(offlineStatus.value(QStringLiteral("connector"))
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
            .clientInfo = AutomationWire::Mcp::ImplementationInfo{
                .name = QStringLiteral("connector-test"),
                .version = QStringLiteral("1"),
            },
        };
        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod), {},
                               context, QStringLiteral("discover")))
                               .toJson(QJsonDocument::Compact));
        ok &= expect(responses.size() == 1,
                     "server/discover must respond while editor is offline");
        if (!responses.isEmpty()) {
            const auto response = QJsonDocument::fromJson(responses.dequeue()).object();
            ok &= expect(response.value(QStringLiteral("id")).toString() ==
                             QStringLiteral("discover") &&
                             response.value(QStringLiteral("result")).toObject().value(
                                 QStringLiteral("resultType")) == QStringLiteral("complete"),
                         "downstream discover response must use modern MCP envelope");
        }

        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod), {},
                               context, QStringLiteral("list")))
                               .toJson(QJsonDocument::Compact));
        ok &= expect(responses.size() == 1, "tools/list must respond while editor is offline");
        if (!responses.isEmpty()) {
            const auto response = QJsonDocument::fromJson(responses.dequeue()).object();
            const auto result = response.value(QStringLiteral("result")).toObject();
            const auto tools = result.value(QStringLiteral("tools")).toArray();
            ok &= expect(tools.size() == 6 &&
                             !result.contains(QStringLiteral("nextCursor")),
                         "l0 downstream list must retain six fixed tools without a cursor");
        }

        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod),
                               QJsonObject{{QStringLiteral("cursor"), QStringLiteral("1")}}, context,
                               QStringLiteral("forged-list-cursor")))
                               .toJson(QJsonDocument::Compact));
        ok &= expect(responses.size() == 1 &&
                         QJsonDocument::fromJson(responses.dequeue())
                                 .object()
                                 .value(QStringLiteral("error"))
                                 .toObject()
                                 .value(QStringLiteral("code"))
                                 .toInt() == AutomationWire::Mcp::InvalidParams,
                     "downstream tools/list must reject unsigned and forged cursors");

        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod),
                               QJsonObject{{QStringLiteral("limit"), 1}}, context,
                               QStringLiteral("invalid-list")))
                               .toJson(QJsonDocument::Compact));
        ok &= expect(responses.size() == 1 &&
                         QJsonDocument::fromJson(responses.dequeue())
                                 .object()
                                 .value(QStringLiteral("error"))
                                 .toObject()
                                 .value(QStringLiteral("code"))
                                 .toInt() == AutomationWire::Mcp::InvalidParams,
                     "tools/list must reject non-standard limit and additional params");

        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod), {},
                               context))
                               .toJson(QJsonDocument::Compact));
        ok &= expect(responses.isEmpty(),
                     "JSON-RPC core notifications must never produce a response");

        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QStringLiteral("notifications/cancelled"),
                               QJsonObject{{QStringLiteral("request_id"),
                                            QStringLiteral("legacy-id")}},
                               context))
                               .toJson(QJsonDocument::Compact));
        ok &= expect(responses.isEmpty(),
                     "invalid and legacy cancellation notifications must remain silent");

        const auto statusCall = AutomationWire::Mcp::makeRequest(
            QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
            QJsonObject{{QStringLiteral("name"), QStringLiteral("connector.get_status")},
                        {QStringLiteral("arguments"), QJsonObject{}}},
            context, QStringLiteral("reusable-sync-id"));
        server.processLine(QJsonDocument(statusCall).toJson(QJsonDocument::Compact));
        server.processLine(QJsonDocument(statusCall).toJson(QJsonDocument::Compact));
        ok &= expect(responses.size() == 2,
                     "synchronous callbacks must not leave a stale in-flight request id");
        responses.clear();

        DsConnector::ConnectorRuntime l2Runtime(DsConnector::ConnectorOptions{
            .exposure = {.profile = AutomationWire::ExposureProfile::L2},
        }, QStringLiteral("DsConnectorLite-No-Such-Editor-L2"));
        DsConnector::DownstreamMcpServer l2Server(&l2Runtime);
        QObject::connect(&l2Server, &DsConnector::DownstreamMcpServer::responseLine, &l2Server,
                         [&responses](const QByteArray &line) { responses.enqueue(line); });
        l2Server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                                 QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod), {},
                                 context, QStringLiteral("l2-list")))
                                 .toJson(QJsonDocument::Compact));
        if (!responses.isEmpty()) {
            const auto result = QJsonDocument::fromJson(responses.dequeue())
                                    .object()
                                    .value(QStringLiteral("result"))
                                    .toObject();
            ok &= expect(result.value(QStringLiteral("tools")).toArray().size() == 93 &&
                             !result.contains(QStringLiteral("nextCursor")),
                         "the frozen L2 surface must fit one downstream tools/list page");
        } else {
            ok &= expect(false, "the frozen L2 downstream list must respond");
        }
        return ok;
    }

    bool verifyUnavailableEditorStates() {
        const auto serviceName = QStringLiteral("DsConnectorLite-State-Test-%1")
                                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        FakeBootstrap bootstrap(serviceName);
        bool ok = expect(bootstrap.listen(), "state fake bootstrap endpoint must listen");
        if (!ok)
            return false;

        DsConnector::ConnectorRuntime runtime(DsConnector::ConnectorOptions{
            .exposure = {.profile = AutomationWire::ExposureProfile::L1},
        }, serviceName);
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
        ok &= expect(callCode() == QStringLiteral("editor_not_running"),
                     "an unavailable editor must report editor_not_running");
        runtime.start();

        const QList<QPair<SingleInstanceAutomationState, QString>> states{
            {SingleInstanceAutomationState::Starting, QStringLiteral("editor_starting")},
            {SingleInstanceAutomationState::McpDisabled, QStringLiteral("mcp_disabled")},
            {SingleInstanceAutomationState::McpStarting, QStringLiteral("mcp_starting")},
            {SingleInstanceAutomationState::McpStopping, QStringLiteral("mcp_stopping")},
            {SingleInstanceAutomationState::Error, QStringLiteral("editor_error")},
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
            ok &= expect(waitUntil([&] {
                             return runtime.status()
                                        .value(QStringLiteral("editor"))
                                        .toObject()
                                        .value(QStringLiteral("state"))
                                        .toString() ==
                                    SingleInstanceProtocol::automationStateName(state);
                         }),
                         "connector must observe each non-ready editor state");
            ok &= expect(callCode() == expectedCode,
                         "typed calls must return the exact non-ready editor state code");
            ok &= expect(runtime.downstreamTools().size() == fixedToolCount,
                         "non-ready editor states must preserve the fixed typed tool surface");
        }
        runtime.stop();
        return ok;
    }

    bool verifyBootstrapCorrelation() {
        const auto serviceName = QStringLiteral("DsConnectorLite-Correlation-%1")
                                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        FakeBootstrap bootstrap(serviceName);
        bool ok = expect(bootstrap.listen(), "correlation fake bootstrap must listen");
        if (!ok)
            return false;
        bootstrap.emptyFirstRequestId = true;
        bootstrap.publish(SingleInstanceAutomationStatus{
            .state = SingleInstanceAutomationState::Starting,
            .editorInstanceId = QStringLiteral("correlation-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
        });
        DsConnector::ConnectorRuntime runtime({}, serviceName);
        runtime.start();
        ok &= expect(waitUntil([&] {
                         return runtime.status()
                             .value(QStringLiteral("bootstrap"))
                             .toObject()
                             .value(QStringLiteral("error"))
                             .toString()
                             .contains(QStringLiteral("bootstrap_request_id_mismatch"));
                     }, 5000),
                     "the first watch snapshot must carry the exact request correlation id");
        runtime.stop();
        return ok;
    }

    bool verifyHandshakeCoordination() {
        bool ok = true;
        const auto options = DsConnector::ConnectorOptions{
            .exposure = {
                .profile = AutomationWire::ExposureProfile::L0,
                .includes = {QStringLiteral("id:application.get_info"),
                             QStringLiteral("id:automation.get_manifest"),
                             QStringLiteral("id:notes.get")},
            },
            .upstreamTimeoutMs = 2000,
        };
        const auto readyStatus = [](const QString &editorId, const QString &endpoint) {
            return SingleInstanceAutomationStatus{
                .state = SingleInstanceAutomationState::McpReady,
                .editorInstanceId = editorId,
                .executablePath = QCoreApplication::applicationFilePath(),
                .applicationVersion = QStringLiteral("test"),
                .buildId = QStringLiteral("fake-build"),
                .hostMode = QStringLiteral("gui"),
                .mcpEnabled = true,
                .mcpEndpoint = endpoint,
            };
        };
        const auto readyRuntime = [](const DsConnector::ConnectorRuntime &runtime,
                                     const int targetCount) {
            const auto status = runtime.status();
            return status.value(QStringLiteral("mcp"))
                           .toObject()
                           .value(QStringLiteral("connected"))
                           .toBool() &&
                   status.value(QStringLiteral("manifest"))
                           .toObject()
                           .value(QStringLiteral("compatibility"))
                           .toString() == QStringLiteral("compatible_subset") &&
                   status.value(QStringLiteral("exposure"))
                           .toObject()
                           .value(QStringLiteral("generic_target_count"))
                           .toInt() == targetCount;
        };

        {
            FakeHttpEditor http;
            ok &= expect(http.listen(), "handshake-coalescing fake editor must listen");
            if (!ok)
                return false;
            http.discoverResponseDelayMs = 300;
            const auto serviceName = QStringLiteral("DsConnectorLite-Handshake-Coalescing-%1")
                                         .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
            FakeBootstrap bootstrap(serviceName);
            ok &= expect(bootstrap.listen(), "handshake-coalescing bootstrap must listen");
            if (!ok)
                return false;
            const auto ready = readyStatus(QStringLiteral("coalescing-editor"), http.endpoint());
            bootstrap.publish(ready);
            DsConnector::ConnectorRuntime runtime(options, serviceName);
            runtime.start();
            ok &= expect(waitUntil([&] {
                             return bootstrap.watcherCount() == 1 && http.discoverCount == 1;
                         }, 5000),
                         "the initial delayed handshake must be in flight before the ready burst");
            for (auto index = 0; index < 64; ++index)
                bootstrap.publish(ready);
            ok &= expect(waitUntil([&] {
                             return http.discoverCount >= 2 && readyRuntime(runtime, 2);
                         }, 10000),
                         "duplicate ready snapshots must converge after one trailing refresh");
            const auto unexpectedExtraHandshake =
                waitUntil([&] { return http.discoverCount > 2; }, 700);
            ok &= expect(!unexpectedExtraHandshake && http.discoverCount == 2 &&
                             http.toolsListCount == 2 && http.manifestCallCount == 2 &&
                             http.requestIds.size() == 6 && http.requestIds.size() < 20,
                         "a same-target ready burst must stay below the default client request budget");

            http.discoverResponseDelayMs = 0;
            http.exposeNotes = true;
            const auto refreshCount = http.discoverCount;
            bootstrap.publish(ready);
            ok &= expect(waitUntil([&] {
                             return http.discoverCount == refreshCount + 1 &&
                                    readyRuntime(runtime, 3);
                         }, 10000),
                         "a later same-endpoint ready snapshot must still refresh new tools");
            runtime.stop();
        }

        {
            FakeHttpEditor http;
            ok &= expect(http.listen(), "handshake-retry fake editor must listen");
            if (!ok)
                return false;
            http.discoverRateLimitFailuresRemaining = 1;
            const auto serviceName = QStringLiteral("DsConnectorLite-Handshake-Retry-%1")
                                         .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
            FakeBootstrap bootstrap(serviceName);
            ok &= expect(bootstrap.listen(), "handshake-retry bootstrap must listen");
            if (!ok)
                return false;
            const auto ready = readyStatus(QStringLiteral("retry-editor"), http.endpoint());
            bootstrap.publish(ready);
            DsConnector::ConnectorRuntime runtime(options, serviceName);
            runtime.start();
            ok &= expect(waitUntil([&] {
                             return http.discoverCount == 2 && readyRuntime(runtime, 2);
                         }, 10000),
                         "a first HTTP 429 handshake response must recover automatically");

            http.discoverRateLimitFailuresRemaining = 5;
            bootstrap.publish(ready);
            ok &= expect(waitUntil([&] {
                             const auto status = runtime.status();
                             return http.discoverCount == 7 &&
                                    status.value(QStringLiteral("mcp"))
                                            .toObject()
                                            .value(QStringLiteral("error"))
                                            .toString() == QStringLiteral("too_many_requests") &&
                                    status.value(QStringLiteral("manifest"))
                                            .toObject()
                                            .value(QStringLiteral("compatibility"))
                                            .toString() == QStringLiteral("not_loaded");
                         }, 10000),
                         "transient handshake retries must stop after the bounded backoff budget");
            ok &= expect(!waitUntil([&] { return http.discoverCount > 7; }, 1000),
                         "an exhausted handshake must not retry forever without a new event");

            http.discoverRateLimitFailuresRemaining = 1;
            bootstrap.publish(ready);
            ok &= expect(waitUntil([&] {
                             return http.discoverCount == 9 && readyRuntime(runtime, 2);
                         }, 10000),
                         "a later external ready event must receive a fresh bounded retry budget");
            runtime.stop();
        }
        return ok;
    }

    bool verifyFakeEditorIntegration() {
        FakeHttpEditor http;
        bool ok = expect(http.listen(), "fake editor HTTP endpoint must listen");
        if (!ok)
            return false;
        http.exposeFilteredTool = true;

        const auto serviceName = QStringLiteral("DsConnectorLite-Test-%1")
                                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        FakeBootstrap bootstrap(serviceName);
        ok &= expect(bootstrap.listen(), "fake editor bootstrap endpoint must listen");
        if (!ok)
            return false;

        SingleInstanceAutomationStatus ready{
            .state = SingleInstanceAutomationState::McpReady,
            .editorInstanceId = QUuid::createUuid().toString(QUuid::WithoutBraces),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
            .mcpEnabled = true,
            .mcpEndpoint = http.endpoint(),
        };
        bootstrap.publish(ready);

        DsConnector::ConnectorOptions options{
            .exposure = {
                .profile = AutomationWire::ExposureProfile::L0,
                .includes = {
                    QStringLiteral("id:application.get_info"),
                    QStringLiteral("id:automation.get_manifest"),
                    QStringLiteral("id:notes.get"),
                },
            },
            .upstreamTimeoutMs = 2000,
        };
        DsConnector::ConnectorRuntime runtime(options, serviceName);
        runtime.start();
        const auto handshakeReady = waitUntil([&] {
                         const auto status = runtime.status();
                         return status.value(QStringLiteral("mcp")).toObject().value(
                                    QStringLiteral("connected"))
                                    .toBool() &&
                                status.value(QStringLiteral("exposure"))
                                        .toObject()
                                        .value(QStringLiteral("generic_target_count"))
                                        .toInt() == 2 &&
                                status.value(QStringLiteral("manifest"))
                                        .toObject()
                                        .value(QStringLiteral("compatibility"))
                                        .toString() != QStringLiteral("not_loaded") &&
                                status.value(QStringLiteral("manifest"))
                                        .toObject()
                                        .value(QStringLiteral("compatibility"))
                                        .toString() != QStringLiteral("refreshing");
                     }, 10000);
        if (!handshakeReady)
            QTextStream(stderr) << QJsonDocument(runtime.status()).toJson(QJsonDocument::Compact)
                                << Qt::endl;
        ok &= expect(handshakeReady,
                     "connector must discover/watch and complete the fake editor MCP handshake");
        const auto connectedStatus = runtime.status();
        ok &= expect(bootstrap.validWatchRequest && http.headersValid,
                     "bootstrap watch identity and modern HTTP MCP headers must be valid");
        ok &= expect(connectedStatus.value(QStringLiteral("editor"))
                             .toObject()
                             .value(QStringLiteral("editor_instance_id"))
                             .toString() == ready.editorInstanceId,
                     "connector status must report the actual editor identity");
        const auto manifestCompatibility = connectedStatus.value(QStringLiteral("manifest"))
                                               .toObject()
                                               .value(QStringLiteral("compatibility"))
                                               .toString();
        if (manifestCompatibility != QStringLiteral("compatible_subset"))
            QTextStream(stderr) << "Observed compatibility: " << manifestCompatibility << Qt::endl;
        ok &= expect(manifestCompatibility == QStringLiteral("compatible_subset"),
                     "connector status must report the manifest compatibility subset");

        DsConnector::DownstreamMcpServer server(&runtime);
        QQueue<QByteArray> responses;
        QObject::connect(&server, &DsConnector::DownstreamMcpServer::responseLine, &server,
                         [&responses](const QByteArray &line) { responses.enqueue(line); });
        const auto context = clientContext();
        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                               QJsonObject{{QStringLiteral("name"),
                                            QStringLiteral("application.get_info")},
                                           {QStringLiteral("arguments"), QJsonObject{}}},
                               context, QStringLiteral("downstream-call-id")))
                               .toJson(QJsonDocument::Compact));
        ok &= expect(waitUntil([&] { return !responses.isEmpty(); }),
                     "typed downstream call must complete through fake editor HTTP");
        if (!responses.isEmpty()) {
            const auto response = QJsonDocument::fromJson(responses.dequeue()).object();
            const auto structured = response.value(QStringLiteral("result"))
                                        .toObject()
                                        .value(QStringLiteral("structuredContent"))
                                        .toObject();
            ok &= expect(response.value(QStringLiteral("id")).toString() ==
                             QStringLiteral("downstream-call-id") &&
                             structured.value(QStringLiteral("name")).toString() ==
                                 QStringLiteral("DS Editor Lite"),
                         "connector must restore the downstream ID and preserve structured result");
        }
        ok &= expect(std::none_of(http.requestIds.constBegin(), http.requestIds.constEnd(),
                                  [](const QJsonValue &id) {
                                      return id.toString() == QStringLiteral("downstream-call-id");
                                  }),
                     "connector must allocate independent upstream request IDs");

        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                               QJsonObject{
                                   {QStringLiteral("name"), QStringLiteral("editor.tools.invoke")},
                                   {QStringLiteral("arguments"),
                                    QJsonObject{
                                        {QStringLiteral("name"),
                                         QStringLiteral("application.get_info")},
                                        {QStringLiteral("arguments"), QJsonObject{}},
                                    }},
                               },
                               context, QStringLiteral("generic-call-id")))
                               .toJson(QJsonDocument::Compact));
        ok &= expect(waitUntil([&] { return !responses.isEmpty(); }),
                     "generic invoke must use the same permitted editor target");
        if (!responses.isEmpty()) {
            const auto response = QJsonDocument::fromJson(responses.dequeue()).object();
            ok &= expect(!response.value(QStringLiteral("result"))
                              .toObject()
                              .value(QStringLiteral("isError"))
                              .toBool(),
                         "generic invoke must succeed for an exposure-permitted target");
        }

        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                               QJsonObject{
                                   {QStringLiteral("name"), QStringLiteral("editor.tools.list")},
                                   {QStringLiteral("arguments"), QJsonObject{}},
                               },
                               context, QStringLiteral("generic-list")))
                               .toJson(QJsonDocument::Compact));
        ok &= expect(!responses.isEmpty(), "generic list must complete synchronously");
        if (!responses.isEmpty()) {
            const auto tools = QJsonDocument::fromJson(responses.dequeue())
                                   .object()
                                   .value(QStringLiteral("result"))
                                   .toObject()
                                   .value(QStringLiteral("structuredContent"))
                                   .toObject()
                                   .value(QStringLiteral("tools"))
                                   .toArray();
            bool containsFiltered = false;
            for (const auto &entry : tools) {
                containsFiltered |= entry.toObject().value(QStringLiteral("name")).toString() ==
                                    QStringLiteral("project.get");
            }
            ok &= expect(tools.size() == 2 && !containsFiltered,
                         "generic list must use the same connector exposure as typed tools");
        }

        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                               QJsonObject{
                                   {QStringLiteral("name"),
                                    QStringLiteral("editor.tools.search")},
                                   {QStringLiteral("arguments"),
                                    QJsonObject{{QStringLiteral("query"),
                                                 QStringLiteral("project.get")}}},
                               },
                               context, QStringLiteral("filtered-search")))
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
            ok &= expect(tools.isEmpty(),
                         "generic search must not reveal exposure-filtered editor targets");
        } else {
            ok &= expect(false, "filtered generic search must return a result");
        }

        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                               QJsonObject{
                                   {QStringLiteral("name"),
                                    QStringLiteral("editor.tools.describe")},
                                   {QStringLiteral("arguments"),
                                    QJsonObject{{QStringLiteral("name"),
                                                 QStringLiteral("project.get")}}},
                               },
                               context, QStringLiteral("filtered-describe")))
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
            ok &= expect(code == QStringLiteral("connector_tool_filtered"),
                         "generic describe must not bypass connector exposure");
        } else {
            ok &= expect(false, "filtered generic describe must return a result");
        }

        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                               QJsonObject{
                                   {QStringLiteral("name"), QStringLiteral("editor.tools.invoke")},
                                   {QStringLiteral("arguments"),
                                    QJsonObject{
                                        {QStringLiteral("name"), QStringLiteral("project.get")},
                                        {QStringLiteral("arguments"), QJsonObject{}},
                                    }},
                               },
                               context, QStringLiteral("filtered-invoke")))
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
            ok &= expect(code == QStringLiteral("connector_tool_filtered") &&
                             !http.calledTools.contains(QStringLiteral("project.get")),
                         "generic invoke must not bypass connector exposure");
        } else {
            ok &= expect(false, "filtered generic invoke must return a result");
        }

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Hold;
        const auto heldRequest = AutomationWire::Mcp::makeRequest(
            QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
            QJsonObject{{QStringLiteral("name"), QStringLiteral("application.get_info")},
                        {QStringLiteral("arguments"), QJsonObject{}}},
            context, QStringLiteral("cancel-me"));
        server.processLine(QJsonDocument(heldRequest).toJson(QJsonDocument::Compact));
        server.processLine(QJsonDocument(heldRequest).toJson(QJsonDocument::Compact));
        ok &= expect(!responses.isEmpty() &&
                         QJsonDocument::fromJson(responses.dequeue())
                                 .object()
                                 .value(QStringLiteral("error"))
                                 .toObject()
                                 .value(QStringLiteral("code"))
                                 .toInt() == AutomationWire::Mcp::InvalidRequest,
                     "duplicate in-flight downstream request IDs must be rejected");
        ok &= expect(waitUntil([&] {
                         return http.calledTools.count(QStringLiteral("application.get_info")) >= 3;
                     }),
                     "cancellation test request must reach the fake editor");
        server.processLine(QJsonDocument(QJsonObject{
                               {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                               {QStringLiteral("method"),
                                QStringLiteral("notifications/cancelled")},
                               {QStringLiteral("params"),
                                QJsonObject{{QStringLiteral("requestId"),
                                             QStringLiteral("cancel-me")}}},
                           })
                               .toJson(QJsonDocument::Compact));
        ok &= expect(waitUntil([&] {
                         return runtime.status()
                                    .value(QStringLiteral("mcp"))
                                    .toObject()
                                    .value(QStringLiteral("pending_request_count"))
                                    .toInt() == 0;
                     }) &&
                         responses.isEmpty(),
                     "standard cancellation without _meta must abort upstream and emit no response");
        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Success;

        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QStringLiteral("notifications/cancelled"),
                               QJsonObject{{QStringLiteral("requestId"),
                                            QStringLiteral("cancel-me")}},
                               context))
                               .toJson(QJsonDocument::Compact));
        ok &= expect(responses.isEmpty(),
                     "cancelling an already completed request must not emit another response");

        const auto sendApplication = [&](const QString &id) {
            server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                                   QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                                   QJsonObject{{QStringLiteral("name"),
                                                QStringLiteral("application.get_info")},
                                               {QStringLiteral("arguments"), QJsonObject{}}},
                                   context, id))
                                   .toJson(QJsonDocument::Compact));
        };

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Sse;
        sendApplication(QStringLiteral("sse-call"));
        ok &= expect(waitUntil([&] { return !responses.isEmpty(); }),
                     "CRLF multi-event and multi-data-line SSE must complete");
        if (!responses.isEmpty()) {
            const auto response = QJsonDocument::fromJson(responses.dequeue()).object();
            ok &= expect(response.value(QStringLiteral("id")) == QStringLiteral("sse-call") &&
                             !response.value(QStringLiteral("result"))
                                  .toObject()
                                  .value(QStringLiteral("isError"))
                                  .toBool(),
                         "SSE transport must preserve the original downstream request ID");
        }

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::BusinessError;
        sendApplication(QStringLiteral("business-error"));
        ok &= expect(waitUntil([&] { return !responses.isEmpty(); }),
                     "business CallToolResult errors must complete");
        if (!responses.isEmpty()) {
            const auto result = QJsonDocument::fromJson(responses.dequeue())
                                    .object()
                                    .value(QStringLiteral("result"))
                                    .toObject();
            ok &= expect(result.value(QStringLiteral("isError")).toBool() &&
                             result.value(QStringLiteral("structuredContent"))
                                     .toObject()
                                     .value(QStringLiteral("code")) ==
                                 QStringLiteral("fake_business_error"),
                         "business CallToolResult errors must pass through unchanged");
        }

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::ProtocolError;
        sendApplication(QStringLiteral("protocol-error"));
        ok &= expect(waitUntil([&] { return !responses.isEmpty(); }),
                     "upstream JSON-RPC errors must complete");
        if (!responses.isEmpty()) {
            const auto response = QJsonDocument::fromJson(responses.dequeue()).object();
            ok &= expect(response.value(QStringLiteral("id")) ==
                             QStringLiteral("protocol-error") &&
                             response.value(QStringLiteral("error"))
                                     .toObject()
                                     .value(QStringLiteral("code"))
                                     .toInt() == AutomationWire::Mcp::InvalidParams,
                         "upstream JSON-RPC errors must map to the original downstream ID");
        }

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::InvalidOutput;
        sendApplication(QStringLiteral("invalid-output"));
        const auto invalidOutput =
            takeResponseById(responses, QStringLiteral("invalid-output"), 5000);
        ok &= expect(invalidOutput.has_value(),
                     "an upstream output-schema violation must complete downstream");
        if (invalidOutput) {
            const auto result = invalidOutput->value(QStringLiteral("result")).toObject();
            const auto structured = result.value(QStringLiteral("structuredContent")).toObject();
            ok &= expect(result.value(QStringLiteral("isError")).toBool() &&
                             structured.value(QStringLiteral("code")) ==
                                 QStringLiteral("invalid_upstream_output") &&
                             !QJsonDocument(*invalidOutput)
                                  .toJson(QJsonDocument::Compact)
                                  .contains("must-not-pass"),
                         "invalid upstream output must become a complete non-leaking CallToolResult");
        }

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Redirect;
        sendApplication(QStringLiteral("redirect-call"));
        const auto redirect = takeResponseById(responses, QStringLiteral("redirect-call"), 5000);
        ok &= expect(redirect.has_value() &&
                         redirect->value(QStringLiteral("result"))
                                 .toObject()
                                 .value(QStringLiteral("structuredContent"))
                                 .toObject()
                                 .value(QStringLiteral("code")) ==
                             QStringLiteral("upstream_redirect_rejected"),
                     "HTTP redirects must be rejected even when their body resembles JSON-RPC");

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Hold;
        sendApplication(QStringLiteral("timeout-call"));
        ok &= expect(waitUntil([&] { return !responses.isEmpty(); }, 5000),
                     "upstream timeout must complete under the test watchdog");
        if (!responses.isEmpty()) {
            const auto structured = QJsonDocument::fromJson(responses.dequeue())
                                        .object()
                                        .value(QStringLiteral("result"))
                                        .toObject()
                                        .value(QStringLiteral("structuredContent"))
                                        .toObject();
            ok &= expect(structured.value(QStringLiteral("code")) ==
                             QStringLiteral("upstream_timeout") &&
                             structured.value(QStringLiteral("message")) ==
                                 QStringLiteral("upstream_timeout"),
                         "query timeout must preserve the stable upstream_timeout code");
        }
        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Success;

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Oversized;
        sendApplication(QStringLiteral("oversized-call"));
        ok &= expect(waitUntil([&] { return !responses.isEmpty(); }, 10000),
                     "oversized upstream response must be aborted incrementally");
        if (!responses.isEmpty()) {
            const auto structured = QJsonDocument::fromJson(responses.dequeue())
                                        .object()
                                        .value(QStringLiteral("result"))
                                        .toObject()
                                        .value(QStringLiteral("structuredContent"))
                                        .toObject();
            ok &= expect(structured.value(QStringLiteral("message")) ==
                             QStringLiteral("upstream_response_too_large"),
                         "upstream body hard limit must report upstream_response_too_large");
        }
        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Success;

        const auto fixedToolCount = runtime.downstreamTools().size();
        const auto policyRefreshCount = http.toolsListCount;
        http.applicationAvailability = QStringLiteral("profile_disabled");
        bootstrap.publish(ready);
        ok &= expect(waitUntil([&] {
                         return http.toolsListCount > policyRefreshCount &&
                                runtime.status()
                                        .value(QStringLiteral("manifest"))
                                        .toObject()
                                        .value(QStringLiteral("compatibility"))
                                        .toString() != QStringLiteral("refreshing");
                     }, 10000),
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
        runtime.callTool(
            QStringLiteral("editor.tools.list"),
            QJsonObject{{QStringLiteral("cursor"), QStringLiteral("1")}},
            [&forgedCursorCode](const DsConnector::ToolCallOutcome &outcome) {
                forgedCursorCode = outcome.result.value(QStringLiteral("structuredContent"))
                                       .toObject()
                                       .value(QStringLiteral("code"))
                                       .toString();
            });
        ok &= expect(forgedCursorCode == QStringLiteral("invalid_cursor"),
                     "generic list must reject forged numeric cursors");
        const auto policyListContainsApplication = std::any_of(
            policyList.constBegin(), policyList.constEnd(), [](const QJsonValue &entry) {
                return entry.toObject().value(QStringLiteral("name")) ==
                       QStringLiteral("application.get_info");
            });
        QString policyDescribeCode;
        QJsonArray policySearch;
        runtime.callTool(
            QStringLiteral("editor.tools.search"),
            QJsonObject{{QStringLiteral("query"), QStringLiteral("application.get_info")}},
            [&policySearch](const DsConnector::ToolCallOutcome &outcome) {
                policySearch = outcome.result.value(QStringLiteral("structuredContent"))
                                   .toObject()
                                   .value(QStringLiteral("tools"))
                                   .toArray();
            });
        runtime.callTool(
            QStringLiteral("editor.tools.describe"),
            QJsonObject{{QStringLiteral("name"), QStringLiteral("application.get_info")}},
            [&policyDescribeCode](const DsConnector::ToolCallOutcome &outcome) {
                policyDescribeCode = outcome.result.value(QStringLiteral("structuredContent"))
                                         .toObject()
                                         .value(QStringLiteral("code"))
                                         .toString();
            });
        ok &= expect(!policyListContainsApplication && policySearch.isEmpty() &&
                         policyDescribeCode == QStringLiteral("profile_disabled"),
                     "generic list, search, and describe must enforce editor availability policy");
        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                               QJsonObject{
                                   {QStringLiteral("name"), QStringLiteral("editor.tools.invoke")},
                                   {QStringLiteral("arguments"),
                                    QJsonObject{
                                        {QStringLiteral("name"),
                                         QStringLiteral("application.get_info")},
                                        {QStringLiteral("arguments"), QJsonObject{}},
                                    }},
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
            ok &= expect(code == QStringLiteral("profile_disabled") &&
                             http.calledTools.size() == callsBeforePolicyReject,
                         "generic invoke must not bypass actual editor availability policy");
        } else {
            ok &= expect(false, "editor-policy-rejected generic invoke must return a result");
        }
        http.applicationAvailability.clear();
        http.exposeNotes = true;
        http.incompatibleApplicationSchema = true;
        bootstrap.publish(ready);
        ok &= expect(waitUntil([&] {
                         return runtime.status()
                                    .value(QStringLiteral("exposure"))
                                    .toObject()
                                    .value(QStringLiteral("generic_target_count"))
                                    .toInt() == 3 &&
                                http.toolsListCount >= 2 &&
                                runtime.status()
                                        .value(QStringLiteral("manifest"))
                                        .toObject()
                                        .value(QStringLiteral("compatibility"))
                                        .toString() != QStringLiteral("refreshing");
                     }, 10000),
                     "a new ready watch snapshot must refresh same-endpoint editor tools");
        ok &= expect(runtime.downstreamTools().size() == fixedToolCount,
                     "same-endpoint refresh must not mutate the fixed typed downstream set");

        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                               QJsonObject{{QStringLiteral("name"),
                                            QStringLiteral("application.get_info")},
                                           {QStringLiteral("arguments"), QJsonObject{}}},
                               context, QStringLiteral("incompatible-typed")))
                               .toJson(QJsonDocument::Compact));
        const auto incompatibleTyped =
            takeResponseById(responses, QStringLiteral("incompatible-typed"));
        ok &= expect(incompatibleTyped.has_value(),
                     "incompatible typed wrapper must fail without an upstream call");
        if (incompatibleTyped) {
            const auto structured = incompatibleTyped->value(QStringLiteral("result"))
                                        .toObject()
                                        .value(QStringLiteral("structuredContent"))
                                        .toObject();
            ok &= expect(structured.value(QStringLiteral("code")).toString() ==
                             QStringLiteral("contract_incompatible"),
                         "typed schema mismatch must report contract_incompatible");
        }

        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                               QJsonObject{
                                   {QStringLiteral("name"), QStringLiteral("editor.tools.invoke")},
                                   {QStringLiteral("arguments"),
                                    QJsonObject{
                                        {QStringLiteral("name"),
                                         QStringLiteral("application.get_info")},
                                        {QStringLiteral("arguments"), QJsonObject{}},
                                    }},
                               },
                               context, QStringLiteral("incompatible-generic")))
                               .toJson(QJsonDocument::Compact));
        ok &= expect(waitUntil([&] { return !responses.isEmpty(); }),
                     "generic invoke must remain available after typed schema mismatch");
        if (!responses.isEmpty()) {
            const auto result = QJsonDocument::fromJson(responses.dequeue())
                                    .object()
                                    .value(QStringLiteral("result"))
                                    .toObject();
            ok &= expect(result.value(QStringLiteral("isError")).toBool() &&
                             result.value(QStringLiteral("structuredContent"))
                                     .toObject()
                                     .value(QStringLiteral("code")) ==
                                 QStringLiteral("invalid_upstream_output"),
                         "generic invoke must enforce the editor actual output schema");
        }

        auto disabled = ready;
        disabled.state = SingleInstanceAutomationState::McpDisabled;
        disabled.mcpEnabled = false;
        disabled.mcpEndpoint.clear();
        bootstrap.publish(disabled);
        ok &= expect(waitUntil([&] {
                         return !runtime.status()
                                     .value(QStringLiteral("mcp"))
                                     .toObject()
                                     .value(QStringLiteral("connected"))
                                     .toBool();
                     }),
                     "connector must drop only the upstream MCP state when editor disables MCP");
        ok &= expect(runtime.downstreamTools().size() == fixedToolCount,
                     "editor state changes must not mutate the fixed downstream tool list");
        runtime.stop();
        return ok;
    }

    bool verifyForwardCompatibleGenericTools() {
        FakeHttpEditor http;
        bool ok = expect(http.listen(), "forward-compatible fake editor must listen");
        if (!ok)
            return false;
        http.exposeForwardCompatibleTools = true;

        const auto serviceName = QStringLiteral("DsConnectorLite-Forward-%1")
                                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        FakeBootstrap bootstrap(serviceName);
        ok &= expect(bootstrap.listen(), "forward-compatible bootstrap must listen");
        if (!ok)
            return false;
        bootstrap.publish(SingleInstanceAutomationStatus{
            .state = SingleInstanceAutomationState::McpReady,
            .editorInstanceId = QStringLiteral("forward-compatible-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
            .mcpEnabled = true,
            .mcpEndpoint = http.endpoint(),
        });

        DsConnector::ConnectorRuntime runtime(DsConnector::ConnectorOptions{
            .exposure = {
                .profile = AutomationWire::ExposureProfile::L0,
                .includes = {QStringLiteral("id:fake.flexible_output"),
                             QStringLiteral("id:fake.minimal")},
            },
            .upstreamTimeoutMs = 2000,
        }, serviceName);
        runtime.start();
        const auto ready = waitUntil([&] {
            const auto status = runtime.status();
            return status.value(QStringLiteral("mcp"))
                           .toObject()
                           .value(QStringLiteral("connected"))
                           .toBool() &&
                   status.value(QStringLiteral("exposure"))
                           .toObject()
                           .value(QStringLiteral("generic_target_count"))
                           .toInt() == 2 &&
                   status.value(QStringLiteral("manifest"))
                           .toObject()
                           .value(QStringLiteral("compatibility"))
                           .toString() != QStringLiteral("refreshing");
        }, 10000);
        if (!ready)
            QTextStream(stderr) << QJsonDocument(runtime.status()).toJson(QJsonDocument::Compact)
                                << Qt::endl;
        ok &= expect(ready,
                     "optional and additive upstream tool descriptor fields must not fail handshake");

        DsConnector::DownstreamMcpServer server(&runtime);
        QQueue<QByteArray> responses;
        QObject::connect(&server, &DsConnector::DownstreamMcpServer::responseLine, &server,
                         [&responses](const QByteArray &line) { responses.enqueue(line); });
        const auto context = clientContext();
        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                               QJsonObject{{QStringLiteral("name"),
                                            QStringLiteral("editor.tools.list")},
                                           {QStringLiteral("arguments"), QJsonObject{}}},
                               context, QStringLiteral("forward-list")))
                               .toJson(QJsonDocument::Compact));
        const auto listResponse = takeResponseById(responses, QStringLiteral("forward-list"));
        QJsonObject minimalDescriptor;
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
            }
            ok &= expect(!result.value(QStringLiteral("isError")).toBool() && tools.size() == 2 &&
                             !minimalDescriptor.isEmpty() &&
                             !minimalDescriptor.contains(QStringLiteral("title")) &&
                             !minimalDescriptor.contains(QStringLiteral("description")) &&
                             !minimalDescriptor.contains(QStringLiteral("outputSchema")) &&
                             !minimalDescriptor.contains(QStringLiteral("annotations")) &&
                             minimalDescriptor.value(QStringLiteral("icons")).isArray() &&
                             minimalDescriptor.value(QStringLiteral("_meta")).isObject(),
                         "generic list must preserve standard optional metadata without inventing "
                         "omitted fields");
        } else {
            ok &= expect(false, "forward-compatible generic list must return a result");
        }

        server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                               QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                               QJsonObject{
                                   {QStringLiteral("name"),
                                    QStringLiteral("editor.tools.describe")},
                                   {QStringLiteral("arguments"),
                                    QJsonObject{{QStringLiteral("name"),
                                                 QStringLiteral("fake.minimal")}}},
                               },
                               context, QStringLiteral("forward-describe")))
                               .toJson(QJsonDocument::Compact));
        const auto describeResponse =
            takeResponseById(responses, QStringLiteral("forward-describe"));
        if (describeResponse) {
            const auto result = describeResponse->value(QStringLiteral("result")).toObject();
            const auto structured = result.value(QStringLiteral("structuredContent")).toObject();
            ok &= expect(!result.value(QStringLiteral("isError")).toBool() &&
                             !structured.contains(QStringLiteral("output_schema")) &&
                             structured.value(QStringLiteral("tool"))
                                 .toObject()
                                 .value(QStringLiteral("icons"))
                                 .isArray(),
                         "generic describe must support a conforming tool without outputSchema");
        } else {
            ok &= expect(false, "forward-compatible generic describe must return a result");
        }

        const auto invoke = [&](const QString &shape, const QString &id) {
            server.processLine(QJsonDocument(AutomationWire::Mcp::makeRequest(
                                   QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                                   QJsonObject{
                                       {QStringLiteral("name"),
                                        QStringLiteral("editor.tools.invoke")},
                                       {QStringLiteral("arguments"),
                                        QJsonObject{
                                            {QStringLiteral("name"),
                                             QStringLiteral("fake.flexible_output")},
                                            {QStringLiteral("arguments"),
                                             QJsonObject{{QStringLiteral("shape"), shape}}},
                                        }},
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
        ok &= expect(scalar &&
                         !scalar->value(QStringLiteral("result"))
                              .toObject()
                              .value(QStringLiteral("isError"))
                              .toBool() &&
                         structured(scalar).isString() && array && structured(array).isArray() &&
                         null && structured(null).isNull(),
                     "generic invoke must preserve valid scalar, array, and null structuredContent");
        ok &= expect(invalid &&
                         invalid->value(QStringLiteral("result"))
                                 .toObject()
                                 .value(QStringLiteral("isError"))
                                 .toBool() &&
                         structured(invalid)
                                 .toObject()
                                 .value(QStringLiteral("code")) ==
                             QStringLiteral("invalid_upstream_output"),
                     "generic invoke must validate object results against the target outputSchema");
        runtime.stop();
        return ok;
    }

    bool verifyCompatibilityVersions() {
        FakeHttpEditor http;
        bool ok = expect(http.listen(), "compatibility fake editor must listen");
        if (!ok)
            return false;
        http.manifestToolsetVersion = 2;
        http.applicationVersion = 2;
        http.applicationMinimumCompatibleVersion = 1;

        const auto serviceName = QStringLiteral("DsConnectorLite-Compatibility-%1")
                                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        FakeBootstrap bootstrap(serviceName);
        ok &= expect(bootstrap.listen(), "compatibility fake bootstrap must listen");
        if (!ok)
            return false;
        SingleInstanceAutomationStatus ready{
            .state = SingleInstanceAutomationState::McpReady,
            .editorInstanceId = QStringLiteral("compatibility-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
            .mcpEnabled = true,
            .mcpEndpoint = http.endpoint(),
        };
        bootstrap.publish(ready);

        DsConnector::ConnectorRuntime runtime(DsConnector::ConnectorOptions{
            .exposure = {
                .profile = AutomationWire::ExposureProfile::L0,
                .includes = {QStringLiteral("id:application.get_info")},
            },
            .upstreamTimeoutMs = 2000,
        }, serviceName);
        runtime.start();
        const auto compatibility = [&] {
            return runtime.status()
                .value(QStringLiteral("manifest"))
                .toObject()
                .value(QStringLiteral("compatibility"))
                .toString();
        };
        ok &= expect(waitUntil([&] {
                         return compatibility() == QStringLiteral("compatible_subset");
                     }, 10000),
                     "a newer backward-compatible editor contract must be a compatible subset");

        auto refreshCount = http.toolsListCount;
        http.applicationMinimumCompatibleVersion = 2;
        bootstrap.publish(ready);
        ok &= expect(waitUntil([&] {
                         return http.toolsListCount > refreshCount &&
                                compatibility() == QStringLiteral("contract_incompatible");
                     }, 10000),
                     "a newer editor requiring version 2 must be contract_incompatible");
        QString incompatibleCode;
        runtime.callTool(QStringLiteral("application.get_info"), {},
                         [&incompatibleCode](const DsConnector::ToolCallOutcome &outcome) {
                             incompatibleCode =
                                 outcome.result.value(QStringLiteral("structuredContent"))
                                     .toObject()
                                     .value(QStringLiteral("code"))
                                     .toString();
                         });
        ok &= expect(incompatibleCode == QStringLiteral("contract_incompatible"),
                     "newer incompatible typed tools must fail before forwarding");

        refreshCount = http.toolsListCount;
        http.applicationVersion = 1;
        http.applicationMinimumCompatibleVersion = 1;
        http.manifestToolsetVersion = 1;
        bootstrap.publish(ready);
        ok &= expect(waitUntil([&] {
                         return http.toolsListCount > refreshCount &&
                                compatibility() == QStringLiteral("compatible_subset");
                     }, 10000),
                     "the oldest supported editor contract must remain a compatible subset");
        runtime.stop();
        return ok;
    }

    bool verifyParameterHeaders() {
        FakeHttpEditor http;
        bool ok = expect(http.listen(), "parameter-header fake editor must listen");
        if (!ok)
            return false;
        http.annotatedApplicationHeaders = true;
        http.exposeInvalidAnnotatedTool = true;

        const auto serviceName = QStringLiteral("DsConnectorLite-Headers-%1")
                                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        FakeBootstrap bootstrap(serviceName);
        ok &= expect(bootstrap.listen(), "parameter-header bootstrap must listen");
        if (!ok)
            return false;
        const SingleInstanceAutomationStatus ready{
            .state = SingleInstanceAutomationState::McpReady,
            .editorInstanceId = QStringLiteral("parameter-header-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
            .mcpEnabled = true,
            .mcpEndpoint = http.endpoint(),
        };
        bootstrap.publish(ready);

        DsConnector::ConnectorRuntime runtime(DsConnector::ConnectorOptions{
            .exposure = {
                .profile = AutomationWire::ExposureProfile::L0,
                .includes = {QStringLiteral("id:application.get_info"),
                             QStringLiteral("id:fake.invalid_header")},
            },
            .upstreamTimeoutMs = 2000,
        }, serviceName);
        runtime.start();
        ok &= expect(waitUntil([&] {
                         const auto status = runtime.status();
                         return status.value(QStringLiteral("manifest"))
                                    .toObject()
                                    .value(QStringLiteral("compatibility"))
                                    .toString() != QStringLiteral("refreshing") &&
                                status.value(QStringLiteral("exposure"))
                                        .toObject()
                                        .value(QStringLiteral("generic_target_count"))
                                        .toInt() == 1;
                     }, 10000),
                     "invalid x-mcp-header tools must be excluded while valid tools remain");

        const auto arguments = QJsonObject{
            {QStringLiteral("route"), QStringLiteral("华东 路由")},
            {QStringLiteral("retry"), 7},
            {QStringLiteral("enabled"), true},
            {QStringLiteral("nested"),
             QJsonObject{{QStringLiteral("region"), QStringLiteral("west")}}},
        };
        bool completed = false;
        QJsonObject result;
        runtime.callTool(
            QStringLiteral("editor.tools.invoke"),
            QJsonObject{{QStringLiteral("name"), QStringLiteral("application.get_info")},
                        {QStringLiteral("arguments"), arguments}},
            [&](const DsConnector::ToolCallOutcome &outcome) {
                completed = true;
                result = outcome.result;
            });
        ok &= expect(waitUntil([&] { return completed; }, 5000) &&
                         !result.value(QStringLiteral("isError")).toBool(),
                     "an annotated generic invocation must complete through HTTP");

        const auto decoded = [&](const QByteArray &name) {
            QString error;
            const auto value = AutomationWire::Mcp::decodeHeaderValue(
                QString::fromUtf8(http.lastParameterHeaders.value(name)), &error);
            ok &= expect(value.has_value() && error.isEmpty(),
                         "Mcp-Param values must use the standard reversible encoding");
            return value.value_or(QString());
        };
        ok &= expect(decoded("mcp-param-route") == QStringLiteral("华东 路由") &&
                         decoded("mcp-param-retry") == QStringLiteral("7") &&
                         decoded("mcp-param-enabled") == QStringLiteral("true") &&
                         decoded("mcp-param-region") == QStringLiteral("west") &&
                         http.lastParameterHeaders.value("mcp-param-route") !=
                             QStringLiteral("华东 路由").toUtf8(),
                     "primitive and nested x-mcp-header values must be extracted and encoded");

        const auto callsBeforeInvalid = http.calledTools.size();
        QString invalidCode;
        runtime.callTool(
            QStringLiteral("editor.tools.invoke"),
            QJsonObject{{QStringLiteral("name"), QStringLiteral("application.get_info")},
                        {QStringLiteral("arguments"),
                         QJsonObject{{QStringLiteral("route"), QStringLiteral("missing")}}}},
            [&](const DsConnector::ToolCallOutcome &outcome) {
                invalidCode = outcome.result.value(QStringLiteral("structuredContent"))
                                  .toObject()
                                  .value(QStringLiteral("code"))
                                  .toString();
            });
        ok &= expect(invalidCode == QStringLiteral("invalid_editor_tool_arguments") &&
                         http.calledTools.size() == callsBeforeInvalid,
                     "generic invoke must validate the actual input schema before forwarding");
        runtime.stop();
        return ok;
    }

    bool verifyCommandTransportOutcome() {
        FakeHttpEditor http;
        bool ok = expect(http.listen(), "command-transport fake editor must listen");
        if (!ok)
            return false;
        http.exposeCommandTool = true;
        const auto serviceName = QStringLiteral("DsConnectorLite-Command-%1")
                                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        FakeBootstrap bootstrap(serviceName);
        ok &= expect(bootstrap.listen(), "command-transport bootstrap must listen");
        if (!ok)
            return false;
        bootstrap.publish(SingleInstanceAutomationStatus{
            .state = SingleInstanceAutomationState::McpReady,
            .editorInstanceId = QStringLiteral("command-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
            .mcpEnabled = true,
            .mcpEndpoint = http.endpoint(),
        });
        DsConnector::ConnectorRuntime runtime(DsConnector::ConnectorOptions{
            .exposure = {
                .profile = AutomationWire::ExposureProfile::L0,
                .includes = {QStringLiteral("id:fake.command")},
            },
            .upstreamTimeoutMs = 2000,
        }, serviceName);
        runtime.start();
        ok &= expect(waitUntil([&] {
                         const auto status = runtime.status();
                         return status.value(QStringLiteral("exposure"))
                                        .toObject()
                                        .value(QStringLiteral("generic_target_count"))
                                        .toInt() == 1 &&
                                status.value(QStringLiteral("manifest"))
                                        .toObject()
                                        .value(QStringLiteral("compatibility"))
                                        .toString() == QStringLiteral("compatible_subset");
                     }, 10000),
                     "the fake command must become available after handshake");
        const auto invokeCommand = [&] {
            QPair<QString, QString> result;
            runtime.callTool(
                QStringLiteral("editor.tools.invoke"),
                QJsonObject{{QStringLiteral("name"), QStringLiteral("fake.command")},
                            {QStringLiteral("arguments"), QJsonObject{}}},
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
        ok &= expect(rateLimited.first == QStringLiteral("too_many_requests") &&
                         rateLimited.second == QStringLiteral("fake request limit reached"),
                     "a trusted HTTP 429 envelope must remain too_many_requests for commands");

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::TransportError;
        http.applicationTransportStatus = 503;
        http.applicationTransportCode = QStringLiteral("busy");
        http.applicationTransportMessage = QStringLiteral("fake global concurrency limit reached");
        const auto busy = invokeCommand();
        ok &= expect(busy.first == QStringLiteral("busy") &&
                         busy.second == QStringLiteral("fake global concurrency limit reached"),
                     "a trusted HTTP 503 busy envelope must not become outcome_unknown");

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::TransportError;
        http.applicationTransportStatus = 503;
        http.applicationTransportCode = QStringLiteral("mcp_stopping");
        http.applicationTransportMessage = QStringLiteral("fake MCP server is stopping");
        const auto stopping = invokeCommand();
        ok &= expect(stopping.first == QStringLiteral("mcp_stopping") &&
                         stopping.second == QStringLiteral("fake MCP server is stopping"),
                     "a trusted HTTP 503 stopping envelope must remain deterministic");

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::TransportError;
        http.applicationTransportStatus = 504;
        http.applicationTransportCode = QStringLiteral("request_timeout");
        http.applicationTransportMessage = QStringLiteral("fake request deadline elapsed");
        const auto serverTimeout = invokeCommand();
        ok &= expect(serverTimeout.first == QStringLiteral("outcome_unknown") &&
                         serverTimeout.second == QStringLiteral("request_timeout"),
                     "an HTTP request deadline must retain unknown command outcome semantics");

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::TransportError;
        http.applicationTransportStatus = 429;
        http.applicationTransportCode = QStringLiteral("busy");
        http.applicationTransportMessage = QStringLiteral("forged status/code pair");
        const auto mismatched = invokeCommand();
        ok &= expect(mismatched.first == QStringLiteral("outcome_unknown") &&
                         mismatched.second == QStringLiteral("invalid_upstream_response"),
                     "a mismatched transport status and code must not be trusted");

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::TransportError;
        http.applicationTransportStatus = 429;
        http.applicationTransportCode = QStringLiteral("too_many_requests");
        http.applicationTransportMessage = QStringLiteral("forged envelope shape");
        http.applicationTransportExtraField = true;
        const auto extendedEnvelope = invokeCommand();
        http.applicationTransportExtraField = false;
        ok &= expect(extendedEnvelope.first == QStringLiteral("outcome_unknown") &&
                         extendedEnvelope.second == QStringLiteral("invalid_upstream_response"),
                     "a transport envelope with undeclared fields must not be trusted");

        http.applicationResponseMode = FakeHttpEditor::ApplicationResponseMode::Hold;
        const auto timedOut = invokeCommand();
        if (timedOut.first != QStringLiteral("outcome_unknown") ||
            timedOut.second != QStringLiteral("upstream_timeout")) {
            QTextStream(stderr) << "Command timeout observed code=" << timedOut.first
                                << " message=" << timedOut.second << Qt::endl;
        }
        ok &= expect(timedOut.first == QStringLiteral("outcome_unknown") &&
                         timedOut.second == QStringLiteral("upstream_timeout"),
                     "command timeout must report outcome_unknown while preserving the cause");
        waitUntil([&] { return http.connectionCount() == 0; }, 3000);
        runtime.stop();
        return ok;
    }

    bool verifyPaginatedHandshake() {
        FakeHttpEditor http;
        bool ok = expect(http.listen(), "pagination fake editor must listen");
        if (!ok)
            return false;
        http.extraToolCount = 125;
        http.pageSize = 17;

        const auto serviceName = QStringLiteral("DsConnectorLite-Pagination-%1")
                                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        FakeBootstrap bootstrap(serviceName);
        ok &= expect(bootstrap.listen(), "pagination fake bootstrap must listen");
        if (!ok)
            return false;
        SingleInstanceAutomationStatus ready{
            .state = SingleInstanceAutomationState::McpReady,
            .editorInstanceId = QStringLiteral("pagination-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
            .mcpEnabled = true,
            .mcpEndpoint = http.endpoint(),
        };
        bootstrap.publish(ready);

        DsConnector::ConnectorRuntime runtime(DsConnector::ConnectorOptions{
            .exposure = {
                .profile = AutomationWire::ExposureProfile::L0,
                .includes = {QStringLiteral("prefix:fake.")},
            },
            .upstreamTimeoutMs = 2000,
        }, serviceName);
        runtime.start();
        const auto paginationReady = waitUntil([&] {
                         const auto status = runtime.status();
                         return status.value(QStringLiteral("exposure"))
                                        .toObject()
                                        .value(QStringLiteral("generic_target_count"))
                                        .toInt() == 125 &&
                                status.value(QStringLiteral("manifest"))
                                        .toObject()
                                        .value(QStringLiteral("compatibility"))
                                        .toString() != QStringLiteral("refreshing") &&
                                http.toolsListCount >= 8 && http.manifestCallCount >= 10;
                     }, 15000);
        if (!paginationReady) {
            QTextStream(stderr) << "Pagination status: "
                                << QJsonDocument(runtime.status()).toJson(QJsonDocument::Compact)
                                << " tools_pages=" << http.toolsListCount
                                << " manifest_pages=" << http.manifestCallCount
                                << " raw_tail=" << http.rawLog().right(4000) << Qt::endl;
        }
        ok &= expect(paginationReady,
                     "handshake must collect every tools/list and manifest cursor page");
        ok &= expect(runtime.downstreamTools().size() == 6,
                     "paginated unknown tools must not change the frozen typed tool set");
        QJsonObject described;
        runtime.callTool(
            QStringLiteral("editor.tools.describe"),
            QJsonObject{{QStringLiteral("name"), QStringLiteral("fake.tool.124")}},
            [&described](const DsConnector::ToolCallOutcome &outcome) {
                described = outcome.result.value(QStringLiteral("structuredContent")).toObject();
            });
        ok &= expect(described.value(QStringLiteral("tool"))
                             .toObject()
                             .value(QStringLiteral("name")) ==
                         QStringLiteral("fake.tool.124") &&
                         described.value(QStringLiteral("input_schema")).isObject(),
                     "the final pages must be available to generic describe");
        runtime.stop();
        return ok;
    }

    bool verifyConcurrentConnectors() {
        FakeHttpEditor http;
        bool ok = expect(http.listen(), "concurrent fake editor must listen");
        if (!ok)
            return false;
        const auto serviceName = QStringLiteral("DsConnectorLite-Concurrent-%1")
                                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        FakeBootstrap bootstrap(serviceName);
        ok &= expect(bootstrap.listen(), "concurrent fake bootstrap must listen");
        if (!ok)
            return false;
        SingleInstanceAutomationStatus ready{
            .state = SingleInstanceAutomationState::McpReady,
            .editorInstanceId = QStringLiteral("concurrent-editor"),
            .executablePath = QCoreApplication::applicationFilePath(),
            .applicationVersion = QStringLiteral("test"),
            .buildId = QStringLiteral("fake-build"),
            .hostMode = QStringLiteral("gui"),
            .mcpEnabled = true,
            .mcpEndpoint = http.endpoint(),
        };
        bootstrap.publish(ready);
        const DsConnector::ConnectorOptions options{
            .exposure = {
                .profile = AutomationWire::ExposureProfile::L0,
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
                   status.value(QStringLiteral("manifest"))
                           .toObject()
                           .value(QStringLiteral("compatibility"))
                           .toString() != QStringLiteral("refreshing");
        };
        ok &= expect(waitUntil([&] {
                         return readyRuntime(first) && readyRuntime(second) &&
                                bootstrap.watcherCount() == 2;
                     }, 15000),
                     "two connectors must discover and watch the same editor independently");
        ok &= expect(first.instanceId() != second.instanceId(),
                     "concurrent connectors must use distinct connector identities");
        const auto fixedCount = first.downstreamTools().size();
        first.reconnect();
        ok &= expect(waitUntil([&] {
                         return readyRuntime(first) && readyRuntime(second) &&
                                bootstrap.watcherCount() == 2;
                     }, 15000),
                     "one connector reconnect must not disturb the other connector");
        ok &= expect(first.downstreamTools().size() == fixedCount &&
                         second.downstreamTools().size() == fixedCount,
                     "concurrent reconnect must preserve both fixed tool surfaces");

        const auto refreshCount = http.toolsListCount;
        bootstrap.publish(ready);
        ok &= expect(waitUntil([&] {
                         return http.toolsListCount >= refreshCount + 2 && readyRuntime(first) &&
                                readyRuntime(second);
                     }, 15000),
                     "one ready snapshot must independently refresh both connectors");
        first.stop();
        ok &= expect(waitUntil([&] { return bootstrap.watcherCount() == 1; }) &&
                         readyRuntime(second),
                     "stopping one connector must leave the other watch and MCP session alive");
        second.stop();
        return ok;
    }

    bool verifyStdioFraming() {
        const auto executable = QCoreApplication::applicationDirPath() +
                                QStringLiteral("/DsConnectorLite.exe");
        bool ok = expect(QFile::exists(executable), "connector executable must exist for stdio E2E");
        if (!ok)
            return false;

        QProcess process;
        process.setProgram(executable);
        process.setArguments({QStringLiteral("--exposure-profile"), QStringLiteral("l0")});
        process.start();
        ok &= expect(process.waitForStarted(5000), "stdio connector process must start");
        const auto context = clientContext();
        const auto discover = QJsonDocument(AutomationWire::Mcp::makeRequest(
                                                QString::fromLatin1(
                                                    AutomationWire::Mcp::DiscoverMethod),
                                                {}, context, QStringLiteral("stdio-discover")))
                                  .toJson(QJsonDocument::Compact);
        const auto list = QJsonDocument(AutomationWire::Mcp::makeRequest(
                                            QString::fromLatin1(
                                                AutomationWire::Mcp::ToolsListMethod),
                                            {}, context, QStringLiteral("stdio-list")))
                              .toJson(QJsonDocument::Compact);
        process.write(discover + "\r\n" + list + "\r\n");
        process.closeWriteChannel();
        ok &= expect(process.waitForFinished(15000),
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
        ok &= expect(allJson && standardError.isEmpty(),
                     "stdout must contain exactly one JSON object per line and no logs or blanks");

        QProcess unterminated;
        unterminated.setProgram(executable);
        unterminated.setArguments(
            {QStringLiteral("--exposure-profile"), QStringLiteral("l0")});
        unterminated.start();
        ok &= expect(unterminated.waitForStarted(5000),
                     "unterminated-final-frame connector must start");
        unterminated.write(discover);
        unterminated.closeWriteChannel();
        ok &= expect(unterminated.waitForFinished(15000) &&
                         unterminated.readAllStandardOutput().split('\n').size() == 2 &&
                         unterminated.readAllStandardError().isEmpty() &&
                         unterminated.exitCode() == 0,
                     "a valid final EOF frame without newline must be processed exactly once");

        QProcess notification;
        notification.setProgram(executable);
        notification.setArguments(
            {QStringLiteral("--exposure-profile"), QStringLiteral("l0")});
        notification.start();
        ok &= expect(notification.waitForStarted(5000),
                     "stdio notification connector must start");
        notification.write(
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod), {}, context))
                .toJson(QJsonDocument::Compact) +
            '\n');
        notification.closeWriteChannel();
        ok &= expect(notification.waitForFinished(15000) &&
                         notification.readAllStandardOutput().isEmpty() &&
                         notification.readAllStandardError().isEmpty(),
                     "a recognizable JSON-RPC notification must produce zero stdout bytes");

        QProcess rapidInput;
        rapidInput.setProgram(executable);
        rapidInput.setArguments(
            {QStringLiteral("--exposure-profile"), QStringLiteral("l0")});
        rapidInput.start();
        ok &= expect(rapidInput.waitForStarted(5000),
                     "rapid-input connector must start");
        const auto notificationFrame =
            QJsonDocument(AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod), {},
                              context))
                .toJson(QJsonDocument::Compact) +
            '\n';
        QByteArray notificationFlood;
        notificationFlood.reserve(notificationFrame.size() * 20000);
        for (auto index = 0; index < 20000; ++index)
            notificationFlood.append(notificationFrame);
        rapidInput.write(notificationFlood);
        rapidInput.closeWriteChannel();
        ok &= expect(rapidInput.waitForFinished(20000) && rapidInput.exitCode() == 0 &&
                         rapidInput.readAllStandardOutput().isEmpty() &&
                         rapidInput.readAllStandardError().isEmpty(),
                     "bounded stdin delivery must drain a rapid notification flood without output");

        QProcess blockedOutput;
        QProcess blockingSink;
        blockedOutput.setProgram(executable);
        blockedOutput.setArguments(
            {QStringLiteral("--exposure-profile"), QStringLiteral("l0")});
        blockingSink.setProgram(QStringLiteral("powershell.exe"));
        blockingSink.setArguments({QStringLiteral("-NoProfile"), QStringLiteral("-Command"),
                                   QStringLiteral("Start-Sleep -Seconds 30")});
        blockedOutput.setStandardOutputProcess(&blockingSink);
        blockedOutput.start();
        blockingSink.start();
        ok &= expect(blockedOutput.waitForStarted(5000) && blockingSink.waitForStarted(5000),
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
        ok &= expect(blockedFinished && blockedTimer.elapsed() < 10000 &&
                         blockedOutput.exitStatus() == QProcess::NormalExit &&
                         blockedOutput.exitCode() == 3 &&
                         blockedError.contains("stdout_backpressure_limit_exceeded"),
                     "a non-reading stdout peer must trip the bounded writer queue without "
                     "blocking the main event loop");
        blockingSink.kill();
        blockingSink.waitForFinished(5000);

        QProcess boundary;
        boundary.setProgram(executable);
        boundary.setArguments({QStringLiteral("--exposure-profile"), QStringLiteral("l0")});
        boundary.start();
        ok &= expect(boundary.waitForStarted(5000), "boundary-frame connector must start");
        boundary.write(QByteArray(16 * 1024 * 1024, 'x') + '\n');
        boundary.closeWriteChannel();
        ok &= expect(boundary.waitForFinished(15000),
                     "the exact 16 MiB frame boundary must finish under watchdog");
        const auto boundaryOutput = boundary.readAllStandardOutput();
        const auto boundaryResponse =
            QJsonDocument::fromJson(boundaryOutput.trimmed()).object();
        ok &= expect(boundaryResponse.value(QStringLiteral("error"))
                             .toObject()
                             .value(QStringLiteral("code"))
                             .toInt() == AutomationWire::Mcp::ParseError &&
                         boundary.readAllStandardError().isEmpty() && boundary.exitCode() == 0,
                     "an exact-limit frame must be parsed instead of rejected as too large");

        QProcess oversizedLine;
        oversizedLine.setProgram(executable);
        oversizedLine.setArguments(
            {QStringLiteral("--exposure-profile"), QStringLiteral("l0")});
        oversizedLine.start();
        ok &= expect(oversizedLine.waitForStarted(5000),
                     "oversized newline-frame connector must start");
        oversizedLine.write(QByteArray(16 * 1024 * 1024 + 1, 'x') + '\n');
        oversizedLine.closeWriteChannel();
        ok &= expect(oversizedLine.waitForFinished(15000) &&
                         oversizedLine.readAllStandardOutput().isEmpty() &&
                         oversizedLine.readAllStandardError().contains("frame_too_large") &&
                         oversizedLine.exitCode() == 3,
                     "an overlong newline-terminated frame must be rejected without parsing");

        QProcess oversized;
        oversized.setProgram(executable);
        oversized.setArguments({QStringLiteral("--exposure-profile"), QStringLiteral("l0")});
        oversized.start();
        ok &= expect(oversized.waitForStarted(5000), "oversized-frame connector must start");
        oversized.write(QByteArray(16 * 1024 * 1024 + 1, 'x'));
        oversized.closeWriteChannel();
        ok &= expect(oversized.waitForFinished(15000),
                     "oversized no-newline frame must terminate under watchdog");
        ok &= expect(oversized.readAllStandardOutput().isEmpty() &&
                         oversized.readAllStandardError().contains("frame_too_large") &&
                         oversized.exitCode() == 3,
                     "an overlong partial frame must never be parsed and must report frame_too_large");
        return ok;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    bool ok = true;
    const auto arguments = application.arguments();
    const auto only = [&](const QString &name) {
        return arguments.size() == 1 || arguments.contains(QStringLiteral("--") + name);
    };
    if (only(QStringLiteral("options")))
        ok &= verifyOptionsAndExposure();
    if (only(QStringLiteral("offline")))
        ok &= verifyOfflineDownstream();
    if (only(QStringLiteral("states")))
        ok &= verifyUnavailableEditorStates();
    if (only(QStringLiteral("bootstrap")))
        ok &= verifyBootstrapCorrelation();
    if (only(QStringLiteral("handshake")))
        ok &= verifyHandshakeCoordination();
    if (only(QStringLiteral("integration")))
        ok &= verifyFakeEditorIntegration();
    if (only(QStringLiteral("forward")))
        ok &= verifyForwardCompatibleGenericTools();
    if (only(QStringLiteral("compatibility")))
        ok &= verifyCompatibilityVersions();
    if (only(QStringLiteral("headers")))
        ok &= verifyParameterHeaders();
    if (only(QStringLiteral("command")))
        ok &= verifyCommandTransportOutcome();
    if (only(QStringLiteral("pagination")))
        ok &= verifyPaginatedHandshake();
    if (only(QStringLiteral("concurrent")))
        ok &= verifyConcurrentConnectors();
    if (only(QStringLiteral("stdio")))
        ok &= verifyStdioFraming();
    const auto archivePath = qEnvironmentVariable("DS_CONNECTOR_TEST_ARCHIVE");
    if (!archivePath.isEmpty()) {
        QFile archive(archivePath);
        if (archive.open(QIODevice::WriteOnly | QIODevice::Truncate))
            archive.write(g_fakeHttpLog);
        else
            ok = false;
    }
    return ok ? 0 : 1;
}
