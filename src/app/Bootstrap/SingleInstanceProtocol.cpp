#include "SingleInstanceProtocol.h"

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>

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
            case SingleInstanceCommand::ApplicationHandoff:
                return QStringLiteral("application.handoff");
            case SingleInstanceCommand::ApplicationQuery:
                return QStringLiteral("application.query");
            case SingleInstanceCommand::ApplicationCancel:
                return QStringLiteral("application.cancel");
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
        request.command == SingleInstanceCommand::OpenProjects ||
        request.command == SingleInstanceCommand::ApplicationHandoff) {
        object.insert(QStringLiteral("paths"), QJsonArray::fromStringList(request.paths));
    } else if (request.command == SingleInstanceCommand::AutomationWatch) {
        object.insert(QStringLiteral("connector"),
                      QJsonObject{
                          {QStringLiteral("instanceId"), request.connector.instanceId},
                          {QStringLiteral("version"),    request.connector.version   },
        });
    }
    if (request.command == SingleInstanceCommand::ApplicationHandoff)
        object.insert(QStringLiteral("handoffVersion"), request.handoffVersion);
    if (request.command == SingleInstanceCommand::ApplicationQuery ||
        request.command == SingleInstanceCommand::ApplicationCancel) {
        object.insert(QStringLiteral("targetRequestId"), request.targetRequestId);
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
    } else if (command == QStringLiteral("application.handoff")) {
        request.command = SingleInstanceCommand::ApplicationHandoff;
    } else if (command == QStringLiteral("application.query")) {
        request.command = SingleInstanceCommand::ApplicationQuery;
    } else if (command == QStringLiteral("application.cancel")) {
        request.command = SingleInstanceCommand::ApplicationCancel;
    } else {
        error = QStringLiteral("Unsupported command");
        return false;
    }

    request.paths.clear();
    request.connector = {};
    request.targetRequestId.clear();
    request.handoffVersion = 0;
    if (request.command == SingleInstanceCommand::Activate ||
        request.command == SingleInstanceCommand::OpenProjects ||
        request.command == SingleInstanceCommand::ApplicationHandoff) {
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
    if (request.command == SingleInstanceCommand::ApplicationHandoff) {
        const auto version = object.value(QStringLiteral("handoffVersion"));
        if (!version.isDouble()) {
            error = QStringLiteral("Missing handoff version");
            return false;
        }
        request.handoffVersion = version.toInt();
        if (request.handoffVersion != SingleInstanceProtocol::handoffVersion) {
            error = QStringLiteral("Unsupported handoff version");
            return false;
        }
    }
    if (request.command == SingleInstanceCommand::ApplicationQuery ||
        request.command == SingleInstanceCommand::ApplicationCancel) {
        const auto target = object.value(QStringLiteral("targetRequestId"));
        if (!target.isString() || target.toString().isEmpty()) {
            error = QStringLiteral("Missing handoff target request ID");
            return false;
        }
        request.targetRequestId = target.toString();
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

QString SingleInstanceProtocol::handoffStateName(const HandoffRequestState state) {
    switch (state) {
        case HandoffRequestState::Received:
            return QStringLiteral("received");
        case HandoffRequestState::Pending:
            return QStringLiteral("pending");
        case HandoffRequestState::Taken:
            return QStringLiteral("taken");
        case HandoffRequestState::Rejected:
            return QStringLiteral("rejected");
        case HandoffRequestState::Cancelled:
            return QStringLiteral("cancelled");
        case HandoffRequestState::Unknown:
            return QStringLiteral("unknown");
    }
    return {};
}

bool SingleInstanceProtocol::parseHandoffState(const QString &name,
                                               HandoffRequestState &state) {
    if (name == QStringLiteral("received")) {
        state = HandoffRequestState::Received;
    } else if (name == QStringLiteral("pending")) {
        state = HandoffRequestState::Pending;
    } else if (name == QStringLiteral("taken")) {
        state = HandoffRequestState::Taken;
    } else if (name == QStringLiteral("rejected")) {
        state = HandoffRequestState::Rejected;
    } else if (name == QStringLiteral("cancelled")) {
        state = HandoffRequestState::Cancelled;
    } else if (name == QStringLiteral("unknown")) {
        state = HandoffRequestState::Unknown;
    } else {
        return false;
    }
    return true;
}

QString SingleInstanceProtocol::startupStageName(const PrimaryStartupStage stage) {
    switch (stage) {
        case PrimaryStartupStage::Starting:
            return QStringLiteral("starting");
        case PrimaryStartupStage::Initializing:
            return QStringLiteral("initializing");
        case PrimaryStartupStage::WindowShown:
            return QStringLiteral("windowShown");
        case PrimaryStartupStage::Ready:
            return QStringLiteral("ready");
        case PrimaryStartupStage::Stopping:
            return QStringLiteral("stopping");
    }
    return {};
}

bool SingleInstanceProtocol::parseStartupStage(const QString &name,
                                               PrimaryStartupStage &stage) {
    if (name == QStringLiteral("starting")) {
        stage = PrimaryStartupStage::Starting;
    } else if (name == QStringLiteral("initializing")) {
        stage = PrimaryStartupStage::Initializing;
    } else if (name == QStringLiteral("windowShown")) {
        stage = PrimaryStartupStage::WindowShown;
    } else if (name == QStringLiteral("ready")) {
        stage = PrimaryStartupStage::Ready;
    } else if (name == QStringLiteral("stopping")) {
        stage = PrimaryStartupStage::Stopping;
    } else {
        return false;
    }
    return true;
}

HandoffCancelDisposition SingleInstanceProtocol::cancelDisposition(
    const HandoffRequestState state) {
    switch (state) {
        case HandoffRequestState::Cancelled:
            return HandoffCancelDisposition::Cancelled;
        case HandoffRequestState::Taken:
            return HandoffCancelDisposition::AlreadyTaken;
        case HandoffRequestState::Rejected:
            return HandoffCancelDisposition::Rejected;
        case HandoffRequestState::Received:
        case HandoffRequestState::Pending:
        case HandoffRequestState::Unknown:
            return HandoffCancelDisposition::NotConfirmed;
    }
    return HandoffCancelDisposition::NotConfirmed;
}

QByteArray SingleInstanceProtocol::requestDigest(const SingleInstanceRequest &request) {
    QByteArray data = commandName(request.command).toUtf8();
    data.append('\n');
    for (const auto &path : request.paths) {
        data.append(path.toUtf8());
        data.append('\n');
    }
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

QByteArray SingleInstanceProtocol::encodeHandoffResponse(
    const SingleInstanceHandoffResponse &response) {
    const QJsonObject object{
        {QStringLiteral("protocolVersion"),  protocolVersion                       },
        {QStringLiteral("kind"),             QStringLiteral("application.handoffState")},
        {QStringLiteral("requestId"),        response.requestId                   },
        {QStringLiteral("instanceId"),       response.instanceId                  },
        {QStringLiteral("primaryProcessId"), response.primaryProcessId             },
        {QStringLiteral("hostMode"),         response.hostMode                     },
        {QStringLiteral("stage"),            startupStageName(response.stage)      },
        {QStringLiteral("heartbeatAgeMs"),   response.heartbeatAgeMs               },
        {QStringLiteral("state"),            handoffStateName(response.state)      },
        {QStringLiteral("error"),            response.error                        },
        {QStringLiteral("handoffVersion"),   response.handoffVersion               },
    };
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool SingleInstanceProtocol::decodeHandoffResponse(const QByteArray &payload,
                                                   SingleInstanceHandoffResponse &response,
                                                   QString &error) {
    QJsonObject object;
    if (!parseEnvelope(payload, object, error))
        return false;
    if (object.value(QStringLiteral("kind")).toString() !=
        QStringLiteral("application.handoffState")) {
        error = QStringLiteral("Not a handoff state response");
        return false;
    }
    response.requestId = object.value(QStringLiteral("requestId")).toString();
    if (response.requestId.isEmpty() ||
        !parseHandoffState(object.value(QStringLiteral("state")).toString(), response.state)) {
        error = QStringLiteral("Invalid handoff state response");
        return false;
    }
    if (!parseStartupStage(object.value(QStringLiteral("stage")).toString(), response.stage)) {
        error = QStringLiteral("Invalid primary startup stage");
        return false;
    }
    response.instanceId = object.value(QStringLiteral("instanceId")).toString();
    response.primaryProcessId = object.value(QStringLiteral("primaryProcessId")).toInteger();
    response.hostMode = object.value(QStringLiteral("hostMode")).toString();
    response.heartbeatAgeMs = object.value(QStringLiteral("heartbeatAgeMs")).toInteger(-1);
    response.error = object.value(QStringLiteral("error")).toString();
    response.handoffVersion = object.value(QStringLiteral("handoffVersion")).toInt(0);
    return true;
}

QString SingleInstanceProtocol::automationStateName(const SingleInstanceAutomationState state) {
    switch (state) {
        case SingleInstanceAutomationState::EditorStarting:
            return QStringLiteral("editor_starting");
        case SingleInstanceAutomationState::ServerDisabled:
            return QStringLiteral("server_disabled");
        case SingleInstanceAutomationState::ServerStarting:
            return QStringLiteral("server_starting");
        case SingleInstanceAutomationState::ServerReady:
            return QStringLiteral("server_ready");
        case SingleInstanceAutomationState::ServerStopping:
            return QStringLiteral("server_stopping");
        case SingleInstanceAutomationState::EditorStopping:
            return QStringLiteral("editor_stopping");
        case SingleInstanceAutomationState::Error:
            return QStringLiteral("error");
    }
    return {};
}

bool SingleInstanceProtocol::parseAutomationState(const QString &name,
                                                  SingleInstanceAutomationState &state) {
    if (name == QStringLiteral("editor_starting")) {
        state = SingleInstanceAutomationState::EditorStarting;
    } else if (name == QStringLiteral("server_disabled")) {
        state = SingleInstanceAutomationState::ServerDisabled;
    } else if (name == QStringLiteral("server_starting")) {
        state = SingleInstanceAutomationState::ServerStarting;
    } else if (name == QStringLiteral("server_ready")) {
        state = SingleInstanceAutomationState::ServerReady;
    } else if (name == QStringLiteral("server_stopping")) {
        state = SingleInstanceAutomationState::ServerStopping;
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
             {QStringLiteral("serverEnabled"), status.serverEnabled},
             {QStringLiteral("serverEndpoint"), status.serverEndpoint},
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
        !requireString(result, QStringLiteral("serverEndpoint"), status.serverEndpoint, error, true) ||
        !requireString(result, QStringLiteral("error"), status.error, error, true) ||
        !result.value(QStringLiteral("serverEnabled")).isBool()) {
        if (error.isEmpty())
            error = QStringLiteral("Invalid automation state field: serverEnabled");
        return false;
    }
    status.serverEnabled = result.value(QStringLiteral("serverEnabled")).toBool();
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

bool SingleInstanceProtocol::readFrame(QLocalSocket &socket, QByteArray &buffer,
                                       QByteArray &payload, QString &error,
                                       const int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    while (true) {
        if (socket.bytesAvailable() > 0)
            buffer.append(socket.readAll());
        error.clear();
        if (takeFrame(buffer, payload, error))
            return true;
        if (!error.isEmpty())
            return false;
        const auto remaining = timeoutMs - timer.elapsed();
        if (remaining <= 0) {
            error = QStringLiteral("control_response_timeout");
            return false;
        }
        if (!socket.waitForReadyRead(static_cast<int>(remaining))) {
            // Data can arrive just as the wait expires. Give the accumulated
            // buffer one last chance before declaring a timeout.
            if (socket.bytesAvailable() > 0)
                buffer.append(socket.readAll());
            error.clear();
            if (takeFrame(buffer, payload, error))
                return true;
            if (error.isEmpty())
                error = QStringLiteral("control_response_timeout");
            return false;
        }
    }
}
