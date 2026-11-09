#ifndef SINGLEINSTANCEPROTOCOL_H
#define SINGLEINSTANCEPROTOCOL_H

#include <QByteArray>
#include <QString>
#include <QStringList>

class QLocalSocket;

enum class SingleInstanceCommand {
    Activate,
    OpenProjects,
    AutomationDiscover,
    AutomationWatch,
    // Versioned application handoff extension. Legacy peers reject these commands,
    // which lets a new secondary report an incompatibility instead of falling back
    // to the unreliable early ACK.
    ApplicationHandoff,
    ApplicationQuery,
    ApplicationCancel,
};

enum class SingleInstanceAutomationState {
    EditorStarting,
    ServerDisabled,
    ServerStarting,
    ServerReady,
    ServerStopping,
    EditorStopping,
    Error,
};

// Starting/Initializing are reported before the primary's main thread has shown a
// window. Ready is published only after the event loop has actually run.
enum class PrimaryStartupStage {
    Starting,
    Initializing,
    WindowShown,
    Ready,
    Stopping,
};

// Received only proves the IPC worker is alive. Taken is the only successful
// terminal state: the main thread has admitted the request into its work queue.
enum class HandoffRequestState {
    Received,
    Pending,
    Taken,
    Rejected,
    Cancelled,
    Unknown,
};

// How a secondary must interpret the response to application.cancel. Only an
// explicit Cancelled is a confirmed cancellation. A request already dispatched
// to the main thread reports Pending and must never be claimed as cancelled.
enum class HandoffCancelDisposition {
    Cancelled,
    AlreadyTaken,
    Rejected,
    NotConfirmed,
};

struct SingleInstanceConnectorInfo {
    QString instanceId;
    QString version;
};

struct SingleInstanceAutomationStatus {
    SingleInstanceAutomationState state = SingleInstanceAutomationState::EditorStarting;
    QString editorInstanceId;
    QString executablePath;
    QString applicationVersion;
    QString buildId;
    QString hostMode;
    bool serverEnabled = false;
    QString serverEndpoint;
    QString error;
};

struct SingleInstanceRequest {
    QString requestId;
    SingleInstanceCommand command = SingleInstanceCommand::Activate;
    QStringList paths;
    SingleInstanceConnectorInfo connector;
    // Target of application.query / application.cancel.
    QString targetRequestId;
    // Extension version advertised by a secondary that requires state handoff.
    int handoffVersion = 0;
};

// Legacy wire format. Kept unchanged so old secondaries still decode it. A
// successful legacy ACK is now emitted only after the main thread has taken the
// request, never when the IPC worker merely received the bytes.
struct SingleInstanceResponse {
    QString requestId;
    bool accepted = false;
    QString error;
    qint64 primaryProcessId = 0;
};

struct SingleInstanceHandoffResponse {
    QString requestId;
    QString instanceId;
    qint64 primaryProcessId = 0;
    QString hostMode;
    PrimaryStartupStage stage = PrimaryStartupStage::Starting;
    qint64 heartbeatAgeMs = -1;
    HandoffRequestState state = HandoffRequestState::Received;
    QString error;
    int handoffVersion = 0;
};

// Result of admitting a request into the primary's main-thread work queue.
struct AdmissionResult {
    bool taken = false;
    QString error;

    static AdmissionResult accepted() {
        return {true, {}};
    }
    static AdmissionResult rejected(const QString &reason) {
        return {false, reason};
    }
};

struct SingleInstanceAutomationSnapshot {
    QString requestId;
    qint64 primaryProcessId = 0;
    SingleInstanceAutomationStatus result;
};

namespace SingleInstanceProtocol {
    // The outer envelope version stays at 1 so legacy automation.discover/watch
    // peers keep working. The handoff capability is versioned separately.
    constexpr int protocolVersion = 1;
    constexpr int handoffVersion = 1;
    constexpr qsizetype maxPayloadSize = 1024 * 1024;
    constexpr qsizetype maxWatcherCount = 32;
    constexpr qsizetype maxConnectionCount = maxWatcherCount + 16;
    constexpr qsizetype maxPendingWriteFrames = 64;
    constexpr qsizetype maxPendingWriteBytes = maxPayloadSize * 2;
    constexpr int initialReadTimeoutMs = 1000;

    constexpr qsizetype maxInFlightRequests = 64;
    constexpr qsizetype maxTerminalRecords = 256;
    constexpr qint64 terminalRecordLifetimeMs = 10 * 60 * 1000;

    QByteArray encodeRequest(const SingleInstanceRequest &request);
    bool decodeRequest(const QByteArray &payload, SingleInstanceRequest &request, QString &error);

    QByteArray encodeResponse(const SingleInstanceResponse &response);
    bool decodeResponse(const QByteArray &payload, SingleInstanceResponse &response,
                        QString &error);

    QByteArray encodeHandoffResponse(const SingleInstanceHandoffResponse &response);
    bool decodeHandoffResponse(const QByteArray &payload, SingleInstanceHandoffResponse &response,
                               QString &error);

    // Stable digest of the request payload used to detect duplicate IDs with
    // conflicting content. The digest is computed on the primary side.
    QByteArray requestDigest(const SingleInstanceRequest &request);
    HandoffCancelDisposition cancelDisposition(HandoffRequestState state);

    QString handoffStateName(HandoffRequestState state);
    bool parseHandoffState(const QString &name, HandoffRequestState &state);
    QString startupStageName(PrimaryStartupStage stage);
    bool parseStartupStage(const QString &name, PrimaryStartupStage &stage);

    QString automationStateName(SingleInstanceAutomationState state);
    bool parseAutomationState(const QString &name, SingleInstanceAutomationState &state);
    QByteArray encodeAutomationSnapshot(const SingleInstanceAutomationSnapshot &snapshot);
    bool decodeAutomationSnapshot(const QByteArray &payload,
                                  SingleInstanceAutomationSnapshot &snapshot, QString &error);

    QByteArray frame(const QByteArray &payload);
    bool takeFrame(QByteArray &buffer, QByteArray &payload, QString &error);

    // Blocking read of exactly one framed control response. A local socket is
    // a byte stream: one frame can arrive across several reads, so the caller
    // must keep accumulating until a complete frame, a framing error or the
    // deadline. Using a single waitForReadyRead()/readAll() silently turns a
    // split but valid response into a control failure.
    bool readFrame(QLocalSocket &socket, QByteArray &buffer, QByteArray &payload,
                   QString &error, int timeoutMs);
}

#endif // SINGLEINSTANCEPROTOCOL_H
