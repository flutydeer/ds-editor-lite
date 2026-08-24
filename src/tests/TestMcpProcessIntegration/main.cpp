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
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QSet>
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
             QString::fromLatin1(AutomationWire::Mcp::ProtocolVersion)},
            {QStringLiteral("io.modelcontextprotocol/clientCapabilities"), QJsonObject{}},
            {QStringLiteral("io.modelcontextprotocol/clientInfo"),
             QJsonObject{
                 {QStringLiteral("name"), QStringLiteral("phase-two-process-test")},
                 {QStringLiteral("version"), QStringLiteral("1")},
             }},
        };
    }

    QJsonObject makeRequest(const qint64 id, const QString &method, QJsonObject params = {}) {
        params.insert(QStringLiteral("_meta"), requestMeta());
        return {
            {QStringLiteral("jsonrpc"),
             QString::fromLatin1(AutomationWire::Mcp::JsonRpcVersion)},
            {QStringLiteral("id"), id},
            {QStringLiteral("method"), method},
            {QStringLiteral("params"), params},
        };
    }

    QJsonObject makeToolRequest(const qint64 id, const QString &name,
                                const QJsonObject &arguments = {}) {
        return makeRequest(id, QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                           QJsonObject{
                               {QStringLiteral("name"), name},
                               {QStringLiteral("arguments"), arguments},
                           });
    }

    std::optional<QJsonObject> exchange(QProcess &process, const QJsonObject &request,
                                        const int timeoutMilliseconds, QString &error) {
        auto bytes = QJsonDocument(request).toJson(QJsonDocument::Compact);
        bytes.append('\n');
        if (process.write(bytes) != bytes.size() ||
            (process.bytesToWrite() > 0 && !process.waitForBytesWritten(5000))) {
            error = QStringLiteral("Could not write a complete stdio MCP request");
            return std::nullopt;
        }

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
        client.send(QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod), {},
                    [&response](DsConnector::UpstreamResult result) {
                        response = std::move(result);
                    },
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
                .filePath(QStringLiteral("%1/%2")
                              .arg(QString::fromLatin1(LiteProductMetadata::Publisher),
                                   QString::fromLatin1(LiteProductMetadata::ProductName)));
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
        editor.start(editorPath,
                     {QStringLiteral("--mcp"), QStringLiteral("--control-port"),
                      QStringLiteral("0"), QStringLiteral("--automation-profile"),
                      QStringLiteral("l2")});
        if (!editor.waitForStarted(10000)) {
            return fail(QStringLiteral("Editor failed to start: %1").arg(editor.errorString()));
        }

        DsConnector::BootstrapWatcher watcher(
            QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("1"), serviceName);
        watcher.start();
        const auto watcherCleanup = qScopeGuard([&watcher] { watcher.stop(); });
        const auto editorSettled = waitUntil(
            [&] {
                const auto &observation = watcher.observation();
                return observation.snapshot &&
                           observation.snapshot->result.state ==
                               SingleInstanceAutomationState::McpReady ||
                       editor.state() == QProcess::NotRunning;
            },
            45000);
        const auto ready = editorSettled && watcher.observation().snapshot &&
                           watcher.observation().snapshot->result.state ==
                               SingleInstanceAutomationState::McpReady;
        if (!ready) {
            const auto &observation = watcher.observation();
            return fail(
                QStringLiteral("Editor did not publish mcp_ready; appdata=%1; service=%2; "
                               "bootstrap=%3; process_state=%4; exit_status=%5; exit_code=%6; "
                               "stdout=%7; stderr=%8")
                    .arg(appDataRoot, serviceName, observation.error)
                    .arg(static_cast<int>(editor.state()))
                    .arg(static_cast<int>(editor.exitStatus()))
                    .arg(editor.exitCode())
                    .arg(QString::fromUtf8(editor.readAllStandardOutput()),
                         QString::fromUtf8(editor.readAllStandardError())));
        }
        const auto editorInstanceId =
            watcher.observation().snapshot->result.editorInstanceId;
        const auto editorEndpoint = watcher.observation().snapshot->result.mcpEndpoint;

        connector.setProcessEnvironment(environment);
        connector.setWorkingDirectory(QFileInfo(connectorPath).absolutePath());
        connector.setProcessChannelMode(QProcess::SeparateChannels);
        connector.start(connectorPath,
                        {QStringLiteral("--exposure-profile"), QStringLiteral("l2")});
        if (!connector.waitForStarted(10000)) {
            return fail(
                QStringLiteral("Connector failed to start: %1").arg(connector.errorString()));
        }

        QString exchangeError;
        auto discover = exchange(
            connector,
            makeRequest(1, QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod)), 10000,
            exchangeError);
        if (!discover || discover->contains(QStringLiteral("error"))) {
            return fail(QStringLiteral("Connector discover failed: %1").arg(exchangeError));
        }

        bool connectorReady = false;
        QJsonObject lastConnectorStatus;
        for (qint64 attempt = 0; attempt < 100 && !connectorReady; ++attempt) {
            auto status = exchange(connector,
                                   makeToolRequest(10 + attempt,
                                                   QStringLiteral("connector.get_status")),
                                   5000, exchangeError);
            if (!status) {
                return fail(exchangeError);
            }
            const auto content = structuredContent(*status).toObject();
            lastConnectorStatus = content;
            const auto manifestCompatibility = content.value(QStringLiteral("manifest"))
                                                   .toObject()
                                                   .value(QStringLiteral("compatibility"))
                                                   .toString();
            connectorReady =
                content.value(QStringLiteral("mcp"))
                    .toObject()
                    .value(QStringLiteral("connected"))
                    .toBool() &&
                (manifestCompatibility == QStringLiteral("compatible") ||
                 manifestCompatibility == QStringLiteral("compatible_subset"));
            if (!connectorReady)
                QThread::msleep(100);
        }
        if (!connectorReady) {
            return fail(QStringLiteral("Connector did not complete the upstream editor handshake; "
                                       "status=%1; upstream_tools=%2; connector_stderr=%3; "
                                       "editor_stderr=%4")
                            .arg(QString::fromUtf8(QJsonDocument(lastConnectorStatus)
                                                       .toJson(QJsonDocument::Compact)),
                                 upstreamToolsDiagnostic(editorEndpoint),
                                 QString::fromUtf8(connector.readAllStandardError()),
                                 QString::fromUtf8(editor.readAllStandardError())));
        }

        auto tools = exchange(
            connector,
            makeRequest(100, QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod)), 10000,
            exchangeError);
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
            QStringLiteral("connector.get_status"), QStringLiteral("connector.reconnect"),
            QStringLiteral("editor.tools.list"),    QStringLiteral("editor.tools.search"),
            QStringLiteral("editor.tools.describe"), QStringLiteral("editor.tools.invoke"),
        };
        const auto missingFixedTool =
            std::find_if(fixedToolNames.cbegin(), fixedToolNames.cend(),
                         [&toolNames](const QString &name) { return !toolNames.contains(name); });
        if (missingFixedTool != fixedToolNames.cend() ||
            !toolNames.contains(QStringLiteral("automation.get_status")) ||
            toolNames.contains(QStringLiteral("application.request_exit"))) {
            return fail(QStringLiteral("Connector published an unexpected fixed tool surface"));
        }

        QJsonObject lastEditorStatus;
        QJsonObject lastEditorStatusResponse;
        bool editorStatusReady = false;
        for (qint64 attempt = 0; attempt < 100 && !editorStatusReady; ++attempt) {
            auto editorStatus = exchange(
                connector,
                makeToolRequest(101 + attempt, QStringLiteral("automation.get_status")), 5000,
                exchangeError);
            if (!editorStatus) {
                return fail(QStringLiteral("automation.get_status failed: %1").arg(exchangeError));
            }
            lastEditorStatusResponse = *editorStatus;
            if (editorStatus->contains(QStringLiteral("error")) ||
                editorStatus->value(QStringLiteral("result"))
                    .toObject()
                    .value(QStringLiteral("isError"))
                    .toBool()) {
                return fail(QStringLiteral("automation.get_status returned an error: %1")
                                .arg(QString::fromUtf8(QJsonDocument(*editorStatus)
                                                           .toJson(QJsonDocument::Compact))));
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
                         QString::fromUtf8(QJsonDocument(lastEditorStatus)
                                               .toJson(QJsonDocument::Compact)),
                         QString::fromUtf8(QJsonDocument(lastEditorStatusResponse)
                                               .toJson(QJsonDocument::Compact))));
        }

        QTextStream(stdout) << "Validated real editor + connector MCP process integration"
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
