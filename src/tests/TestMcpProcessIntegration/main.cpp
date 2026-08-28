#include <Bootstrap/SingleInstanceIdentity.h>
#include <BootstrapWatcher.h>
#include <UpstreamMcpClient.h>

#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/McpProtocol.h>
#include <lite/ProductMetadata.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QSet>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QThread>
#include <QTextStream>
#include <QUuid>

#include <algorithm>
#include <functional>
#include <optional>
#include <utility>

namespace {
    bool fail(const QString &message) {
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    bool waitUntil(const std::function<bool()> &predicate, const int timeoutMilliseconds) {
        QElapsedTimer timer;
        timer.start();
        while (!predicate() && timer.elapsed() < timeoutMilliseconds) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QThread::msleep(10);
        }
        return predicate();
    }

    QJsonObject requestMeta() {
        return {
            {QStringLiteral("io.modelcontextprotocol/protocolVersion"),
             QString::fromLatin1(AutomationWire::Mcp::ProtocolVersion)                  },
            {QStringLiteral("io.modelcontextprotocol/clientCapabilities"), QJsonObject{}},
            {QStringLiteral("io.modelcontextprotocol/clientInfo"),
             QJsonObject{
                 {QStringLiteral("name"), QStringLiteral("phase-two-process-test")},
                 {QStringLiteral("version"), QStringLiteral("1")},
             }                                                                          },
        };
    }

    QJsonObject makeRequest(const qint64 id, const QString &method, QJsonObject params = {}) {
        params.insert(QStringLiteral("_meta"), requestMeta());
        return {
            {QStringLiteral("jsonrpc"), QString::fromLatin1(AutomationWire::Mcp::JsonRpcVersion)},
            {QStringLiteral("id"),      id                                                      },
            {QStringLiteral("method"),  method                                                  },
            {QStringLiteral("params"),  params                                                  },
        };
    }

    QJsonObject makeToolRequest(const qint64 id, const QString &name,
                                const QJsonObject &arguments = {}) {
        return makeRequest(id, QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                           QJsonObject{
                               {QStringLiteral("name"),      name     },
                               {QStringLiteral("arguments"), arguments},
        });
    }

    AutomationWire::Mcp::RequestContext legacyRequestContext() {
        return {
            .protocolVersion =
                QString::fromLatin1(AutomationWire::Mcp::CompatibilityProtocolVersion),
            .clientCapabilities = QJsonObject{                                                      },
            .clientInfo =
                AutomationWire::Mcp::ImplementationInfo{
                                              .name = QStringLiteral("phase-two-process-test-legacy"),
                                              .version = QStringLiteral("1"),
                                              },
        };
    }

    QJsonObject makeLegacyRequest(const qint64 id, const QString &method, QJsonObject params = {}) {
        return AutomationWire::Mcp::makeRequest(method, std::move(params), legacyRequestContext(),
                                                id);
    }

    QJsonObject makeLegacyToolRequest(const qint64 id, const QString &name,
                                      const QJsonObject &arguments = {}) {
        return makeLegacyRequest(id, QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                                 QJsonObject{
                                     {QStringLiteral("name"),      name     },
                                     {QStringLiteral("arguments"), arguments},
        });
    }

    bool writeMessage(QProcess &process, const QJsonObject &message, QString &error) {
        error.clear();
        auto bytes = QJsonDocument(message).toJson(QJsonDocument::Compact);
        bytes.append('\n');
        if (process.write(bytes) != bytes.size() ||
            (process.bytesToWrite() > 0 && !process.waitForBytesWritten(5000))) {
            error = QStringLiteral("Could not write a complete stdio MCP message");
            return false;
        }
        return true;
    }

    std::optional<QJsonObject> exchange(QProcess &process, const QJsonObject &request,
                                        const int timeoutMilliseconds, QString &error) {
        if (!writeMessage(process, request, error))
            return std::nullopt;

        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMilliseconds) {
            if (!process.canReadLine()) {
                process.waitForReadyRead(100);
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                continue;
            }
            const auto line = process.readLine().trimmed();
            QJsonParseError parseError;
            const auto document = QJsonDocument::fromJson(line, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                error = QStringLiteral("Connector stdout contained a non-JSON protocol line");
                return std::nullopt;
            }
            const auto response = document.object();
            if (response.value(QStringLiteral("id")) != request.value(QStringLiteral("id"))) {
                error = QStringLiteral("Connector returned an unexpected JSON-RPC id");
                return std::nullopt;
            }
            return response;
        }
        error = QStringLiteral("Timed out waiting for a connector response");
        return std::nullopt;
    }

    QJsonValue structuredContent(const QJsonObject &response) {
        return response.value(QStringLiteral("result"))
            .toObject()
            .value(QStringLiteral("structuredContent"));
    }

    QJsonValue normalizedJson(const QJsonValue &value) {
        if (value.isArray()) {
            QJsonArray result;
            for (const auto &entry : value.toArray())
                result.append(normalizedJson(entry));
            return result;
        }
        if (value.isObject()) {
            const auto object = value.toObject();
            auto keys = object.keys();
            std::sort(keys.begin(), keys.end());
            QJsonObject result;
            for (const auto &key : std::as_const(keys))
                result.insert(key, normalizedJson(object.value(key)));
            return result;
        }
        return value;
    }

    QString compactJson(const QJsonObject &object) {
        return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    }

    std::optional<QJsonObject> connectorToolContent(QProcess &process, const qint64 requestId,
                                                    const QString &name,
                                                    const QJsonObject &arguments,
                                                    const int timeoutMilliseconds, QString &error) {
        const auto response = exchange(process, makeToolRequest(requestId, name, arguments),
                                       timeoutMilliseconds, error);
        if (!response)
            return std::nullopt;
        if (response->contains(QStringLiteral("error"))) {
            error = QStringLiteral("Connector JSON-RPC error: %1").arg(compactJson(*response));
            return std::nullopt;
        }
        const auto result = response->value(QStringLiteral("result"));
        if (!result.isObject()) {
            error = QStringLiteral("Connector tool response has no result object: %1")
                        .arg(compactJson(*response));
            return std::nullopt;
        }
        const auto resultObject = result.toObject();
        if (resultObject.value(QStringLiteral("isError")).toBool()) {
            error = QStringLiteral("Connector tool returned an application error: %1")
                        .arg(compactJson(*response));
            return std::nullopt;
        }
        const auto content = resultObject.value(QStringLiteral("structuredContent"));
        if (!content.isObject()) {
            error = QStringLiteral("Connector tool response has no structuredContent object: %1")
                        .arg(compactJson(*response));
            return std::nullopt;
        }
        return content.toObject();
    }

    std::optional<QJsonObject> directToolContent(DsConnector::UpstreamMcpClient &client,
                                                 const QString &name, const QJsonObject &arguments,
                                                 const int timeoutMilliseconds, QString &error) {
        error.clear();
        std::optional<DsConnector::UpstreamResult> response;
        client.send(
            QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
            QJsonObject{
                {QStringLiteral("name"),      name     },
                {QStringLiteral("arguments"), arguments},
        },
            [&response](DsConnector::UpstreamResult result) { response = std::move(result); },
            timeoutMilliseconds);
        if (!waitUntil([&response] { return response.has_value(); }, timeoutMilliseconds + 1000)) {
            error = QStringLiteral("Direct editor tool call timed out");
            return std::nullopt;
        }
        if (!response->connectorError.isEmpty()) {
            error = QStringLiteral("Direct editor transport error: %1 (%2)")
                        .arg(response->connectorError, response->connectorErrorMessage);
            return std::nullopt;
        }
        if (response->protocolError) {
            error = QStringLiteral("Direct editor JSON-RPC error %1: %2")
                        .arg(response->protocolError->code)
                        .arg(response->protocolError->message);
            return std::nullopt;
        }
        if (response->result.value(QStringLiteral("isError")).toBool()) {
            error = QStringLiteral("Direct editor tool returned an application error: %1")
                        .arg(compactJson(response->result));
            return std::nullopt;
        }
        const auto content = response->result.value(QStringLiteral("structuredContent"));
        if (!content.isObject()) {
            error =
                QStringLiteral("Direct editor tool response has no structuredContent object: %1")
                    .arg(compactJson(response->result));
            return std::nullopt;
        }
        return content.toObject();
    }

    bool equivalentContent(const QString &name, const QJsonObject &throughConnector,
                           const QJsonObject &direct, QString &error) {
        const auto normalizedConnector = normalizedJson(throughConnector);
        const auto normalizedDirect = normalizedJson(direct);
        if (normalizedConnector == normalizedDirect)
            return true;
        error =
            QStringLiteral(
                "%1 differs between connector and direct editor calls; connector=%2; direct=%3")
                .arg(
                    name,
                    QString::fromUtf8(QJsonDocument(normalizedConnector.toObject())
                                          .toJson(QJsonDocument::Compact)),
                    QString::fromUtf8(
                        QJsonDocument(normalizedDirect.toObject()).toJson(QJsonDocument::Compact)));
        return false;
    }

    QString schemaIssue(const QJsonValue &schema) {
        const auto validation = AutomationWire::checkJsonSchema(schema);
        if (validation.valid())
            return {};
        const auto &issue = validation.issues.constFirst();
        return QStringLiteral("schema_path=%1, message=%2").arg(issue.schemaPath, issue.message);
    }

    QString descriptorIssue(const QJsonValue &value) {
        if (!value.isObject())
            return QStringLiteral("descriptor is not an object");
        const auto tool = value.toObject();
        const auto name = tool.value(QStringLiteral("name"));
        if (!name.isString() || name.toString().isEmpty())
            return QStringLiteral("name is missing or invalid");
        const auto inputSchema = tool.value(QStringLiteral("inputSchema"));
        if (!inputSchema.isObject() ||
            inputSchema.toObject().value(QStringLiteral("type")) != QStringLiteral("object")) {
            return QStringLiteral("inputSchema is not an object schema");
        }
        for (const auto &field : {QStringLiteral("title"), QStringLiteral("description")}) {
            if (tool.contains(field) && !tool.value(field).isString())
                return field + QStringLiteral(" is not a string");
        }
        if (tool.contains(QStringLiteral("annotations")) &&
            !tool.value(QStringLiteral("annotations")).isObject()) {
            return QStringLiteral("annotations is not an object");
        }
        if (tool.contains(QStringLiteral("icons")) &&
            !tool.value(QStringLiteral("icons")).isArray()) {
            return QStringLiteral("icons is not an array");
        }
        if (tool.contains(QStringLiteral("_meta")) &&
            !tool.value(QStringLiteral("_meta")).isObject()) {
            return QStringLiteral("_meta is not an object");
        }
        const auto inputIssue = schemaIssue(inputSchema);
        if (!inputIssue.isEmpty())
            return QStringLiteral("invalid inputSchema: ") + inputIssue;
        if (tool.contains(QStringLiteral("outputSchema"))) {
            if (!tool.value(QStringLiteral("outputSchema")).isObject())
                return QStringLiteral("outputSchema is not an object");
            const auto outputIssue = schemaIssue(tool.value(QStringLiteral("outputSchema")));
            if (!outputIssue.isEmpty())
                return QStringLiteral("invalid outputSchema: ") + outputIssue;
        }
        return {};
    }

    QString upstreamToolsDiagnostic(const QString &endpoint) {
        DsConnector::UpstreamMcpClient client(QStringLiteral("process-test-diagnostic"),
                                              QStringLiteral("1"));
        QString endpointError;
        if (!client.setEndpoint(endpoint, &endpointError))
            return QStringLiteral("invalid endpoint: ") + endpointError;
        std::optional<DsConnector::UpstreamResult> response;
        client.send(
            QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod), {},
            [&response](DsConnector::UpstreamResult result) { response = std::move(result); },
            5000);
        if (!waitUntil([&response] { return response.has_value(); }, 6000))
            return QStringLiteral("direct tools/list timed out");
        if (!response->succeeded()) {
            return QStringLiteral("direct tools/list failed: %1")
                .arg(response->connectorError.isEmpty() ? response->protocolError->message
                                                        : response->connectorError);
        }
        QSet<QString> names;
        const auto tools = response->result.value(QStringLiteral("tools")).toArray();
        for (const auto &tool : tools) {
            const auto name = tool.toObject().value(QStringLiteral("name")).toString();
            const auto issue = descriptorIssue(tool);
            if (!issue.isEmpty())
                return QStringLiteral("tool=%1: %2").arg(name, issue);
            if (names.contains(name))
                return QStringLiteral("duplicate tool name: ") + name;
            names.insert(name);
        }
        return QStringLiteral("page descriptors valid; count=%1; nextCursor=%2")
            .arg(tools.size())
            .arg(response->result.value(QStringLiteral("nextCursor")).toString());
    }

    QString upstreamStatusDiagnostic(const QString &endpoint) {
        DsConnector::UpstreamMcpClient client(QStringLiteral("process-test-status-diagnostic"),
                                              QStringLiteral("1"));
        QString endpointError;
        if (!client.setEndpoint(endpoint, &endpointError))
            return QStringLiteral("invalid endpoint: ") + endpointError;

        QElapsedTimer elapsed;
        elapsed.start();
        std::optional<DsConnector::UpstreamResult> response;
        client.send(
            QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
            QJsonObject{
                {QStringLiteral("name"),      QStringLiteral("application.get_status")},
                {QStringLiteral("arguments"), QJsonObject{}                           },
        },
            [&response](DsConnector::UpstreamResult result) { response = std::move(result); },
            5000);
        if (!waitUntil([&response] { return response.has_value(); }, 6000)) {
            return QStringLiteral("callback timeout; elapsed_ms=%1").arg(elapsed.elapsed());
        }

        const auto prefix = QStringLiteral("elapsed_ms=%1; http=%2")
                                .arg(elapsed.elapsed())
                                .arg(response->httpStatus);
        if (!response->connectorError.isEmpty()) {
            return QStringLiteral("%1; connector_error=%2; connector_message=%3; "
                                  "outcome_unknown=%4")
                .arg(prefix, response->connectorError, response->connectorErrorMessage,
                     response->outcomeUnknown ? QStringLiteral("true") : QStringLiteral("false"));
        }
        if (response->protocolError) {
            return QStringLiteral("%1; protocol_error=%2; protocol_message=%3")
                .arg(prefix)
                .arg(response->protocolError->code)
                .arg(response->protocolError->message);
        }

        const auto toolError = response->result.value(QStringLiteral("isError")).toBool();
        const auto content = response->result.value(QStringLiteral("structuredContent")).toObject();
        if (toolError) {
            return QStringLiteral("%1; tool_error=true; code=%2; message=%3")
                .arg(prefix, content.value(QStringLiteral("code")).toString(),
                     content.value(QStringLiteral("message")).toString());
        }
        return QStringLiteral("%1; tool_error=false; toolset_version=%2; documents=%3")
            .arg(prefix)
            .arg(content.value(QStringLiteral("toolset_version")).toInteger())
            .arg(content.value(QStringLiteral("documents")).toArray().size());
    }

    void stopProcess(QProcess &process) {
        if (process.state() == QProcess::NotRunning)
            return;
        process.terminate();
        if (!process.waitForFinished(5000)) {
            process.kill();
            process.waitForFinished(5000);
        }
    }

    bool runIntegration(const QString &editorPath, const QString &connectorPath) {
        QTemporaryDir isolatedRoot;
        if (!isolatedRoot.isValid()) {
            return fail(QStringLiteral("Could not create an isolated process-test data root"));
        }
        const auto appDataRoot = QDir(isolatedRoot.path()).filePath(QStringLiteral("Roaming"));
        const auto localDataRoot = QDir(isolatedRoot.path()).filePath(QStringLiteral("Local"));
        QDir().mkpath(appDataRoot);
        QDir().mkpath(localDataRoot);
        const auto editorDataDirectory =
            QDir(appDataRoot)
                .filePath(QStringLiteral("%1/%2").arg(
                    QString::fromLatin1(LiteProductMetadata::Publisher),
                    QString::fromLatin1(LiteProductMetadata::ProductName)));
        if (!QDir().mkpath(editorDataDirectory))
            return fail(QStringLiteral("Could not create the isolated editor data directory"));
        QFile seededConfig(QDir(editorDataDirectory).filePath(QStringLiteral("appConfig.json")));
        if (!seededConfig.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return fail(QStringLiteral("Could not seed the isolated editor configuration"));
        seededConfig.write(
            QJsonDocument(QJsonObject{
                              {QStringLiteral("audio"),
                               QJsonObject{{QStringLiteral("deviceName"),
                                            QStringLiteral("configured-only-device")}}},
        })
                .toJson(QJsonDocument::Compact));
        seededConfig.close();
        const auto serviceName = SingleInstanceIdentity::serviceName(editorDataDirectory);

        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("APPDATA"), appDataRoot);
        environment.insert(QStringLiteral("LOCALAPPDATA"), localDataRoot);
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        environment.insert(QStringLiteral("QT_OPENGL"), QStringLiteral("software"));
        environment.insert(QStringLiteral("QT_LOGGING_TO_CONSOLE"), QStringLiteral("1"));

        QProcess editor;
        QProcess connector;
        const auto cleanup = qScopeGuard([&] {
            connector.closeWriteChannel();
            if (!connector.waitForFinished(3000))
                stopProcess(connector);
            stopProcess(editor);
        });

        editor.setProcessEnvironment(environment);
        editor.setWorkingDirectory(QFileInfo(editorPath).absolutePath());
        editor.setProcessChannelMode(QProcess::SeparateChannels);

        QTcpServer portProbe;
        if (!portProbe.listen(QHostAddress::LocalHost, 0)) {
            return fail(QStringLiteral("Could not allocate an isolated editor control port: %1")
                            .arg(portProbe.errorString()));
        }
        const auto controlPort = portProbe.serverPort();
        portProbe.close();

        editor.start(editorPath, {QStringLiteral("--mcp"), QStringLiteral("--automation-profile"),
                                  QStringLiteral("l3"), QStringLiteral("--control-port"),
                                  QString::number(controlPort)});
        if (!editor.waitForStarted(10000)) {
            return fail(QStringLiteral("Editor failed to start: %1").arg(editor.errorString()));
        }

        DsConnector::BootstrapWatcher watcher(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                              QStringLiteral("1"), serviceName);
        watcher.start();
        const auto watcherCleanup = qScopeGuard([&watcher] { watcher.stop(); });
        const auto editorSettled = waitUntil(
            [&] {
                const auto &observation = watcher.observation();
                return observation.snapshot && observation.snapshot->result.state ==
                                                   SingleInstanceAutomationState::McpReady ||
                       editor.state() == QProcess::NotRunning;
            },
            45000);
        const auto ready =
            editorSettled && watcher.observation().snapshot &&
            watcher.observation().snapshot->result.state == SingleInstanceAutomationState::McpReady;
        if (!ready) {
            const auto &observation = watcher.observation();
            const auto snapshotState = observation.snapshot
                                           ? SingleInstanceProtocol::automationStateName(
                                                 observation.snapshot->result.state)
                                           : QStringLiteral("none");
            const auto snapshotError =
                observation.snapshot ? observation.snapshot->result.error : QString{};
            return fail(
                QStringLiteral("Editor did not publish mcp_ready; appdata=%1; service=%2; "
                               "bootstrap=%3; connected=%4; snapshot_state=%5; "
                               "snapshot_error=%6; process_state=%7; exit_status=%8; "
                               "exit_code=%9; stdout=%10; stderr=%11")
                    .arg(appDataRoot, serviceName, observation.error,
                         observation.connected ? QStringLiteral("true") : QStringLiteral("false"),
                         snapshotState, snapshotError)
                    .arg(static_cast<int>(editor.state()))
                    .arg(static_cast<int>(editor.exitStatus()))
                    .arg(editor.exitCode())
                    .arg(QString::fromUtf8(editor.readAllStandardOutput()),
                         QString::fromUtf8(editor.readAllStandardError())));
        }
        const auto editorInstanceId = watcher.observation().snapshot->result.editorInstanceId;
        const auto editorEndpoint = watcher.observation().snapshot->result.mcpEndpoint;

        connector.setProcessEnvironment(environment);
        connector.setWorkingDirectory(QFileInfo(connectorPath).absolutePath());
        connector.setProcessChannelMode(QProcess::SeparateChannels);
        connector.start(connectorPath,
                        {QStringLiteral("--exposure-profile"), QStringLiteral("l3")});
        if (!connector.waitForStarted(10000)) {
            return fail(
                QStringLiteral("Connector failed to start: %1").arg(connector.errorString()));
        }

        QString exchangeError;
        auto discover = exchange(
            connector, makeRequest(1, QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod)),
            10000, exchangeError);
        if (!discover || discover->contains(QStringLiteral("error"))) {
            return fail(QStringLiteral("Connector discover failed: %1").arg(exchangeError));
        }

        bool connectorReady = false;
        QJsonObject lastConnectorStatus;
        for (qint64 attempt = 0; attempt < 100 && !connectorReady; ++attempt) {
            auto status = exchange(
                connector, makeToolRequest(10 + attempt, QStringLiteral("connector.get_status")),
                5000, exchangeError);
            if (!status) {
                return fail(exchangeError);
            }
            const auto content = structuredContent(*status).toObject();
            lastConnectorStatus = content;
            const auto toolsetCompatibility = content.value(QStringLiteral("toolset"))
                                                  .toObject()
                                                  .value(QStringLiteral("compatibility"))
                                                  .toString();
            connectorReady = content.value(QStringLiteral("mcp"))
                                 .toObject()
                                 .value(QStringLiteral("connected"))
                                 .toBool() &&
                             toolsetCompatibility == QStringLiteral("compatible");
            if (!connectorReady)
                QThread::msleep(100);
        }
        if (!connectorReady) {
            return fail(QStringLiteral("Connector did not complete the upstream editor handshake; "
                                       "bootstrap_snapshot_sequence=%1; status=%2; "
                                       "upstream_tools=%3; upstream_status=%4; "
                                       "connector_stderr=%5; editor_stderr=%6")
                            .arg(watcher.observation().snapshotSequence)
                            .arg(QString::fromUtf8(
                                QJsonDocument(lastConnectorStatus).toJson(QJsonDocument::Compact)))
                            .arg(upstreamToolsDiagnostic(editorEndpoint),
                                 upstreamStatusDiagnostic(editorEndpoint),
                                 QString::fromUtf8(connector.readAllStandardError()),
                                 QString::fromUtf8(editor.readAllStandardError())));
        }

        const auto settingsBefore = connectorToolContent(
            connector, 210, QStringLiteral("settings.query"),
            QJsonObject{
                {QStringLiteral("domains"), QJsonArray{QStringLiteral("audio_device")}}
        },
            5000, exchangeError);
        const auto configuredBefore = settingsBefore
                                          ? settingsBefore->value(QStringLiteral("domains"))
                                                .toObject()
                                                .value(QStringLiteral("audio_device"))
                                                .toObject()
                                                .value(QStringLiteral("configured"))
                                                .toObject()
                                          : QJsonObject{};
        if (configuredBefore.value(QStringLiteral("device_name")).toString() !=
            QStringLiteral("configured-only-device")) {
            return fail(
                QStringLiteral("Editor did not preserve the seeded configured audio device: %1; "
                               "configured=%2; config=%3")
                    .arg(exchangeError, compactJson(configuredBefore), seededConfig.fileName()));
        }
        const auto sparseAudioUpdate =
            connectorToolContent(connector, 211, QStringLiteral("settings.audio_device.update"),
                                 QJsonObject{
                                     {QStringLiteral("gain"), 0.75}
        },
                                 5000, exchangeError);
        const auto configuredAfter =
            sparseAudioUpdate ? sparseAudioUpdate->value(QStringLiteral("configured")).toObject()
                              : QJsonObject{};
        if (!sparseAudioUpdate || configuredAfter.value(QStringLiteral("device_name")).toString() !=
                                      QStringLiteral("configured-only-device")) {
            return fail(
                QStringLiteral("Sparse audio update overwrote an omitted configured device: %1")
                    .arg(exchangeError));
        }

        auto tools = exchange(
            connector, makeRequest(100, QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod)),
            10000, exchangeError);
        if (!tools || tools->contains(QStringLiteral("error"))) {
            return fail(QStringLiteral("Connector tools/list failed: %1").arg(exchangeError));
        }
        QSet<QString> toolNames;
        for (const auto &tool : tools->value(QStringLiteral("result"))
                                    .toObject()
                                    .value(QStringLiteral("tools"))
                                    .toArray()) {
            toolNames.insert(tool.toObject().value(QStringLiteral("name")).toString());
        }
        const QStringList fixedToolNames{
            QStringLiteral("connector.get_status"),  QStringLiteral("connector.reconnect"),
            QStringLiteral("editor.tools.list"),     QStringLiteral("editor.tools.search"),
            QStringLiteral("editor.tools.describe"), QStringLiteral("editor.tools.invoke"),
        };
        const auto missingFixedTool =
            std::find_if(fixedToolNames.cbegin(), fixedToolNames.cend(),
                         [&toolNames](const QString &name) { return !toolNames.contains(name); });
        if (missingFixedTool != fixedToolNames.cend() ||
            !toolNames.contains(QStringLiteral("application.get_status")) ||
            !toolNames.contains(QStringLiteral("application.request_exit")) ||
            !toolNames.contains(QStringLiteral("application.request_restart")) ||
            !toolNames.contains(QStringLiteral("workspace.get_state")) ||
            !toolNames.contains(QStringLiteral("settings.query")) ||
            !toolNames.contains(QStringLiteral("packages.refresh"))) {
            return fail(
                QStringLiteral("Connector did not publish the required bridge and L3 tools"));
        }

        QJsonObject lastEditorStatus;
        QJsonObject lastEditorStatusResponse;
        bool editorStatusReady = false;
        for (qint64 attempt = 0; attempt < 100 && !editorStatusReady; ++attempt) {
            auto editorStatus = exchange(
                connector, makeToolRequest(101 + attempt, QStringLiteral("application.get_status")),
                5000, exchangeError);
            if (!editorStatus) {
                return fail(QStringLiteral("application.get_status failed: %1").arg(exchangeError));
            }
            lastEditorStatusResponse = *editorStatus;
            if (editorStatus->contains(QStringLiteral("error")) ||
                editorStatus->value(QStringLiteral("result"))
                    .toObject()
                    .value(QStringLiteral("isError"))
                    .toBool()) {
                return fail(QStringLiteral("application.get_status returned an error: %1")
                                .arg(QString::fromUtf8(
                                    QJsonDocument(*editorStatus).toJson(QJsonDocument::Compact))));
            }
            lastEditorStatus = structuredContent(*editorStatus).toObject();
            editorStatusReady =
                lastEditorStatus.value(QStringLiteral("editor_instance_id")).toString() ==
                    editorInstanceId &&
                !lastEditorStatus.value(QStringLiteral("documents")).toArray().isEmpty();
            if (!editorStatusReady)
                QThread::msleep(100);
        }
        if (!editorStatusReady) {
            return fail(
                QStringLiteral("Editor status did not preserve bootstrap identity and document "
                               "discovery; expected_editor_instance_id=%1; status=%2; response=%3")
                    .arg(editorInstanceId,
                         QString::fromUtf8(
                             QJsonDocument(lastEditorStatus).toJson(QJsonDocument::Compact)),
                         QString::fromUtf8(QJsonDocument(lastEditorStatusResponse)
                                               .toJson(QJsonDocument::Compact))));
        }

        const auto failWithProcessDiagnostics = [&](const QString &message) {
            return fail(
                QStringLiteral("%1; appdata=%2; endpoint=%3; connector_state=%4; editor_state=%5; "
                               "connector_stderr=%6; editor_stderr=%7")
                    .arg(message, appDataRoot, editorEndpoint)
                    .arg(static_cast<int>(connector.state()))
                    .arg(static_cast<int>(editor.state()))
                    .arg(QString::fromUtf8(connector.readAllStandardError()),
                         QString::fromUtf8(editor.readAllStandardError())));
        };

        DsConnector::UpstreamMcpClient directClient(QStringLiteral("process-test-direct"),
                                                    QStringLiteral("1"));
        QString directEndpointError;
        if (!directClient.setEndpoint(editorEndpoint, &directEndpointError)) {
            return failWithProcessDiagnostics(
                QStringLiteral("Could not configure direct editor MCP client: %1")
                    .arg(directEndpointError));
        }

        QString toolError;
        const auto directInitialStatus = directToolContent(
            directClient, QStringLiteral("application.get_status"), {}, 10000, toolError);
        if (!directInitialStatus ||
            !equivalentContent(QStringLiteral("application.get_status"), lastEditorStatus,
                               directInitialStatus.value_or(QJsonObject{}), toolError)) {
            return failWithProcessDiagnostics(
                QStringLiteral("Initial direct/connector status equivalence failed: %1")
                    .arg(toolError));
        }

        const auto documents = lastEditorStatus.value(QStringLiteral("documents")).toArray();
        QJsonObject activeDocument;
        for (const auto &document : documents) {
            if (document.toObject().value(QStringLiteral("active")).toBool()) {
                activeDocument = document.toObject();
                break;
            }
        }
        if (activeDocument.isEmpty() && !documents.isEmpty())
            activeDocument = documents.first().toObject();
        const auto documentId = activeDocument.value(QStringLiteral("document_id")).toString();
        const auto initialRevision = activeDocument.value(QStringLiteral("revision")).toInteger(-1);
        if (documentId.isEmpty() || initialRevision < 0) {
            return failWithProcessDiagnostics(
                QStringLiteral(
                    "application.get_status did not provide a usable document version: %1")
                    .arg(compactJson(lastEditorStatus)));
        }

        const QJsonObject documentArguments{
            {QStringLiteral("document_id"), documentId}
        };
        const auto initialDocument = connectorToolContent(
            connector, 999, QStringLiteral("documents.get"), documentArguments, 10000, toolError);
        if (!initialDocument) {
            return failWithProcessDiagnostics(
                QStringLiteral("Initial connector documents.get failed: %1").arg(toolError));
        }
        const auto initialStatistics = initialDocument->value(QStringLiteral("snapshot"))
                                           .toObject()
                                           .value(QStringLiteral("statistics"))
                                           .toObject();

        const auto trackClientRef = QStringLiteral("process-integration-track");
        const auto trackName = QStringLiteral("MCP Process Integration Track");
        const QJsonObject insertArguments{
            {QStringLiteral("document_id"),       documentId                   },
            {QStringLiteral("expected_revision"), initialRevision              },
            {QStringLiteral("index"),             0                            },
            {QStringLiteral("tracks"),            QJsonArray{QJsonObject{
                                           {QStringLiteral("client_ref"), trackClientRef},
                                           {QStringLiteral("name"), trackName},
                                           {QStringLiteral("color_index"), 0},
                                       }}},
        };
        const auto mutation = connectorToolContent(connector, 1000, QStringLiteral("tracks.insert"),
                                                   insertArguments, 10000, toolError);
        if (!mutation) {
            return failWithProcessDiagnostics(
                QStringLiteral("Connector tracks.insert failed: %1").arg(toolError));
        }
        const auto previous = mutation->value(QStringLiteral("previous")).toObject();
        const auto current = mutation->value(QStringLiteral("current")).toObject();
        const auto currentRevision = current.value(QStringLiteral("revision")).toInteger(-1);
        qint64 createdTrackId = -1;
        for (const auto &createdValue :
             mutation->value(QStringLiteral("created_objects")).toArray()) {
            const auto created = createdValue.toObject();
            const auto object = created.value(QStringLiteral("object")).toObject();
            if (created.value(QStringLiteral("client_ref")).toString() == trackClientRef &&
                object.value(QStringLiteral("kind")).toString() == QStringLiteral("track")) {
                createdTrackId = object.value(QStringLiteral("id")).toInteger(-1);
                break;
            }
        }
        if (previous.value(QStringLiteral("document_id")).toString() != documentId ||
            previous.value(QStringLiteral("revision")).toInteger(-1) != initialRevision ||
            current.value(QStringLiteral("document_id")).toString() != documentId ||
            currentRevision != initialRevision + 1 ||
            !mutation->value(QStringLiteral("changed")).toBool() ||
            mutation->value(QStringLiteral("validated_only")).toBool() || createdTrackId < 0) {
            return failWithProcessDiagnostics(
                QStringLiteral("tracks.insert returned an invalid mutation/revision result: %1")
                    .arg(compactJson(*mutation)));
        }

        const auto connectorDocument = connectorToolContent(
            connector, 1001, QStringLiteral("documents.get"), documentArguments, 10000, toolError);
        if (!connectorDocument) {
            return failWithProcessDiagnostics(
                QStringLiteral("Connector documents.get failed: %1").arg(toolError));
        }
        const auto statistics = connectorDocument->value(QStringLiteral("snapshot"))
                                    .toObject()
                                    .value(QStringLiteral("statistics"))
                                    .toObject();
        if (connectorDocument->value(QStringLiteral("document"))
                    .toObject()
                    .value(QStringLiteral("revision"))
                    .toInteger(-1) != currentRevision ||
            statistics.value(QStringLiteral("track_count")).toInteger(-1) !=
                initialStatistics.value(QStringLiteral("track_count")).toInteger(-1) + 1 ||
            statistics.value(QStringLiteral("empty_track_count")).toInteger(-1) !=
                initialStatistics.value(QStringLiteral("empty_track_count")).toInteger(-1) + 1 ||
            statistics.value(QStringLiteral("clip_count")).toInteger(-1) !=
                initialStatistics.value(QStringLiteral("clip_count")).toInteger(-1)) {
            return failWithProcessDiagnostics(
                QStringLiteral(
                    "documents.get did not expose the inserted empty track statistics at the new "
                    "revision: %1")
                    .arg(compactJson(*connectorDocument)));
        }
        const auto directDocument = directToolContent(directClient, QStringLiteral("documents.get"),
                                                      documentArguments, 10000, toolError);
        if (!directDocument ||
            !equivalentContent(QStringLiteral("documents.get"), *connectorDocument,
                               directDocument.value_or(QJsonObject{}), toolError)) {
            return failWithProcessDiagnostics(
                QStringLiteral("Direct/connector documents.get equivalence failed: %1")
                    .arg(toolError));
        }

        const auto connectorFormats = connectorToolContent(
            connector, 1002, QStringLiteral("formats.list"), {}, 10000, toolError);
        if (!connectorFormats || !connectorFormats->value(QStringLiteral("formats")).isArray()) {
            return failWithProcessDiagnostics(
                QStringLiteral("Connector formats.list failed or returned an invalid payload: %1")
                    .arg(toolError));
        }
        QProcess legacyConnector;
        const auto legacyConnectorCleanup = qScopeGuard([&legacyConnector] {
            legacyConnector.closeWriteChannel();
            if (!legacyConnector.waitForFinished(3000))
                stopProcess(legacyConnector);
        });
        legacyConnector.setProcessEnvironment(environment);
        legacyConnector.setWorkingDirectory(QFileInfo(connectorPath).absolutePath());
        legacyConnector.setProcessChannelMode(QProcess::SeparateChannels);
        legacyConnector.start(connectorPath,
                              {QStringLiteral("--exposure-profile"), QStringLiteral("l3")});
        if (!legacyConnector.waitForStarted(10000)) {
            return failWithProcessDiagnostics(
                QStringLiteral("Legacy-protocol Connector failed to start: %1")
                    .arg(legacyConnector.errorString()));
        }

        const auto legacyContext = legacyRequestContext();
        const auto legacyInitialize = exchange(legacyConnector,
                                               AutomationWire::Mcp::makeInitializeRequest(
                                                   legacyContext, QStringLiteral("legacy-init")),
                                               10000, exchangeError);
        if (!legacyInitialize || legacyInitialize->contains(QStringLiteral("error"))) {
            return failWithProcessDiagnostics(
                QStringLiteral("Connector MCP 2025-06-18 initialize failed: %1")
                    .arg(exchangeError));
        }
        const auto legacyInitializeResult =
            legacyInitialize->value(QStringLiteral("result")).toObject();
        if (legacyInitializeResult.value(QStringLiteral("protocolVersion")) !=
                QString::fromLatin1(AutomationWire::Mcp::CompatibilityProtocolVersion) ||
            legacyInitializeResult.contains(QStringLiteral("resultType"))) {
            return failWithProcessDiagnostics(
                QStringLiteral("Connector returned an invalid MCP 2025-06-18 initialize result: %1")
                    .arg(compactJson(*legacyInitialize)));
        }
        if (!writeMessage(legacyConnector,
                          AutomationWire::Mcp::makeRequest(
                              QString::fromLatin1(AutomationWire::Mcp::InitializedNotification), {},
                              legacyContext),
                          exchangeError)) {
            return failWithProcessDiagnostics(
                QStringLiteral("Connector MCP 2025-06-18 initialized notification failed: %1")
                    .arg(exchangeError));
        }

        bool legacyConnectorReady = false;
        QJsonObject legacyConnectorStatus;
        for (qint64 attempt = 0; attempt < 100 && !legacyConnectorReady; ++attempt) {
            const auto status = exchange(
                legacyConnector,
                makeLegacyToolRequest(20000 + attempt, QStringLiteral("connector.get_status")),
                5000, exchangeError);
            if (!status || status->contains(QStringLiteral("error"))) {
                return failWithProcessDiagnostics(
                    QStringLiteral("Legacy Connector status failed: %1").arg(exchangeError));
            }
            const auto result = status->value(QStringLiteral("result")).toObject();
            if (result.contains(QStringLiteral("resultType"))) {
                return failWithProcessDiagnostics(
                    QStringLiteral("MCP 2025-06-18 Connector result leaked 2026 metadata: %1")
                        .arg(compactJson(*status)));
            }
            legacyConnectorStatus = result.value(QStringLiteral("structuredContent")).toObject();
            const auto compatibility = legacyConnectorStatus.value(QStringLiteral("toolset"))
                                           .toObject()
                                           .value(QStringLiteral("compatibility"))
                                           .toString();
            legacyConnectorReady = legacyConnectorStatus.value(QStringLiteral("mcp"))
                                       .toObject()
                                       .value(QStringLiteral("connected"))
                                       .toBool() &&
                                   compatibility == QStringLiteral("compatible");
            if (!legacyConnectorReady)
                QThread::msleep(100);
        }
        if (!legacyConnectorReady) {
            return failWithProcessDiagnostics(
                QStringLiteral("MCP 2025-06-18 Connector did not become ready: %1")
                    .arg(compactJson(legacyConnectorStatus)));
        }

        const auto legacyTools = exchange(
            legacyConnector,
            makeLegacyRequest(20150, QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod)),
            10000, exchangeError);
        QSet<QString> legacyToolNames;
        if (legacyTools) {
            for (const auto &tool : legacyTools->value(QStringLiteral("result"))
                                        .toObject()
                                        .value(QStringLiteral("tools"))
                                        .toArray()) {
                legacyToolNames.insert(tool.toObject().value(QStringLiteral("name")).toString());
            }
        }
        if (!legacyTools || legacyTools->contains(QStringLiteral("error")) ||
            legacyTools->value(QStringLiteral("result"))
                .toObject()
                .contains(QStringLiteral("resultType")) ||
            legacyToolNames != toolNames) {
            return failWithProcessDiagnostics(
                QStringLiteral("Connector MCP 2025-06-18 tools/list failed: %1")
                    .arg(legacyTools ? compactJson(*legacyTools) : exchangeError));
        }

        DsConnector::UpstreamMcpClient legacyDirectClient(
            QStringLiteral("process-test-direct-legacy"), QStringLiteral("1"));
        if (!legacyDirectClient.setEndpoint(editorEndpoint, &directEndpointError) ||
            !legacyDirectClient.setProtocolVersion(
                QString::fromLatin1(AutomationWire::Mcp::LegacyProtocolVersion))) {
            return failWithProcessDiagnostics(
                QStringLiteral("Could not configure direct MCP 2025-11-25 editor client: %1")
                    .arg(directEndpointError));
        }
        const QJsonObject legacyInitializeParams{
            {QStringLiteral("protocolVersion"),
             QString::fromLatin1(AutomationWire::Mcp::LegacyProtocolVersion)      },
            {QStringLiteral("capabilities"),    QJsonObject{}                     },
            {QStringLiteral("clientInfo"),      legacyContext.clientInfo->toJson()},
        };
        std::optional<DsConnector::UpstreamResult> legacyDirectInitialize;
        legacyDirectClient.send(
            QString::fromLatin1(AutomationWire::Mcp::InitializeMethod), legacyInitializeParams,
            [&legacyDirectInitialize](DsConnector::UpstreamResult result) {
                legacyDirectInitialize = std::move(result);
            },
            10000);
        if (!waitUntil([&legacyDirectInitialize] { return legacyDirectInitialize.has_value(); },
                       11000) ||
            !legacyDirectInitialize->succeeded() ||
            legacyDirectInitialize->result.value(QStringLiteral("protocolVersion")) !=
                QString::fromLatin1(AutomationWire::Mcp::LegacyProtocolVersion) ||
            legacyDirectInitialize->result.contains(QStringLiteral("resultType"))) {
            return failWithProcessDiagnostics(
                QStringLiteral("Direct Editor MCP 2025-11-25 initialize failed"));
        }
        std::optional<DsConnector::UpstreamResult> legacyDirectInitialized;
        legacyDirectClient.sendNotification(
            QString::fromLatin1(AutomationWire::Mcp::InitializedNotification), {},
            [&legacyDirectInitialized](DsConnector::UpstreamResult result) {
                legacyDirectInitialized = std::move(result);
            },
            10000);
        if (!waitUntil([&legacyDirectInitialized] { return legacyDirectInitialized.has_value(); },
                       11000) ||
            !legacyDirectInitialized->succeeded()) {
            return failWithProcessDiagnostics(
                QStringLiteral("Direct Editor MCP 2025-11-25 initialized notification failed"));
        }
        std::optional<DsConnector::UpstreamResult> legacyDirectTools;
        legacyDirectClient.send(
            QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod), {},
            [&legacyDirectTools](DsConnector::UpstreamResult result) {
                legacyDirectTools = std::move(result);
            },
            10000);
        if (!waitUntil([&legacyDirectTools] { return legacyDirectTools.has_value(); }, 11000) ||
            !legacyDirectTools->succeeded() ||
            legacyDirectTools->result.contains(QStringLiteral("resultType")) ||
            legacyDirectTools->result.contains(QStringLiteral("ttlMs")) ||
            legacyDirectTools->result.value(QStringLiteral("tools")).toArray().isEmpty()) {
            return failWithProcessDiagnostics(
                QStringLiteral("Direct Editor MCP 2025-11-25 tools/list failed"));
        }
        const auto legacyDirectStatus = directToolContent(
            legacyDirectClient, QStringLiteral("application.get_status"), {}, 10000, toolError);
        if (!legacyDirectStatus ||
            legacyDirectStatus->value(QStringLiteral("editor_instance_id")).toString() !=
                editorInstanceId) {
            return failWithProcessDiagnostics(
                QStringLiteral("Direct Editor MCP 2025-11-25 tools/call failed: %1")
                    .arg(toolError));
        }

        const auto rejectedExit = exchange(
            connector,
            makeToolRequest(30000, QStringLiteral("application.request_exit"), QJsonObject{}),
            10000, exchangeError);
        const auto rejectedExitResult =
            rejectedExit ? rejectedExit->value(QStringLiteral("result")).toObject() : QJsonObject{};
        const auto rejectedExitContent =
            rejectedExitResult.value(QStringLiteral("structuredContent")).toObject();
        if (!rejectedExit || !rejectedExitResult.value(QStringLiteral("isError")).toBool() ||
            rejectedExitContent.value(QStringLiteral("code")).toString() !=
                QStringLiteral("busy") ||
            rejectedExitContent.value(QStringLiteral("field_path")).toString() !=
                QStringLiteral("discard_changes") ||
            editor.state() != QProcess::Running) {
            return failWithProcessDiagnostics(
                QStringLiteral("Dirty editor exit was not rejected non-interactively: %1")
                    .arg(rejectedExit ? compactJson(*rejectedExit) : exchangeError));
        }

        const auto acceptedExit =
            connectorToolContent(connector, 30001, QStringLiteral("application.request_exit"),
                                 QJsonObject{
                                     {QStringLiteral("discard_changes"), true}
        },
                                 10000, exchangeError);
        if (!acceptedExit || !acceptedExit->value(QStringLiteral("accepted")).toBool() ||
            acceptedExit->value(QStringLiteral("action")).toString() != QStringLiteral("exit") ||
            !acceptedExit->value(QStringLiteral("discard_changes")).toBool() ||
            !editor.waitForFinished(15000)) {
            return failWithProcessDiagnostics(
                QStringLiteral("Forced graceful editor exit did not complete: %1")
                    .arg(acceptedExit ? compactJson(*acceptedExit) : exchangeError));
        }

        QTextStream(stdout)
            << "Validated real editor + connector MCP 2025-06-18/2025-11-25/2026-07-28 process "
               "integration, representative operations, the complete L3 surface, direct-editor "
               "equivalence, and non-interactive graceful exit"
            << Qt::endl;
        return true;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    if (application.arguments().size() != 3) {
        QTextStream(stderr) << "FAILED: Expected editor and connector executable paths" << Qt::endl;
        return 2;
    }
    return runIntegration(application.arguments().at(1), application.arguments().at(2)) ? 0 : 1;
}
