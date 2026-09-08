#include <Bootstrap/SingleInstanceIdentity.h>
#include <BootstrapWatcher.h>
#include <UpstreamMcpClient.h>

#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/McpProtocol.h>
#include <lite/ProductMetadata.h>

#include <QCoreApplication>
#include <QtTest>
#include "../TestSupport/ProcessFixture.h"
#include <lite/AutomationWire/PublicToolContract.h>
#include <QDataStream>
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
    using TestSupport::ProcessFixture;
    using TestSupport::stopProcess;
    using TestSupport::waitUntil;

    bool fail(const QString &message) {
        QTest::qFail(qPrintable(message), __FILE__, __LINE__);
        return false;
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
        TestSupport::recordProcessMessage(process, "sent", bytes);
        bytes.append('\n');
        if (process.write(bytes) != bytes.size() ||
            (process.bytesToWrite() > 0 && !process.waitForBytesWritten(5000))) {
            error = QStringLiteral("Could not write a complete stdio MCP message");
            return false;
        }
        return true;
    }

    std::optional<QJsonObject> readResponse(QProcess &process, const QJsonValue &requestId,
                                            const int timeoutMilliseconds, QString &error) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMilliseconds) {
            if (!process.canReadLine()) {
                process.waitForReadyRead(100);
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                continue;
            }
            const auto line = process.readLine().trimmed();
            TestSupport::recordProcessMessage(process, "received", line);
            QJsonParseError parseError;
            const auto document = QJsonDocument::fromJson(line, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                error = QStringLiteral("Connector stdout contained a non-JSON protocol line");
                return std::nullopt;
            }
            const auto response = document.object();
            if (response.value(QStringLiteral("id")) != requestId) {
                error = QStringLiteral("Connector returned an unexpected JSON-RPC id");
                return std::nullopt;
            }
            return response;
        }
        error = QStringLiteral("Timed out waiting for a connector response");
        return std::nullopt;
    }

    std::optional<QJsonObject> exchange(QProcess &process, const QJsonObject &request,
                                        const int timeoutMilliseconds, QString &error) {
        if (!writeMessage(process, request, error))
            return std::nullopt;
        return readResponse(process, request.value(QStringLiteral("id")), timeoutMilliseconds,
                            error);
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

    bool verifyDocumentLifecycle(QProcess &connector, const QString &directory,
                                 const qint64 audioTrackId) {
        qint64 requestId = 20000;
        QString error;
        QJsonObject document;
        QString windowId;
        const auto call = [&](const QString &name, const QJsonObject &arguments = {}) {
            auto result =
                connectorToolContent(connector, requestId++, name, arguments, 10000, error);
            if (!result)
                fail(QStringLiteral("Lifecycle %1 failed: %2").arg(name, error));
            return result;
        };
        const auto refresh = [&] {
            const auto status = call(QStringLiteral("application.get_status"));
            if (!status)
                return false;
            document = status->value(QStringLiteral("documents")).toArray().first().toObject();
            windowId = status->value(QStringLiteral("windows"))
                           .toArray()
                           .first()
                           .toObject()
                           .value(QStringLiteral("window_id"))
                           .toString();
            return !document.value(QStringLiteral("document_id")).toString().isEmpty();
        };
        const auto arguments = [&] {
            return QJsonObject{
                {QStringLiteral("document_id"),       document.value("document_id")},
                {QStringLiteral("expected_revision"), document.value("revision")   }
            };
        };
        const auto snapshot = [&] {
            return call(QStringLiteral("documents.get"),
                        {
                            {QStringLiteral("document_id"), document.value("document_id")}
            });
        };
        const auto checkDocument = [&](const QString &path, const bool dirty) {
            const auto result = snapshot();
            const auto state = result ? result->value("snapshot").toObject() : QJsonObject{};
            return result && state.value("path").toString() == path &&
                   state.value("dirty").toBool() == dirty &&
                   state.value("saved").toBool() != dirty &&
                   state.value("lifecycle").toString() == QStringLiteral("active");
        };
        const auto activeClip = [&] {
            return call(QStringLiteral("clip_editor.get_state"),
                        {
                            {QStringLiteral("document_id"), document.value("document_id")},
                            {QStringLiteral("window_id"),   windowId                     }
            });
        };
        const auto checkActiveClip = [&](const bool singing) {
            const auto editor = activeClip();
            if (!editor)
                return false;
            const auto id = editor->value("active_clip_id");
            if (!singing)
                return id.isNull();
            const auto clips =
                call(QStringLiteral("clips.list"),
                     {
                         {QStringLiteral("document_id"), document.value("document_id")},
                         {QStringLiteral("type"),        QStringLiteral("singing")    }
            });
            const auto items = clips ? clips->value("clips").toArray() : QJsonArray{};
            return !items.isEmpty() && id == items.first().toObject().value("clip_id");
        };
        const auto saveAs = [&](const QString &path) {
            if (!refresh())
                return false;
            const auto before = activeClip();
            auto input = arguments();
            input.insert(QStringLiteral("path"), path);
            input.insert(QStringLiteral("overwrite_policy"), QStringLiteral("reject"));
            const auto saved = call(QStringLiteral("documents.save_as"), input);
            const auto after = activeClip();
            return saved && before && after && checkDocument(path, false) &&
                   before->value("active_clip_id") == after->value("active_clip_id");
        };
        const auto create = [&](const QString &preset) {
            if (!refresh())
                return false;
            const auto result =
                call(QStringLiteral("documents.new"),
                     {
                         {QStringLiteral("current_document_id"), document.value("document_id")},
                         {QStringLiteral("expected_revision"),   document.value("revision")   },
                         {QStringLiteral("unsaved_policy"),      QStringLiteral("discard")    },
                         {QStringLiteral("template"),            preset                       }
            });
            return result && refresh() && checkDocument({}, false);
        };
        const auto open = [&](const QString &path) {
            if (!refresh())
                return false;
            const auto previous = document.value("document_id");
            const auto started =
                call(QStringLiteral("documents.open"),
                     {
                         {QStringLiteral("current_document_id"), previous                  },
                         {QStringLiteral("expected_revision"),   document.value("revision")},
                         {QStringLiteral("unsaved_policy"),      QStringLiteral("reject")  },
                         {QStringLiteral("path"),                path                      }
            });
            if (!started)
                return false;
            const auto taskId = started->value("task_id").toString();
            for (int attempt = 0; attempt < 100; ++attempt) {
                if (!refresh())
                    return false;
                const auto task = connectorToolContent(
                    connector, requestId++, QStringLiteral("tasks.get"),
                    {
                        {QStringLiteral("scope"),       QStringLiteral("document")   },
                        {QStringLiteral("document_id"), document.value("document_id")},
                        {QStringLiteral("task_id"),     taskId                       }
                },
                    5000, error);
                if (!task) {
                    if (error.contains(QStringLiteral("document_changed")))
                        continue;
                    return fail(error);
                }
                const auto state = task->value("state").toString();
                if (state == QStringLiteral("succeeded"))
                    return document.value("document_id") != previous;
                if (state == QStringLiteral("failed") || state == QStringLiteral("canceled"))
                    return fail(
                        QStringLiteral("Lifecycle open task failed: %1").arg(compactJson(*task)));
                QThread::msleep(50);
            }
            return fail(QStringLiteral("Lifecycle open task timed out"));
        };

        if (!refresh())
            return false;
        const auto tracks =
            call(QStringLiteral("tracks.list"),
                 {
                     {QStringLiteral("document_id"), document.value("document_id")}
        });
        if (!tracks)
            return false;
        for (const auto &track : tracks->value("tracks").toArray()) {
            const auto id = track.toObject().value("track_id").toInteger();
            if (id == audioTrackId)
                continue;
            if (!refresh())
                return false;
            auto input = arguments();
            input.insert(QStringLiteral("track_ids"), QJsonArray{id});
            if (!call(QStringLiteral("tracks.remove"), input))
                return false;
        }
        const auto audioProject = QDir(directory).filePath(QStringLiteral("audio-only.dspx"));
        const auto firstPath = QDir(directory).filePath(QStringLiteral("生命周期甲.dspx"));
        const auto secondPath = QDir(directory).filePath(QStringLiteral("生命周期乙.dspx"));
        if (!saveAs(audioProject) || !create(QStringLiteral("default")) || !checkActiveClip(true) ||
            !saveAs(firstPath) || !saveAs(secondPath)) {
            return fail(
                QStringLiteral("Lifecycle new/save-as did not preserve document or selection"));
        }
        const auto recent = call(QStringLiteral("documents.list_recent"));
        const auto projects = recent ? recent->value("projects").toArray() : QJsonArray{};
        if (projects.size() < 2 || projects.at(0).toObject().value("path") != secondPath ||
            projects.at(1).toObject().value("path") != firstPath) {
            return fail(
                QStringLiteral("Lifecycle save-as did not update recent projects in order"));
        }
        if (!create(QStringLiteral("empty")) || !checkActiveClip(false) || !open(firstPath) ||
            !checkDocument(firstPath, false) || !checkActiveClip(true)) {
            return fail(
                QStringLiteral("Lifecycle open did not restore its document and first clip"));
        }
        const auto reopenedRecent = call(QStringLiteral("documents.list_recent"));
        if (!reopenedRecent ||
            reopenedRecent->value("projects").toArray().size() != projects.size() ||
            reopenedRecent->value("projects").toArray().first().toObject().value("path") !=
                firstPath) {
            return fail(
                QStringLiteral("Lifecycle open must promote, not duplicate, its recent path"));
        }

        const auto openedTracks =
            call(QStringLiteral("tracks.list"),
                 {
                     {QStringLiteral("document_id"), document.value("document_id")}
        });
        if (!openedTracks || !refresh())
            return false;
        auto renameInput = arguments();
        renameInput.insert(
            QStringLiteral("track_id"),
            openedTracks->value("tracks").toArray().first().toObject().value("track_id"));
        renameInput.insert(QStringLiteral("name"), QStringLiteral("Lifecycle edited"));
        const auto selectedBeforeSave = activeClip();
        if (!call(QStringLiteral("tracks.rename"), renameInput) || !refresh() ||
            !checkDocument(firstPath, true) ||
            !call(QStringLiteral("documents.save"), arguments()) ||
            !checkDocument(firstPath, false)) {
            return fail(QStringLiteral("Lifecycle save did not clear the dirty state"));
        }
        const auto selectedAfterSave = activeClip();
        if (!selectedBeforeSave || !selectedAfterSave ||
            selectedBeforeSave->value("active_clip_id") !=
                selectedAfterSave->value("active_clip_id") ||
            !open(audioProject) || !checkDocument(audioProject, false) || !checkActiveClip(false)) {
            return fail(QStringLiteral("Lifecycle audio-only open retained a stale active clip"));
        }

        QFile midi(QDir(directory).filePath(QStringLiteral("来源.mid")));
        if (!midi.open(QIODevice::WriteOnly))
            return fail(QStringLiteral("Could not create lifecycle MIDI fixture"));
        midi.write(QByteArray::fromHex("4d546864000000060000000101e04d54726b0000001400ff510307a1200"
                                       "0903c648360803c0000ff2f00"));
        midi.close();
        const auto recentBeforeForeign = call(QStringLiteral("documents.list_recent"));
        if (!open(midi.fileName()) || !checkDocument({}, true) || !checkActiveClip(true))
            return fail(
                QStringLiteral("Foreign open must be unsaved and activate its singing clip"));
        const auto foreign = snapshot();
        const auto recentAfterForeign = call(QStringLiteral("documents.list_recent"));
        if (!foreign ||
            foreign->value("snapshot").toObject().value("project_name") !=
                QFileInfo(midi.fileName()).completeBaseName() ||
            !recentBeforeForeign || !recentAfterForeign ||
            *recentBeforeForeign != *recentAfterForeign) {
            return fail(QStringLiteral("Foreign lifecycle identity/recent mismatch: %1; %2; %3")
                            .arg(foreign ? compactJson(*foreign) : error,
                                 recentBeforeForeign ? compactJson(*recentBeforeForeign) : error,
                                 recentAfterForeign ? compactJson(*recentAfterForeign) : error));
        }
        return true;
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

    std::optional<QSet<QString>> directToolCatalog(DsConnector::UpstreamMcpClient &client,
                                                   const int timeoutMilliseconds, QString &error) {
        error.clear();
        QSet<QString> names;
        QString cursor;
        for (int pageIndex = 0; pageIndex < 16; ++pageIndex) {
            QJsonObject params;
            if (!cursor.isEmpty())
                params.insert(QStringLiteral("cursor"), cursor);
            std::optional<DsConnector::UpstreamResult> response;
            client.send(
                QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod), std::move(params),
                [&response](DsConnector::UpstreamResult result) { response = std::move(result); },
                timeoutMilliseconds);
            if (!waitUntil([&response] { return response.has_value(); },
                           timeoutMilliseconds + 1000)) {
                error = QStringLiteral("Direct editor tools/list timed out");
                return std::nullopt;
            }
            if (!response->connectorError.isEmpty()) {
                error = QStringLiteral("Direct editor tools/list transport error: %1 (%2)")
                            .arg(response->connectorError, response->connectorErrorMessage);
                return std::nullopt;
            }
            if (response->protocolError) {
                error = QStringLiteral("Direct editor tools/list JSON-RPC error %1: %2")
                            .arg(response->protocolError->code)
                            .arg(response->protocolError->message);
                return std::nullopt;
            }
            const auto tools = response->result.value(QStringLiteral("tools")).toArray();
            for (const auto &tool : tools) {
                const auto name = tool.toObject().value(QStringLiteral("name")).toString();
                const auto issue = descriptorIssue(tool);
                if (name.isEmpty() || !issue.isEmpty() || names.contains(name)) {
                    error =
                        QStringLiteral("Direct editor tools/list returned an invalid tool %1: %2")
                            .arg(name, issue.isEmpty() ? QStringLiteral("duplicate name") : issue);
                    return std::nullopt;
                }
                names.insert(name);
            }
            cursor = response->result.value(QStringLiteral("nextCursor")).toString();
            if (cursor.isEmpty())
                return names;
        }
        error = QStringLiteral("Direct editor tools/list pagination did not terminate");
        return std::nullopt;
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

    class EditorConnectorFixture final {
    public:
        const QString editorPath;
        const QString connectorPath;
        ProcessFixture storage;
        const QString appDataRoot = storage.path();
        const QString editorDataDirectory = storage.dataDirectory();
        const QString configPath =
            QDir(editorDataDirectory).filePath(QStringLiteral("appConfig.json"));
        const QString serviceName = SingleInstanceIdentity::serviceName(editorDataDirectory);
        const QString audioPath = storage.filePath(QStringLiteral("import-fixture.wav"));
        QProcessEnvironment environment;
        QProcess &editor = storage.process(QStringLiteral("editor"));
        QProcess &connector = storage.process(QStringLiteral("connector"));
        DsConnector::BootstrapWatcher watcher{QUuid::createUuid().toString(QUuid::WithoutBraces),
                                              QStringLiteral("1"), serviceName};
        QString editorInstanceId;
        QString editorEndpoint;
        QString error;
        QJsonObject lastEditorStatus;

        EditorConnectorFixture(const QString &editor, const QString &connector, const QString &name)
            : editorPath(editor), connectorPath(connector), storage(name) {
        }

        ~EditorConnectorFixture() {
            watcher.stop();
            if (connector.state() != QProcess::NotRunning) {
                connector.closeWriteChannel();
                if (!connector.waitForFinished(3000))
                    stopProcess(connector);
            }
            stopProcess(editor);
        }

        bool start() {
            if (!storage.isValid() || !ProcessFixture::writeWaveFixture(audioPath) ||
                !storage.writeConfig({
                    {QStringLiteral("audio"),
                     QJsonObject{
                         {QStringLiteral("deviceName"), QStringLiteral("configured-only-device")}}},
                    {QStringLiteral("automation"),
                     QJsonObject{{QStringLiteral("accessRoots"), QJsonArray{storage.path()}}}     }
            }))
                return fail(QStringLiteral("Could not prepare editor/connector sandbox"));
            environment = storage.environment();
            environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
            environment.insert(QStringLiteral("QT_OPENGL"), QStringLiteral("software"));
            QTcpServer reservation;
            if (!reservation.listen(QHostAddress::LocalHost, 0))
                return fail(reservation.errorString());
            const auto port = reservation.serverPort();
            reservation.close();
            editor.setProcessEnvironment(environment);
            editor.setWorkingDirectory(QFileInfo(editorPath).absolutePath());
            editor.start(editorPath, {QStringLiteral("--mcp"), QStringLiteral("--control-level"),
                                      QStringLiteral("l3"), QStringLiteral("--control-port"),
                                      QString::number(port)});
            if (!editor.waitForStarted(10000))
                return fail(editor.errorString());
            watcher.start();
            if (!waitUntil(
                    [&] {
                        return (watcher.observation().snapshot &&
                                watcher.observation().snapshot->result.state ==
                                    SingleInstanceAutomationState::ServerReady) ||
                               editor.state() == QProcess::NotRunning;
                    },
                    45000) ||
                !watcher.observation().snapshot ||
                watcher.observation().snapshot->result.state !=
                    SingleInstanceAutomationState::ServerReady)
                return fail(QStringLiteral("Editor did not become ready: %1")
                                .arg(watcher.observation().error));
            editorInstanceId = watcher.observation().snapshot->result.editorInstanceId;
            editorEndpoint = watcher.observation().snapshot->result.serverEndpoint;
            connector.setProcessEnvironment(environment);
            connector.setWorkingDirectory(QFileInfo(connectorPath).absolutePath());
            connector.start(connectorPath,
                            {QStringLiteral("--control-level"), QStringLiteral("l3")});
            if (!connector.waitForStarted(10000))
                return fail(connector.errorString());
            const auto discover = exchange(
                connector, makeRequest(1, QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod)),
                10000, error);
            if (!discover || discover->contains(QStringLiteral("error")))
                return fail(QStringLiteral("Connector discovery failed: %1").arg(error));
            bool connected = false;
            for (qint64 attempt = 0; attempt < 100 && !connected; ++attempt) {
                const auto response =
                    exchange(connector,
                             makeToolRequest(10 + attempt, QStringLiteral("connector.get_status")),
                             5000, error);
                if (!response)
                    return fail(error);
                const auto status = structuredContent(*response).toObject();
                connected =
                    status.value(QStringLiteral("mcp"))
                        .toObject()
                        .value(QStringLiteral("connected"))
                        .toBool() &&
                    status.value(QStringLiteral("toolset"))
                            .toObject()
                            .value(QStringLiteral("compatibility")) == QStringLiteral("compatible");
                if (!connected)
                    QTest::qWait(100);
            }
            if (!connected)
                return fail(QStringLiteral("Connector did not finish its editor handshake"));
            return refreshStatus();
        }

        bool refreshStatus() {
            const auto status = connectorToolContent(
                connector, 200, QStringLiteral("application.get_status"), {}, 10000, error);
            if (!status ||
                status->value(QStringLiteral("editor_instance_id")).toString() !=
                    editorInstanceId ||
                status->value(QStringLiteral("documents")).toArray().isEmpty())
                return fail(QStringLiteral("Editor status did not preserve bootstrap identity: %1")
                                .arg(error));
            lastEditorStatus = *status;
            return true;
        }

        std::optional<qint64> seedTrack(const bool audio) {
            if (!refreshStatus())
                return std::nullopt;
            const auto document =
                lastEditorStatus.value(QStringLiteral("documents")).toArray().first().toObject();
            const auto inserted = connectorToolContent(
                connector, 201, QStringLiteral("tracks.insert"),
                {
                    {QStringLiteral("document_id"),       document.value(QStringLiteral("document_id"))},
                    {QStringLiteral("expected_revision"),
                     document.value(QStringLiteral("revision"))                                        },
                    {QStringLiteral("index"),             0                                            },
                    {QStringLiteral("tracks"),
                     QJsonArray{QJsonObject{
                         {QStringLiteral("client_ref"), QStringLiteral("fixture-track")},
                         {QStringLiteral("name"), QStringLiteral("Fixture Track")}}}                   }
            },
                10000, error);
            if (!inserted) {
                fail(QStringLiteral("Could not seed a track: %1").arg(error));
                return std::nullopt;
            }
            qint64 trackId = -1;
            for (const auto &value : inserted->value(QStringLiteral("created_objects")).toArray()) {
                const auto created = value.toObject();
                const auto object = created.value(QStringLiteral("object")).toObject();
                if (created.value(QStringLiteral("client_ref")) ==
                        QStringLiteral("fixture-track") &&
                    object.value(QStringLiteral("kind")) == QStringLiteral("track"))
                    trackId = object.value(QStringLiteral("id")).toInteger(-1);
            }
            if (trackId < 0) {
                fail(QStringLiteral("Track insertion omitted the fixture client reference: %1")
                         .arg(compactJson(*inserted)));
                return std::nullopt;
            }
            if (!refreshStatus())
                return std::nullopt;
            if (!audio)
                return trackId;
            const auto current =
                lastEditorStatus.value(QStringLiteral("documents")).toArray().first().toObject();
            const auto imported = connectorToolContent(
                connector, 202, QStringLiteral("audio_clips.import"),
                {
                    {QStringLiteral("document_id"),       current.value(QStringLiteral("document_id"))},
                    {QStringLiteral("expected_revision"),
                     current.value(QStringLiteral("revision"))                                        },
                    {QStringLiteral("track_id"),          trackId                                     },
                    {QStringLiteral("path"),              audioPath                                   },
                    {QStringLiteral("start"),             0                                           }
            },
                10000, error);
            if (!imported) {
                fail(QStringLiteral("Could not import audio fixture: %1").arg(error));
                return std::nullopt;
            }
            const auto taskId = imported->value(QStringLiteral("task_id")).toString();
            for (qint64 attempt = 0; attempt < 100; ++attempt) {
                const auto task =
                    connectorToolContent(connector, 300 + attempt, QStringLiteral("tasks.get"),
                                         {
                                             {QStringLiteral("scope"),       QStringLiteral("document")},
                                             {QStringLiteral("document_id"),
                                              current.value(QStringLiteral("document_id"))             },
                                             {QStringLiteral("task_id"),     taskId                    }
                },
                                         5000, error);
                if (!task)
                    break;
                const auto state = task->value(QStringLiteral("state")).toString();
                if (state == QStringLiteral("succeeded"))
                    return trackId;
                if (state == QStringLiteral("failed") || state == QStringLiteral("canceled"))
                    break;
                QTest::qWait(50);
            }
            fail(QStringLiteral("Audio fixture task did not succeed: %1").arg(error));
            return std::nullopt;
        }

        bool shutdown() {
            const auto result =
                connectorToolContent(connector, 39999, QStringLiteral("application.request_exit"),
                                     {
                                         {QStringLiteral("discard_changes"), true}
            },
                                     10000, error);
            return (result && editor.waitForFinished(15000) &&
                    editor.exitStatus() == QProcess::NormalExit && editor.exitCode() == 0) ||
                   fail(QStringLiteral("Editor fixture did not exit cleanly: %1").arg(error));
        }
    };

    bool runEditingWorkflow(const QString &editorPath, const QString &connectorPath) {
        EditorConnectorFixture fixture(editorPath, connectorPath,
                                       QStringLiteral("mcp-editing-workflow"));
        if (!fixture.start())
            return false;
        auto &isolatedRoot = fixture.storage;
        auto &editor = fixture.editor;
        auto &connector = fixture.connector;
        auto &watcher = fixture.watcher;
        const auto &environment = fixture.environment;
        const auto &appDataRoot = fixture.appDataRoot;
        const auto &editorInstanceId = fixture.editorInstanceId;
        const auto &editorEndpoint = fixture.editorEndpoint;
        auto &exchangeError = fixture.error;
        const auto &lastEditorStatus = fixture.lastEditorStatus;
        const auto &audioPath = fixture.audioPath;
        const auto failWithProcessDiagnostics = [&](const QString &message) {
            return fail(
                QStringLiteral("%1; appdata=%2; endpoint=%3; connector_state=%4; editor_state=%5; "
                               "connector_stderr=%6; editor_stderr=%7")
                    .arg(message, appDataRoot, editorEndpoint)
                    .arg(static_cast<int>(connector.state()))
                    .arg(static_cast<int>(editor.state()))
                    .arg(QString::fromUtf8(TestSupport::readProcessStderr(connector)),
                         QString::fromUtf8(TestSupport::readProcessStderr(editor))));
        };

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
                    .arg(exchangeError, compactJson(configuredBefore), fixture.configPath));
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

        const auto directCatalog = directToolCatalog(directClient, 10000, toolError);
        QSet<QString> declaredTools;
        for (const auto &contract : AutomationWire::publicToolContracts())
            declaredTools.insert(contract.operationId);
        if (!directCatalog || *directCatalog != declaredTools) {
            return failWithProcessDiagnostics(
                QStringLiteral("Direct GUI Editor MCP exposed an invalid catalog: count=%1; %2")
                    .arg(directCatalog ? directCatalog->size() : -1)
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

        const auto windows = lastEditorStatus.value(QStringLiteral("windows")).toArray();
        const auto activeWindow = windows.size() == 1 ? windows.first().toObject() : QJsonObject{};
        const auto windowId = activeWindow.value(QStringLiteral("window_id")).toString();
        if (lastEditorStatus.value(QStringLiteral("host_mode")).toString() !=
                QStringLiteral("gui") ||
            windows.size() != 1 || QUuid::fromString(windowId).isNull() ||
            activeWindow.value(QStringLiteral("document_id")).toString() != documentId) {
            return failWithProcessDiagnostics(
                QStringLiteral("GUI application status did not expose exactly one real window: %1")
                    .arg(compactJson(lastEditorStatus)));
        }

        const QJsonObject guiWindowArguments{
            {QStringLiteral("window_id"), windowId}
        };
        const QJsonObject guiDocumentArguments{
            {QStringLiteral("window_id"),   windowId  },
            {QStringLiteral("document_id"), documentId},
        };
        const auto verifyGuiQuery = [&](const qint64 requestId, const QString &name,
                                        const QJsonObject &arguments) {
            const auto throughConnector =
                connectorToolContent(connector, requestId, name, arguments, 10000, toolError);
            if (!throughConnector)
                return false;
            const auto direct = directToolContent(directClient, name, arguments, 10000, toolError);
            return direct && equivalentContent(name, *throughConnector, *direct, toolError);
        };
        if (!verifyGuiQuery(900, QStringLiteral("workspace.get_state"), guiWindowArguments) ||
            !verifyGuiQuery(901, QStringLiteral("track_panel.get_state"), guiDocumentArguments) ||
            !verifyGuiQuery(902, QStringLiteral("clip_editor.get_state"), guiDocumentArguments)) {
            return failWithProcessDiagnostics(
                QStringLiteral("Representative GUI-only operation failed: %1").arg(toolError));
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
        const auto insertedRevision = current.value(QStringLiteral("revision")).toInteger(-1);
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
            insertedRevision != initialRevision + 1 ||
            !mutation->value(QStringLiteral("changed")).toBool() ||
            mutation->value(QStringLiteral("validated_only")).toBool() || createdTrackId < 0) {
            return failWithProcessDiagnostics(
                QStringLiteral("tracks.insert returned an invalid mutation/revision result: %1")
                    .arg(compactJson(*mutation)));
        }

        const auto undo =
            connectorToolContent(connector, 1006, QStringLiteral("history.undo"),
                                 QJsonObject{
                                     {QStringLiteral("document_id"),       documentId      },
                                     {QStringLiteral("expected_revision"), insertedRevision},
        },
                                 10000, toolError);
        const auto undoneRevision = undo ? undo->value(QStringLiteral("current"))
                                               .toObject()
                                               .value(QStringLiteral("revision"))
                                               .toInteger(-1)
                                         : -1;
        const auto documentAfterUndo = connectorToolContent(
            connector, 1007, QStringLiteral("documents.get"), documentArguments, 10000, toolError);
        const auto trackCountAfterUndo = documentAfterUndo
                                             ? documentAfterUndo->value(QStringLiteral("snapshot"))
                                                   .toObject()
                                                   .value(QStringLiteral("statistics"))
                                                   .toObject()
                                                   .value(QStringLiteral("track_count"))
                                                   .toInteger(-1)
                                             : -1;
        if (!undo || !undo->value(QStringLiteral("changed")).toBool() ||
            undoneRevision != insertedRevision + 1 || !documentAfterUndo ||
            trackCountAfterUndo !=
                initialStatistics.value(QStringLiteral("track_count")).toInteger(-1)) {
            return failWithProcessDiagnostics(
                QStringLiteral("history.undo did not restore the pre-insert document: %1; %2")
                    .arg(undo ? compactJson(*undo) : toolError,
                         documentAfterUndo ? compactJson(*documentAfterUndo) : toolError));
        }

        const auto redo =
            connectorToolContent(connector, 1008, QStringLiteral("history.redo"),
                                 QJsonObject{
                                     {QStringLiteral("document_id"),       documentId    },
                                     {QStringLiteral("expected_revision"), undoneRevision},
        },
                                 10000, toolError);
        const auto redoneRevision = redo ? redo->value(QStringLiteral("current"))
                                               .toObject()
                                               .value(QStringLiteral("revision"))
                                               .toInteger(-1)
                                         : -1;
        const auto documentAfterRedo = connectorToolContent(
            connector, 1009, QStringLiteral("documents.get"), documentArguments, 10000, toolError);
        const auto trackCountAfterRedo = documentAfterRedo
                                             ? documentAfterRedo->value(QStringLiteral("snapshot"))
                                                   .toObject()
                                                   .value(QStringLiteral("statistics"))
                                                   .toObject()
                                                   .value(QStringLiteral("track_count"))
                                                   .toInteger(-1)
                                             : -1;
        if (!redo || !redo->value(QStringLiteral("changed")).toBool() ||
            redoneRevision != undoneRevision + 1 || !documentAfterRedo ||
            trackCountAfterRedo !=
                initialStatistics.value(QStringLiteral("track_count")).toInteger(-1) + 1) {
            return failWithProcessDiagnostics(
                QStringLiteral("history.redo did not restore the inserted track: %1; %2")
                    .arg(redo ? compactJson(*redo) : toolError,
                         documentAfterRedo ? compactJson(*documentAfterRedo) : toolError));
        }
        const auto currentRevision = redoneRevision;

        const QJsonObject audioImportArguments{
            {QStringLiteral("document_id"),       documentId                        },
            {QStringLiteral("expected_revision"), currentRevision                   },
            {QStringLiteral("track_id"),          createdTrackId                    },
            {QStringLiteral("start"),             0                                 },
            {QStringLiteral("path"),              audioPath                         },
            {QStringLiteral("idempotency_key"),   QStringLiteral("audio-import-key")},
        };
        const auto audioImport =
            connectorToolContent(connector, 1003, QStringLiteral("audio_clips.import"),
                                 audioImportArguments, 10000, toolError);
        const auto audioImportTaskId =
            audioImport ? audioImport->value(QStringLiteral("task_id")).toString() : QString{};
        if (audioImportTaskId.isEmpty()) {
            return failWithProcessDiagnostics(
                QStringLiteral("audio_clips.import did not start: %1").arg(toolError));
        }
        const auto replayedAudioImport =
            connectorToolContent(connector, 1004, QStringLiteral("audio_clips.import"),
                                 audioImportArguments, 10000, toolError);
        auto conflictingAudioImportArguments = audioImportArguments;
        conflictingAudioImportArguments.insert(QStringLiteral("start"), 1);
        const auto conflictingAudioImport =
            connectorToolContent(connector, 1005, QStringLiteral("audio_clips.import"),
                                 conflictingAudioImportArguments, 10000, toolError);
        if (!replayedAudioImport ||
            replayedAudioImport->value(QStringLiteral("task_id")).toString() != audioImportTaskId ||
            conflictingAudioImport ||
            !toolError.contains(QStringLiteral("\"code\":\"idempotency_conflict\""))) {
            return failWithProcessDiagnostics(
                QStringLiteral("Audio import idempotency did not replay or reject conflicts: %1")
                    .arg(toolError));
        }
        QJsonObject audioImportTask;
        for (qint64 attempt = 0; attempt < 100; ++attempt) {
            const auto snapshot =
                connectorToolContent(connector, 1100 + attempt, QStringLiteral("tasks.get"),
                                     QJsonObject{
                                         {QStringLiteral("scope"),       QStringLiteral("document")},
                                         {QStringLiteral("document_id"), documentId                },
                                         {QStringLiteral("task_id"),     audioImportTaskId         },
            },
                                     5000, toolError);
            if (!snapshot) {
                return failWithProcessDiagnostics(
                    QStringLiteral("Audio import task lookup failed: %1").arg(toolError));
            }
            audioImportTask = *snapshot;
            const auto state = audioImportTask.value(QStringLiteral("state")).toString();
            if (state == QStringLiteral("succeeded") || state == QStringLiteral("failed") ||
                state == QStringLiteral("canceled")) {
                break;
            }
            QThread::msleep(50);
        }
        if (audioImportTask.value(QStringLiteral("state")).toString() !=
            QStringLiteral("succeeded")) {
            return failWithProcessDiagnostics(
                QStringLiteral("Audio import task did not succeed: %1")
                    .arg(compactJson(audioImportTask)));
        }
        qint64 createdAudioClipId = -1;
        const auto audioMutation = audioImportTask.value(QStringLiteral("result")).toObject();
        const auto audioCurrentRevision = audioMutation.value(QStringLiteral("current"))
                                              .toObject()
                                              .value(QStringLiteral("revision"))
                                              .toInteger(-1);
        for (const auto &affectedValue :
             audioMutation.value(QStringLiteral("affected_objects")).toArray()) {
            const auto object = affectedValue.toObject();
            if (object.value(QStringLiteral("kind")).toString() == QStringLiteral("clip")) {
                createdAudioClipId = object.value(QStringLiteral("id")).toInteger(-1);
                break;
            }
        }
        const auto importedAudio =
            connectorToolContent(connector, 1200, QStringLiteral("audio_clips.get"),
                                 QJsonObject{
                                     {QStringLiteral("document_id"), documentId        },
                                     {QStringLiteral("clip_id"),     createdAudioClipId},
        },
                                 5000, toolError);
        if (audioCurrentRevision != currentRevision + 1 || createdAudioClipId < 0 ||
            !importedAudio ||
            !importedAudio->value(QStringLiteral("snapshot"))
                 .toObject()
                 .value(QStringLiteral("hash_exists"))
                 .toBool()) {
            return failWithProcessDiagnostics(
                QStringLiteral("Imported audio clip did not retain a SHA-512 digest: %1")
                    .arg(importedAudio ? compactJson(*importedAudio) : toolError));
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
                    .toInteger(-1) != audioCurrentRevision ||
            statistics.value(QStringLiteral("track_count")).toInteger(-1) !=
                initialStatistics.value(QStringLiteral("track_count")).toInteger(-1) + 1 ||
            statistics.value(QStringLiteral("empty_track_count")).toInteger(-1) !=
                initialStatistics.value(QStringLiteral("empty_track_count")).toInteger(-1) ||
            statistics.value(QStringLiteral("clip_count")).toInteger(-1) !=
                initialStatistics.value(QStringLiteral("clip_count")).toInteger(-1) + 1) {
            return failWithProcessDiagnostics(
                QStringLiteral(
                    "documents.get did not expose the inserted track and audio clip statistics at "
                    "the new revision: %1")
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

        const auto playbackBefore =
            connectorToolContent(connector, 1300, QStringLiteral("playback.get_state"),
                                 documentArguments, 10000, toolError);
        if (!playbackBefore || !playbackBefore->value(QStringLiteral("snapshot")).isObject()) {
            return failWithProcessDiagnostics(
                QStringLiteral("Initial playback.get_state failed: %1").arg(toolError));
        }
        const auto playResponse = exchange(
            connector, makeToolRequest(1301, QStringLiteral("playback.play"), documentArguments),
            10000, exchangeError);
        if (!playResponse || playResponse->contains(QStringLiteral("error")) ||
            !playResponse->value(QStringLiteral("result")).isObject()) {
            return failWithProcessDiagnostics(
                QStringLiteral("playback.play returned an invalid MCP response: %1")
                    .arg(playResponse ? compactJson(*playResponse) : exchangeError));
        }
        const auto playResult = playResponse->value(QStringLiteral("result")).toObject();
        const auto playContent = playResult.value(QStringLiteral("structuredContent")).toObject();
        if (playResult.value(QStringLiteral("isError")).toBool()) {
            if (playContent.value(QStringLiteral("code")).toString() !=
                    QStringLiteral("host_capability_unavailable") ||
                playContent.value(QStringLiteral("operation_id")).toString() !=
                    QStringLiteral("playback.play") ||
                playContent.value(QStringLiteral("message")).toString() !=
                    QStringLiteral("Playback device could not be started")) {
                return failWithProcessDiagnostics(
                    QStringLiteral("playback.play did not return the expected unavailable-device "
                                   "error: %1")
                        .arg(compactJson(*playResponse)));
            }
        } else {
            if (playContent.value(QStringLiteral("playback"))
                    .toObject()
                    .value(QStringLiteral("state"))
                    .toString() != QStringLiteral("playing")) {
                return failWithProcessDiagnostics(
                    QStringLiteral("playback.play did not enter playing state: %1")
                        .arg(compactJson(*playResponse)));
            }
            const auto playbackDuring =
                connectorToolContent(connector, 1302, QStringLiteral("playback.get_state"),
                                     documentArguments, 10000, toolError);
            if (!playbackDuring || !playbackDuring->value(QStringLiteral("snapshot")).isObject()) {
                return failWithProcessDiagnostics(
                    QStringLiteral("Playback state query after play failed: %1").arg(toolError));
            }
            const auto stopped =
                connectorToolContent(connector, 1303, QStringLiteral("playback.stop"),
                                     documentArguments, 10000, toolError);
            const auto playbackAfter =
                connectorToolContent(connector, 1304, QStringLiteral("playback.get_state"),
                                     documentArguments, 10000, toolError);
            if (!stopped ||
                stopped->value(QStringLiteral("playback"))
                        .toObject()
                        .value(QStringLiteral("state"))
                        .toString() != QStringLiteral("stopped") ||
                !playbackAfter ||
                playbackAfter->value(QStringLiteral("snapshot"))
                        .toObject()
                        .value(QStringLiteral("state"))
                        .toString() != QStringLiteral("stopped")) {
                return failWithProcessDiagnostics(
                    QStringLiteral("playback play/get_state/stop loop failed: %1").arg(toolError));
            }
        }

        return fixture.shutdown();
    }

    bool runSecondaryStartup(const QString &editorPath, const QString &connectorPath) {
        EditorConnectorFixture fixture(editorPath, connectorPath,
                                       QStringLiteral("mcp-secondary-startup"));
        if (!fixture.start())
            return false;
        auto &isolatedRoot = fixture.storage;
        auto &editor = fixture.editor;
        auto &connector = fixture.connector;
        auto &watcher = fixture.watcher;
        const auto &environment = fixture.environment;
        const auto &appDataRoot = fixture.appDataRoot;
        const auto &editorInstanceId = fixture.editorInstanceId;
        const auto &editorEndpoint = fixture.editorEndpoint;
        auto &exchangeError = fixture.error;
        const auto &lastEditorStatus = fixture.lastEditorStatus;
        const auto &audioPath = fixture.audioPath;
        const auto failWithProcessDiagnostics = [&](const QString &message) {
            return fail(
                QStringLiteral("%1; appdata=%2; endpoint=%3; connector_state=%4; editor_state=%5; "
                               "connector_stderr=%6; editor_stderr=%7")
                    .arg(message, appDataRoot, editorEndpoint)
                    .arg(static_cast<int>(connector.state()))
                    .arg(static_cast<int>(editor.state()))
                    .arg(QString::fromUtf8(TestSupport::readProcessStderr(connector)),
                         QString::fromUtf8(TestSupport::readProcessStderr(editor))));
        };

        auto &secondaryEditor = isolatedRoot.process(QStringLiteral("gui-secondary"));
        auto &headlessSecondaryEditor = isolatedRoot.process(QStringLiteral("headless-secondary"));
        secondaryEditor.setProcessEnvironment(environment);
        secondaryEditor.setWorkingDirectory(QFileInfo(editorPath).absolutePath());
        secondaryEditor.setProcessChannelMode(QProcess::SeparateChannels);
        secondaryEditor.start(editorPath, {QStringLiteral("--no-mcp")});
        if (!secondaryEditor.waitForStarted(10000) || !secondaryEditor.waitForFinished(10000)) {
            return fail(QStringLiteral("Secondary editor with automation overrides did not exit"));
        }
        const auto secondaryError =
            QString::fromUtf8(TestSupport::readProcessStderr(secondaryEditor));
        if (secondaryEditor.exitStatus() != QProcess::NormalExit ||
            secondaryEditor.exitCode() == 0 ||
            !secondaryError.contains(
                QStringLiteral("Automation command-line options cannot be applied"))) {
            return fail(QStringLiteral("Secondary editor did not reject automation overrides; "
                                       "exit_status=%1; exit_code=%2; stderr=%3")
                            .arg(static_cast<int>(secondaryEditor.exitStatus()))
                            .arg(secondaryEditor.exitCode())
                            .arg(secondaryError));
        }

        headlessSecondaryEditor.setProcessEnvironment(environment);
        headlessSecondaryEditor.setWorkingDirectory(QFileInfo(editorPath).absolutePath());
        headlessSecondaryEditor.setProcessChannelMode(QProcess::SeparateChannels);
        headlessSecondaryEditor.start(editorPath, {QStringLiteral("--headless")});
        if (!headlessSecondaryEditor.waitForStarted(10000) ||
            !headlessSecondaryEditor.waitForFinished(10000)) {
            return fail(QStringLiteral("Headless secondary did not forward activation and exit"));
        }
        const auto headlessSecondaryError =
            QString::fromUtf8(TestSupport::readProcessStderr(headlessSecondaryEditor));
        const auto &primaryAfterHeadless = watcher.observation().snapshot;
        if (headlessSecondaryEditor.exitStatus() != QProcess::NormalExit ||
            headlessSecondaryEditor.exitCode() != 0 ||
            !headlessSecondaryError.contains(
                QStringLiteral("no new Headless instance was started")) ||
            !primaryAfterHeadless || primaryAfterHeadless->primaryProcessId != editor.processId() ||
            primaryAfterHeadless->result.editorInstanceId != editorInstanceId ||
            primaryAfterHeadless->result.hostMode != QStringLiteral("gui") ||
            primaryAfterHeadless->result.serverEndpoint != editorEndpoint ||
            editor.state() != QProcess::Running) {
            return fail(QStringLiteral("Headless secondary replaced or disturbed the GUI Primary; "
                                       "secondary_exit_status=%1; secondary_exit_code=%2; "
                                       "secondary_stderr=%3")
                            .arg(static_cast<int>(headlessSecondaryEditor.exitStatus()))
                            .arg(headlessSecondaryEditor.exitCode())
                            .arg(headlessSecondaryError));
        }

        return fixture.shutdown();
    }

    bool runLegacyConnector(const QString &editorPath, const QString &connectorPath) {
        EditorConnectorFixture fixture(editorPath, connectorPath,
                                       QStringLiteral("mcp-legacy-connector"));
        if (!fixture.start())
            return false;
        auto &isolatedRoot = fixture.storage;
        auto &editor = fixture.editor;
        auto &connector = fixture.connector;
        auto &watcher = fixture.watcher;
        const auto &environment = fixture.environment;
        const auto &appDataRoot = fixture.appDataRoot;
        const auto &editorInstanceId = fixture.editorInstanceId;
        const auto &editorEndpoint = fixture.editorEndpoint;
        auto &exchangeError = fixture.error;
        const auto &lastEditorStatus = fixture.lastEditorStatus;
        const auto &audioPath = fixture.audioPath;
        const auto failWithProcessDiagnostics = [&](const QString &message) {
            return fail(
                QStringLiteral("%1; appdata=%2; endpoint=%3; connector_state=%4; editor_state=%5; "
                               "connector_stderr=%6; editor_stderr=%7")
                    .arg(message, appDataRoot, editorEndpoint)
                    .arg(static_cast<int>(connector.state()))
                    .arg(static_cast<int>(editor.state()))
                    .arg(QString::fromUtf8(TestSupport::readProcessStderr(connector)),
                         QString::fromUtf8(TestSupport::readProcessStderr(editor))));
        };

        QSet<QString> toolNames;
        const auto modernTools = exchange(
            connector, makeRequest(100, QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod)),
            10000, exchangeError);
        if (!modernTools || modernTools->contains(QStringLiteral("error")))
            return failWithProcessDiagnostics(QStringLiteral("Modern baseline tools/list failed"));
        for (const auto &tool : modernTools->value(QStringLiteral("result"))
                                    .toObject()
                                    .value(QStringLiteral("tools"))
                                    .toArray())
            toolNames.insert(tool.toObject().value(QStringLiteral("name")).toString());
        auto &legacyConnector = isolatedRoot.process(QStringLiteral("legacyConnector"));
        const auto legacyConnectorCleanup = qScopeGuard([&legacyConnector] {
            legacyConnector.closeWriteChannel();
            if (!legacyConnector.waitForFinished(3000))
                stopProcess(legacyConnector);
        });
        legacyConnector.setProcessEnvironment(environment);
        legacyConnector.setWorkingDirectory(QFileInfo(connectorPath).absolutePath());
        legacyConnector.setProcessChannelMode(QProcess::SeparateChannels);
        legacyConnector.start(connectorPath,
                              {QStringLiteral("--control-level"), QStringLiteral("l3")});
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

        return fixture.shutdown();
    }

    bool runLegacyEditor(const QString &editorPath, const QString &connectorPath) {
        EditorConnectorFixture fixture(editorPath, connectorPath,
                                       QStringLiteral("mcp-legacy-editor"));
        if (!fixture.start())
            return false;
        auto &isolatedRoot = fixture.storage;
        auto &editor = fixture.editor;
        auto &connector = fixture.connector;
        auto &watcher = fixture.watcher;
        const auto &environment = fixture.environment;
        const auto &appDataRoot = fixture.appDataRoot;
        const auto &editorInstanceId = fixture.editorInstanceId;
        const auto &editorEndpoint = fixture.editorEndpoint;
        auto &exchangeError = fixture.error;
        const auto &lastEditorStatus = fixture.lastEditorStatus;
        const auto &audioPath = fixture.audioPath;
        const auto failWithProcessDiagnostics = [&](const QString &message) {
            return fail(
                QStringLiteral("%1; appdata=%2; endpoint=%3; connector_state=%4; editor_state=%5; "
                               "connector_stderr=%6; editor_stderr=%7")
                    .arg(message, appDataRoot, editorEndpoint)
                    .arg(static_cast<int>(connector.state()))
                    .arg(static_cast<int>(editor.state()))
                    .arg(QString::fromUtf8(TestSupport::readProcessStderr(connector)),
                         QString::fromUtf8(TestSupport::readProcessStderr(editor))));
        };

        const auto legacyContext = legacyRequestContext();
        QString directEndpointError;
        QString toolError;
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

        return fixture.shutdown();
    }

    bool runDocumentLifecycle(const QString &editorPath, const QString &connectorPath) {
        EditorConnectorFixture fixture(editorPath, connectorPath,
                                       QStringLiteral("mcp-document-lifecycle"));
        if (!fixture.start())
            return false;
        auto &isolatedRoot = fixture.storage;
        auto &editor = fixture.editor;
        auto &connector = fixture.connector;
        auto &watcher = fixture.watcher;
        const auto &environment = fixture.environment;
        const auto &appDataRoot = fixture.appDataRoot;
        const auto &editorInstanceId = fixture.editorInstanceId;
        const auto &editorEndpoint = fixture.editorEndpoint;
        auto &exchangeError = fixture.error;
        const auto &lastEditorStatus = fixture.lastEditorStatus;
        const auto &audioPath = fixture.audioPath;
        const auto failWithProcessDiagnostics = [&](const QString &message) {
            return fail(
                QStringLiteral("%1; appdata=%2; endpoint=%3; connector_state=%4; editor_state=%5; "
                               "connector_stderr=%6; editor_stderr=%7")
                    .arg(message, appDataRoot, editorEndpoint)
                    .arg(static_cast<int>(connector.state()))
                    .arg(static_cast<int>(editor.state()))
                    .arg(QString::fromUtf8(TestSupport::readProcessStderr(connector)),
                         QString::fromUtf8(TestSupport::readProcessStderr(editor))));
        };

        const auto audioTrackId = fixture.seedTrack(true);
        if (!audioTrackId ||
            !verifyDocumentLifecycle(connector, isolatedRoot.path(), *audioTrackId))
            return failWithProcessDiagnostics(QStringLiteral("Document lifecycle checks failed"));
        return fixture.shutdown();
    }

    bool runGracefulExit(const QString &editorPath, const QString &connectorPath) {
        EditorConnectorFixture fixture(editorPath, connectorPath,
                                       QStringLiteral("mcp-graceful-exit"));
        if (!fixture.start())
            return false;
        auto &isolatedRoot = fixture.storage;
        auto &editor = fixture.editor;
        auto &connector = fixture.connector;
        auto &watcher = fixture.watcher;
        const auto &environment = fixture.environment;
        const auto &appDataRoot = fixture.appDataRoot;
        const auto &editorInstanceId = fixture.editorInstanceId;
        const auto &editorEndpoint = fixture.editorEndpoint;
        auto &exchangeError = fixture.error;
        const auto &lastEditorStatus = fixture.lastEditorStatus;
        const auto &audioPath = fixture.audioPath;
        const auto failWithProcessDiagnostics = [&](const QString &message) {
            return fail(
                QStringLiteral("%1; appdata=%2; endpoint=%3; connector_state=%4; editor_state=%5; "
                               "connector_stderr=%6; editor_stderr=%7")
                    .arg(message, appDataRoot, editorEndpoint)
                    .arg(static_cast<int>(connector.state()))
                    .arg(static_cast<int>(editor.state()))
                    .arg(QString::fromUtf8(TestSupport::readProcessStderr(connector)),
                         QString::fromUtf8(TestSupport::readProcessStderr(editor))));
        };

        if (!fixture.seedTrack(false))
            return failWithProcessDiagnostics(QStringLiteral("Could not make the document dirty"));
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

        const auto acceptedExitRequest =
            makeToolRequest(30001, QStringLiteral("application.request_exit"),
                            QJsonObject{
                                {QStringLiteral("discard_changes"), true}
        });
        if (!writeMessage(connector, acceptedExitRequest, exchangeError))
            return failWithProcessDiagnostics(exchangeError);
        connector.closeWriteChannel();
        const auto acceptedExitResponse = readResponse(
            connector, acceptedExitRequest.value(QStringLiteral("id")), 10000, exchangeError);
        const auto acceptedExitResult =
            acceptedExitResponse ? acceptedExitResponse->value(QStringLiteral("result")).toObject()
                                 : QJsonObject{};
        const auto acceptedExit =
            acceptedExitResult.value(QStringLiteral("structuredContent")).toObject();
        if (!acceptedExitResponse || acceptedExitResponse->contains(QStringLiteral("error")) ||
            acceptedExitResult.value(QStringLiteral("isError")).toBool() ||
            !acceptedExit.value(QStringLiteral("accepted")).toBool() ||
            acceptedExit.value(QStringLiteral("action")).toString() != QStringLiteral("exit") ||
            !acceptedExit.value(QStringLiteral("discard_changes")).toBool() ||
            !connector.waitForFinished(5000) || !editor.waitForFinished(15000)) {
            return failWithProcessDiagnostics(
                QStringLiteral("Forced graceful editor exit did not complete: %1")
                    .arg(acceptedExitResponse ? compactJson(*acceptedExitResponse)
                                              : exchangeError));
        }

        return true;
    }

}

class TestMcpProcessIntegration final : public QObject {
    Q_OBJECT
public:
    QString editorPath;
    QString connectorPath;
private slots:

    void editingWorkflow() {
        QVERIFY(runEditingWorkflow(editorPath, connectorPath));
    }

    void secondaryStartup() {
        QVERIFY(runSecondaryStartup(editorPath, connectorPath));
    }

    void legacyConnector() {
        QVERIFY(runLegacyConnector(editorPath, connectorPath));
    }

    void legacyEditor() {
        QVERIFY(runLegacyEditor(editorPath, connectorPath));
    }

    void documentLifecycle() {
        QVERIFY(runDocumentLifecycle(editorPath, connectorPath));
    }

    void gracefulExit() {
        QVERIFY(runGracefulExit(editorPath, connectorPath));
    }
};

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    TestMcpProcessIntegration test;
    auto arguments = application.arguments();
    if (arguments.size() < 3)
        return 2;
    test.editorPath = arguments.takeAt(1);
    test.connectorPath = arguments.takeAt(1);
    return QTest::qExec(&test, arguments);
}

#include "main.moc"
