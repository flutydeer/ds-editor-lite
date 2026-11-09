#ifndef SECONDARYHANDOFF_H
#define SECONDARYHANDOFF_H

#include "AppHostMode.h"
#include "SingleInstanceCoordinator.h"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <functional>

class QLocalSocket;
class QTimer;
class HandoffFeedbackDialog;

struct SecondaryHandoffOptions {
    // Connection retry budget. Unrelated to how long we wait for the primary's
    // main thread: an IPC server that answers immediately is not evidence of a
    // healthy primary.
    int connectTimeoutMs = 3000;
    int progressDelayMs = 500;
    int recoveryDelayMs = 10000;
    int totalBudgetMs = 30000;
    bool interactive = true;
    bool allowTerminateRecovery = true;
};

// Lightweight secondary-side handoff controller. It deliberately depends on
// nothing from AppContext, inference or audio: it must remain usable while the
// primary is stuck before any of those subsystems exist.
//
// run() drives its own nested event loop, so the progress dialog stays
// responsive without blocking the secondary's UI thread. Non-interactive
// launches report everything through stderr and a non-zero exit code.
class SecondaryHandoffController final : public QObject {
    Q_OBJECT
public:
    SecondaryHandoffController(QString serverName, QString dataDirectory,
                               AppHostMode ownHostMode, QObject *parent = nullptr);

    HandoffResult run(const SingleInstanceRequest &startupRequest,
                      const SecondaryHandoffOptions &options = {});

    [[nodiscard]] QString diagnostics() const {
        return m_diagnostics;
    }
    [[nodiscard]] bool restartAfterRecovery() const {
        return m_restartAfterRecovery;
    }

private:
    void onFrame(const QByteArray &payload);
    void publishStatus(const QString &status);
    void finish(HandoffOutcome outcome, QString error, bool closeSocket = true);
    void cancelPendingRequest();
    void queryPendingResult();
    bool recoverStuckPrimary();
    void appendDiagnostic(const QString &key, const QString &value);
    void refreshFeedback();

    QString m_serverName;
    QString m_dataDirectory;
    AppHostMode m_ownHostMode;
    QLocalSocket *m_socket = nullptr;
    QTimer *m_progressTimer = nullptr;
    QTimer *m_recoveryTimer = nullptr;
    QTimer *m_budgetTimer = nullptr;
    HandoffFeedbackDialog *m_feedback = nullptr;
    QByteArray m_readBuffer;
    QString m_requestId;
    QString m_primaryInstanceId;
    QString m_primaryHostMode;
    qint64 m_primaryProcessId = 0;
    PrimaryStartupStage m_stage = PrimaryStartupStage::Starting;
    qint64 m_heartbeatAgeMs = -1;
    QString m_status;
    QString m_diagnostics;
    HandoffResult m_result;
    SecondaryHandoffOptions m_options;
    bool m_finished = false;
    bool m_recoveryOffered = false;
    bool m_restartAfterRecovery = false;
    std::function<void()> m_stopLoop;
};

#endif // SECONDARYHANDOFF_H
