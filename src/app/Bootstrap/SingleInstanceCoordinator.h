#ifndef SINGLEINSTANCECOORDINATOR_H
#define SINGLEINSTANCECOORDINATOR_H

#include "AppHostMode.h"
#include "SingleInstanceProtocol.h"

#include <QMutex>
#include <QObject>

#include <functional>
#include <memory>

class QLockFile;
class QThread;
class QTimer;
class SingleInstanceIpcWorker;

// Structured outcome of a secondary handing its startup request to a primary.
// A boolean cannot express "the IPC thread is alive but the main thread never
// took the request", which is the failure mode this replaces.
enum class HandoffOutcome {
    Taken,
    Rejected,
    Cancelled,
    Unknown,
    Incompatible,
    Failed,
};

struct HandoffResult {
    HandoffOutcome outcome = HandoffOutcome::Failed;
    QString error;
    QString instanceId;
    qint64 primaryProcessId = 0;
    QString hostMode;
    PrimaryStartupStage stage = PrimaryStartupStage::Starting;
    qint64 heartbeatAgeMs = -1;
    QString diagnostics;
    // Set when the user explicitly confirmed terminating a stuck primary. The
    // caller re-enters the normal lock race instead of force-taking ownership.
    bool restartAfterRecovery = false;

    [[nodiscard]] bool success() const {
        return outcome == HandoffOutcome::Taken;
    }
    // Never replay an open-project command when the outcome is not a clean
    // rejection: the primary may already have taken the request.
    [[nodiscard]] bool safeToRetry() const {
        return outcome == HandoffOutcome::Rejected ||
               outcome == HandoffOutcome::Incompatible || outcome == HandoffOutcome::Failed;
    }
};

class SingleInstanceCoordinator final : public QObject {
    Q_OBJECT
public:
    enum class StartResult { Primary, Secondary, Error };

    using AdmissionHandler = std::function<AdmissionResult(const SingleInstanceRequest &)>;

    explicit SingleInstanceCoordinator(AppHostMode hostMode, QObject *parent = nullptr);
    explicit SingleInstanceCoordinator(QString dataDirectory = {}, QString serverName = {},
                                       QObject *parent = nullptr);
    ~SingleInstanceCoordinator() override;

    Q_DISABLE_COPY_MOVE(SingleInstanceCoordinator)

    StartResult start();
    bool forwardRequest(const SingleInstanceRequest &request, QString &error);

    // Preferred handler: the main thread decides whether the request was really
    // admitted into its work queue. A rejection is propagated to the secondary.
    void setAdmissionHandler(AdmissionHandler handler);
    // Compatibility wrapper for callers that do not need to reject requests.
    void setRequestHandler(std::function<void(const SingleInstanceRequest &)> handler);

    // Main-thread takeover boundary. Nothing is dispatched to the main thread
    // before this call, so an instance stuck during initialization cannot ACK.
    void enableAdmission();
    void setPrimaryStage(PrimaryStartupStage stage);
    void noteMainThreadHeartbeat();

    void pauseRequestDispatchAndFlush();
    void resumeRequestDispatch();
    void updateAutomationState(const SingleInstanceAutomationStatus &status);
    void updateAutomationState(SingleInstanceAutomationState state, bool serverEnabled,
                               QString serverEndpoint = {}, QString error = {});
    void broadcastAutomationState();
    void stopAcceptingRequests();
    void shutdown();

    [[nodiscard]] QString errorString() const;
    [[nodiscard]] QString serverName() const;
    [[nodiscard]] SingleInstanceAutomationStatus automationState() const;
    [[nodiscard]] QString instanceId() const;
    [[nodiscard]] qint64 primaryProcessIdFromLock() const;

private:
    friend class SingleInstanceIpcWorker;

    void admitRequest(const QString &requestId, const SingleInstanceRequest &request);
    void allowPrimaryToTakeForeground(qint64 processId) const;
    void applyAutomationState(const SingleInstanceAutomationStatus &status, bool broadcast);
    void installHandler(AdmissionHandler handler);

    QString m_dataDirectory;
    QString m_serverName;
    QString m_error;
    std::unique_ptr<QLockFile> m_lockFile;
    QThread *m_ipcThread = nullptr;
    SingleInstanceIpcWorker *m_worker = nullptr;
    QTimer *m_heartbeatTimer = nullptr;
    AdmissionHandler m_requestHandler;
    QList<SingleInstanceRequest> m_pendingRequests;
    mutable QMutex m_automationStateMutex;
    SingleInstanceAutomationStatus m_automationStatus;
};

#endif // SINGLEINSTANCECOORDINATOR_H
