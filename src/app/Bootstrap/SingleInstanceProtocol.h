#ifndef SINGLEINSTANCEPROTOCOL_H
#define SINGLEINSTANCEPROTOCOL_H

#include <QByteArray>
#include <QString>
#include <QStringList>

enum class SingleInstanceCommand {
    Activate,
    OpenProjects,
};

struct SingleInstanceRequest {
    QString requestId;
    SingleInstanceCommand command = SingleInstanceCommand::Activate;
    QStringList paths;
};

struct SingleInstanceResponse {
    QString requestId;
    bool accepted = false;
    QString error;
    qint64 primaryProcessId = 0;
};

namespace SingleInstanceProtocol {
    constexpr int protocolVersion = 1;
    constexpr qsizetype maxPayloadSize = 1024 * 1024;

    QByteArray encodeRequest(const SingleInstanceRequest &request);
    bool decodeRequest(const QByteArray &payload, SingleInstanceRequest &request, QString &error);

    QByteArray encodeResponse(const SingleInstanceResponse &response);
    bool decodeResponse(const QByteArray &payload, SingleInstanceResponse &response,
                        QString &error);

    QByteArray frame(const QByteArray &payload);
    bool takeFrame(QByteArray &buffer, QByteArray &payload, QString &error);
}

#endif // SINGLEINSTANCEPROTOCOL_H
