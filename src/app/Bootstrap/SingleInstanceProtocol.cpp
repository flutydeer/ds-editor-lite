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
        }
        return {};
    }

    bool parseEnvelope(const QByteArray &payload, QJsonObject &object, QString &error) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            error = QStringLiteral("Invalid JSON payload");
            return false;
        }
        object = document.object();
        if (object.value(QStringLiteral("protocolVersion")).toInt(-1) !=
            SingleInstanceProtocol::protocolVersion) {
            error = QStringLiteral("Unsupported single-instance protocol version");
            return false;
        }
        return true;
    }
}

QByteArray SingleInstanceProtocol::encodeRequest(const SingleInstanceRequest &request) {
    const QJsonObject object{
        {QStringLiteral("protocolVersion"), protocolVersion                          },
        {QStringLiteral("requestId"),       request.requestId                        },
        {QStringLiteral("command"),         commandName(request.command)             },
        {QStringLiteral("paths"),           QJsonArray::fromStringList(request.paths)},
    };
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool SingleInstanceProtocol::decodeRequest(const QByteArray &payload,
                                           SingleInstanceRequest &request, QString &error) {
    QJsonObject object;
    if (!parseEnvelope(payload, object, error))
        return false;

    request.requestId = object.value(QStringLiteral("requestId")).toString();
    if (request.requestId.isEmpty()) {
        error = QStringLiteral("Missing request ID");
        return false;
    }

    const auto command = object.value(QStringLiteral("command")).toString();
    if (command == QStringLiteral("activate")) {
        request.command = SingleInstanceCommand::Activate;
    } else if (command == QStringLiteral("openProjects")) {
        request.command = SingleInstanceCommand::OpenProjects;
    } else {
        error = QStringLiteral("Unsupported command");
        return false;
    }

    request.paths.clear();
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
