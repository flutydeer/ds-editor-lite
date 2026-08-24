#include "SingleInstanceProtocol.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
    QString commandName(const SingleInstanceCommand command) {
        switch (command) {
            case SingleInstanceCommand::Activate:
                return QStringLiteral("activate");
            case SingleInstanceCommand::OpenProjects:
                return QStringLiteral("openProjects");
            case SingleInstanceCommand::AutomationDiscover:
                return QStringLiteral("automation.discover");
            case SingleInstanceCommand::AutomationWatch:
                return QStringLiteral("automation.watch");
        }
        return {};
    }

    bool parseEnvelope(const QByteArray &payload, QJsonObject &object, QString &error,
                       QString *requestId = nullptr) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            error = QStringLiteral("Invalid JSON payload");
            return false;
        }
        object = document.object();
        if (requestId)
            *requestId = object.value(QStringLiteral("requestId")).toString();
        if (object.value(QStringLiteral("protocolVersion")).toInt(-1) !=
            SingleInstanceProtocol::protocolVersion) {
            error = QStringLiteral("Unsupported single-instance protocol version");
            return false;
        }
        return true;
    }

    bool requireString(const QJsonObject &object, const QString &name, QString &value,
                       QString &error, const bool allowEmpty = false) {
        const auto candidate = object.value(name);
        if (!candidate.isString() || (!allowEmpty && candidate.toString().isEmpty())) {
            error = QStringLiteral("Invalid automation state field: %1").arg(name);
            return false;
        }
        value = candidate.toString();
        return true;
    }
}

QByteArray SingleInstanceProtocol::encodeRequest(const SingleInstanceRequest &request) {
    QJsonObject object{
        {QStringLiteral("protocolVersion"), protocolVersion             },
        {QStringLiteral("requestId"),       request.requestId           },
        {QStringLiteral("command"),         commandName(request.command)},
    };
    if (request.command == SingleInstanceCommand::Activate ||
        request.command == SingleInstanceCommand::OpenProjects) {
        object.insert(QStringLiteral("paths"), QJsonArray::fromStringList(request.paths));
    } else if (request.command == SingleInstanceCommand::AutomationWatch) {
        object.insert(QStringLiteral("connector"),
                      QJsonObject{
                          {QStringLiteral("instanceId"), request.connector.instanceId},
                          {QStringLiteral("version"),    request.connector.version   },
        });
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool SingleInstanceProtocol::decodeRequest(const QByteArray &payload,
                                           SingleInstanceRequest &request, QString &error) {
    QJsonObject object;
    if (!parseEnvelope(payload, object, error, &request.requestId))
        return false;

    if (request.requestId.isEmpty()) {
        error = QStringLiteral("Missing request ID");
        return false;
    }

    const auto command = object.value(QStringLiteral("command")).toString();
    if (command == QStringLiteral("activate")) {
        request.command = SingleInstanceCommand::Activate;
    } else if (command == QStringLiteral("openProjects")) {
        request.command = SingleInstanceCommand::OpenProjects;
    } else if (command == QStringLiteral("automation.discover")) {
        request.command = SingleInstanceCommand::AutomationDiscover;
    } else if (command == QStringLiteral("automation.watch")) {
        request.command = SingleInstanceCommand::AutomationWatch;
    } else {
        error = QStringLiteral("Unsupported command");
        return false;
    }

    request.paths.clear();
    request.connector = {};
    if (request.command == SingleInstanceCommand::Activate ||
        request.command == SingleInstanceCommand::OpenProjects) {
        const auto paths = object.value(QStringLiteral("paths"));
        if (!paths.isArray()) {
            error = QStringLiteral("Invalid project path list");
            return false;
        }
        for (const auto &value : paths.toArray()) {
            if (!value.isString() || value.toString().isEmpty()) {
                error = QStringLiteral("Invalid project path");
                return false;
            }
            request.paths.append(value.toString());
        }
        if (request.command == SingleInstanceCommand::OpenProjects && request.paths.isEmpty()) {
            error = QStringLiteral("Open request has no project paths");
            return false;
        }
    }
    if (request.command == SingleInstanceCommand::AutomationWatch) {
        const auto connector = object.value(QStringLiteral("connector"));
        if (!connector.isObject()) {
            error = QStringLiteral("Missing connector identity");
            return false;
        }
        const auto connectorObject = connector.toObject();
        request.connector.instanceId =
            connectorObject.value(QStringLiteral("instanceId")).toString();
        request.connector.version = connectorObject.value(QStringLiteral("version")).toString();
        if (request.connector.instanceId.isEmpty() || request.connector.version.isEmpty()) {
            error = QStringLiteral("Invalid connector identity");
            return false;
        }
    }
    return true;
}

QByteArray SingleInstanceProtocol::encodeResponse(const SingleInstanceResponse &response) {
    const QJsonObject object{
        {QStringLiteral("protocolVersion"),  protocolVersion          },
        {QStringLiteral("requestId"),        response.requestId       },
        {QStringLiteral("accepted"),         response.accepted        },
        {QStringLiteral("error"),            response.error           },
        {QStringLiteral("primaryProcessId"), response.primaryProcessId},
    };
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool SingleInstanceProtocol::decodeResponse(const QByteArray &payload,
                                            SingleInstanceResponse &response, QString &error) {
    QJsonObject object;
    if (!parseEnvelope(payload, object, error))
        return false;
    response.requestId = object.value(QStringLiteral("requestId")).toString();
    if (response.requestId.isEmpty() || !object.value(QStringLiteral("accepted")).isBool()) {
        error = QStringLiteral("Invalid single-instance response");
        return false;
    }
    response.accepted = object.value(QStringLiteral("accepted")).toBool();
    response.error = object.value(QStringLiteral("error")).toString();
    response.primaryProcessId = object.value(QStringLiteral("primaryProcessId")).toInteger();
    return true;
}

QString SingleInstanceProtocol::automationStateName(const SingleInstanceAutomationState state) {
    switch (state) {
        case SingleInstanceAutomationState::Starting:
            return QStringLiteral("starting");
        case SingleInstanceAutomationState::McpDisabled:
            return QStringLiteral("mcp_disabled");
        case SingleInstanceAutomationState::McpStarting:
            return QStringLiteral("mcp_starting");
        case SingleInstanceAutomationState::McpReady:
            return QStringLiteral("mcp_ready");
        case SingleInstanceAutomationState::McpStopping:
            return QStringLiteral("mcp_stopping");
        case SingleInstanceAutomationState::EditorStopping:
            return QStringLiteral("editor_stopping");
        case SingleInstanceAutomationState::Error:
            return QStringLiteral("error");
    }
    return {};
}

bool SingleInstanceProtocol::parseAutomationState(const QString &name,
                                                  SingleInstanceAutomationState &state) {
    if (name == QStringLiteral("starting")) {
        state = SingleInstanceAutomationState::Starting;
    } else if (name == QStringLiteral("mcp_disabled")) {
        state = SingleInstanceAutomationState::McpDisabled;
    } else if (name == QStringLiteral("mcp_starting")) {
        state = SingleInstanceAutomationState::McpStarting;
    } else if (name == QStringLiteral("mcp_ready")) {
        state = SingleInstanceAutomationState::McpReady;
    } else if (name == QStringLiteral("mcp_stopping")) {
        state = SingleInstanceAutomationState::McpStopping;
    } else if (name == QStringLiteral("editor_stopping")) {
        state = SingleInstanceAutomationState::EditorStopping;
    } else if (name == QStringLiteral("error")) {
        state = SingleInstanceAutomationState::Error;
    } else {
        return false;
    }
    return true;
}

QByteArray SingleInstanceProtocol::encodeAutomationSnapshot(
    const SingleInstanceAutomationSnapshot &snapshot) {
    const auto &status = snapshot.result;
    QJsonObject object{
        {QStringLiteral("protocolVersion"),  protocolVersion                          },
        {QStringLiteral("event"),            QStringLiteral("automation.stateChanged")},
        {QStringLiteral("primaryProcessId"), snapshot.primaryProcessId                },
        {QStringLiteral("result"),
         QJsonObject{
             {QStringLiteral("state"), automationStateName(status.state)},
             {QStringLiteral("editorInstanceId"), status.editorInstanceId},
             {QStringLiteral("hostMode"), status.hostMode},
             {QStringLiteral("applicationVersion"), status.applicationVersion},
             {QStringLiteral("buildId"), status.buildId},
             {QStringLiteral("executablePath"), status.executablePath},
             {QStringLiteral("mcpEnabled"), status.mcpEnabled},
             {QStringLiteral("mcpEndpoint"), status.mcpEndpoint},
             {QStringLiteral("error"), status.error},
         }                                                                            },
    };
    if (!snapshot.requestId.isEmpty())
        object.insert(QStringLiteral("requestId"), snapshot.requestId);
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool SingleInstanceProtocol::decodeAutomationSnapshot(const QByteArray &payload,
                                                      SingleInstanceAutomationSnapshot &snapshot,
                                                      QString &error) {
    QJsonObject object;
    if (!parseEnvelope(payload, object, error))
        return false;
    if (object.value(QStringLiteral("event")).toString() !=
            QStringLiteral("automation.stateChanged") ||
        !object.value(QStringLiteral("primaryProcessId")).isDouble() ||
        !object.value(QStringLiteral("result")).isObject()) {
        error = QStringLiteral("Invalid automation state event");
        return false;
    }

    snapshot.requestId = object.value(QStringLiteral("requestId")).toString();
    snapshot.primaryProcessId = object.value(QStringLiteral("primaryProcessId")).toInteger();
    const auto result = object.value(QStringLiteral("result")).toObject();
    SingleInstanceAutomationStatus status;
    if (!parseAutomationState(result.value(QStringLiteral("state")).toString(), status.state)) {
        error = QStringLiteral("Invalid automation state");
        return false;
    }
    if (!requireString(result, QStringLiteral("editorInstanceId"), status.editorInstanceId,
                       error) ||
        !requireString(result, QStringLiteral("hostMode"), status.hostMode, error) ||
        !requireString(result, QStringLiteral("applicationVersion"), status.applicationVersion,
                       error, true) ||
        !requireString(result, QStringLiteral("buildId"), status.buildId, error, true) ||
        !requireString(result, QStringLiteral("executablePath"), status.executablePath, error) ||
        !requireString(result, QStringLiteral("mcpEndpoint"), status.mcpEndpoint, error, true) ||
        !requireString(result, QStringLiteral("error"), status.error, error, true) ||
        !result.value(QStringLiteral("mcpEnabled")).isBool()) {
        if (error.isEmpty())
            error = QStringLiteral("Invalid automation state field: mcpEnabled");
        return false;
    }
    status.mcpEnabled = result.value(QStringLiteral("mcpEnabled")).toBool();
    snapshot.result = std::move(status);
    return true;
}

QByteArray SingleInstanceProtocol::frame(const QByteArray &payload) {
    if (payload.size() > maxPayloadSize)
        return {};
    const auto size = static_cast<quint32>(payload.size());
    QByteArray result(4, Qt::Uninitialized);
    result[0] = static_cast<char>((size >> 24) & 0xff);
    result[1] = static_cast<char>((size >> 16) & 0xff);
    result[2] = static_cast<char>((size >> 8) & 0xff);
    result[3] = static_cast<char>(size & 0xff);
    result.append(payload);
    return result;
}

bool SingleInstanceProtocol::takeFrame(QByteArray &buffer, QByteArray &payload, QString &error) {
    if (buffer.size() < 4)
        return false;
    const auto size = (static_cast<quint32>(static_cast<uchar>(buffer[0])) << 24) |
                      (static_cast<quint32>(static_cast<uchar>(buffer[1])) << 16) |
                      (static_cast<quint32>(static_cast<uchar>(buffer[2])) << 8) |
                      static_cast<quint32>(static_cast<uchar>(buffer[3]));
    if (size > static_cast<quint32>(maxPayloadSize)) {
        error = QStringLiteral("Single-instance message is too large");
        return false;
    }
    if (buffer.size() < 4 + static_cast<qsizetype>(size))
        return false;
    payload = buffer.mid(4, size);
    buffer.remove(0, 4 + size);
    return true;
}
