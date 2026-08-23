#ifndef SINGLEINSTANCEPROTOCOL_H
#define SINGLEINSTANCEPROTOCOL_H

#include <QByteArray>
#include <QString>
#include <QStringList>

enum class SingleInstanceCommand {
    Activate,
    OpenProjects,
    AutomationDiscover,
    AutomationWatch,
};

enum class SingleInstanceAutomationState {
    Starting,
    McpDisabled,
    McpStarting,
    McpReady,
    McpStopping,
    EditorStopping,
    Error,
};

struct SingleInstanceConnectorInfo {
    QString instanceId;
    QString version;
};

struct SingleInstanceAutomationStatus {
    SingleInstanceAutomationState state = SingleInstanceAutomationState::Starting;
    QString editorInstanceId;
    QString executablePath;
    QString applicationVersion;
    QString buildId;
    QString hostMode;
    bool mcpEnabled = false;
    QString mcpEndpoint;
    QString error;
};

struct SingleInstanceRequest {
    QString requestId;
    SingleInstanceCommand command = SingleInstanceCommand::Activate;
    QStringList paths;
    SingleInstanceConnectorInfo connector;
};

struct SingleInstanceResponse {
    QString requestId;
    bool accepted = false;
    QString error;
    qint64 primaryProcessId = 0;
};

struct SingleInstanceAutomationSnapshot {
    QString requestId;
    qint64 primaryProcessId = 0;
    SingleInstanceAutomationStatus result;
};

namespace SingleInstanceProtocol {
    constexpr int protocolVersion = 1;
    constexpr qsizetype maxPayloadSize = 1024 * 1024;
    constexpr qsizetype maxWatcherCount = 32;
    constexpr qsizetype maxConnectionCount = maxWatcherCount + 16;
    constexpr qsizetype maxPendingWriteFrames = 64;
    constexpr qsizetype maxPendingWriteBytes = maxPayloadSize * 2;

    QByteArray encodeRequest(const SingleInstanceRequest &request);
    bool decodeRequest(const QByteArray &payload, SingleInstanceRequest &request, QString &error);

    QByteArray encodeResponse(const SingleInstanceResponse &response);
    bool decodeResponse(const QByteArray &payload, SingleInstanceResponse &response,
                        QString &error);

    QString automationStateName(SingleInstanceAutomationState state);
    bool parseAutomationState(const QString &name, SingleInstanceAutomationState &state);
    QByteArray encodeAutomationSnapshot(const SingleInstanceAutomationSnapshot &snapshot);
    bool decodeAutomationSnapshot(const QByteArray &payload,
                                  SingleInstanceAutomationSnapshot &snapshot, QString &error);

    QByteArray frame(const QByteArray &payload);
    bool takeFrame(QByteArray &buffer, QByteArray &payload, QString &error);
}

#endif // SINGLEINSTANCEPROTOCOL_H
