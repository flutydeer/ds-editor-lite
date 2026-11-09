#include "SecondaryHandoff.h"

#include "AppHostMode.h"
#include "SingleInstanceIdentity.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocalSocket>
#include <QLockFile>
#include <QMessageBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QTextStream>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {
    // Windows reports native backslashes while QFileInfo::canonicalFilePath()
    // returns Qt separators. Comparing them directly rejects the same binary
    // on a normal installation, so both sides must use one representation.
    QString normalizedExecutablePath(const QString &path) {
        return QDir::cleanPath(QDir::fromNativeSeparators(path)).toLower();
    }

    void allowPrimaryToTakeForeground(const QString &dataDirectory) {
        if (dataDirectory.isEmpty())
            return;
        QLockFile lock(SingleInstanceIdentity::lockFilePath(
            SingleInstanceIdentity::normalizeDataDirectory(dataDirectory)));
        qint64 pid = 0;
        QString hostname;
        QString applicationName;
        if (!lock.getLockInfo(&pid, &hostname, &applicationName) || pid <= 0)
            return;
#ifdef Q_OS_WIN
        // Windows restricts activateWindow() on a background process. The
        // secondary grants the primary foreground rights before the handoff,
        // matching what the legacy forwardRequest() path used to do.
        AllowSetForegroundWindow(static_cast<DWORD>(pid));
#endif
    }

#ifdef Q_OS_WIN
    struct ProcessIdentity {
        qint64 pid = 0;
        QString executablePath;
        qint64 creationFileTime = 0;
    };

    bool captureProcessIdentity(const qint64 pid, ProcessIdentity &identity, QString &error) {
        if (pid <= 0) {
            error = QStringLiteral("invalid_primary_pid");
            return false;
        }
        auto *handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE,
                                   static_cast<DWORD>(pid));
        if (!handle) {
            error = QStringLiteral("open_process_failed");
            return false;
        }
        const auto closeHandle = qScopeGuard([handle] { CloseHandle(handle); });
        wchar_t imagePath[MAX_PATH];
        DWORD imageLength = MAX_PATH;
        if (!QueryFullProcessImageNameW(handle, 0, imagePath, &imageLength)) {
            error = QStringLiteral("query_image_failed");
            return false;
        }
        FILETIME creation{};
        FILETIME exitTime{};
        FILETIME kernelTime{};
        FILETIME userTime{};
        if (!GetProcessTimes(handle, &creation, &exitTime, &kernelTime, &userTime)) {
            error = QStringLiteral("query_creation_time_failed");
            return false;
        }
        identity.pid = pid;
        identity.executablePath =
            normalizedExecutablePath(QString::fromWCharArray(imagePath, imageLength));
        identity.creationFileTime =
            (static_cast<qint64>(creation.dwHighDateTime) << 32) | creation.dwLowDateTime;
        return true;
    }

    bool sameIdentity(const ProcessIdentity &a, const ProcessIdentity &b) {
        return a.pid == b.pid && a.creationFileTime == b.creationFileTime &&
               a.executablePath.compare(b.executablePath, Qt::CaseInsensitive) == 0;
    }

    // The handle is opened once and used for both the final identity check and
    // the termination, so a recycled PID cannot redirect the kill.
    bool terminateVerifiedPrimary(const ProcessIdentity &expected, QString &error) {
        auto *handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE,
                                   static_cast<DWORD>(expected.pid));
        if (!handle) {
            error = QStringLiteral("open_process_failed");
            return false;
        }
        const auto closeHandle = qScopeGuard([handle] { CloseHandle(handle); });
        wchar_t imagePath[MAX_PATH];
        DWORD imageLength = MAX_PATH;
        if (!QueryFullProcessImageNameW(handle, 0, imagePath, &imageLength)) {
            error = QStringLiteral("query_image_failed");
            return false;
        }
        FILETIME creation{};
        FILETIME exitTime{};
        FILETIME kernelTime{};
        FILETIME userTime{};
        if (!GetProcessTimes(handle, &creation, &exitTime, &kernelTime, &userTime)) {
            error = QStringLiteral("query_creation_time_failed");
            return false;
        }
        ProcessIdentity current;
        current.pid = expected.pid;
        current.executablePath =
            normalizedExecutablePath(QString::fromWCharArray(imagePath, imageLength));
        current.creationFileTime =
            (static_cast<qint64>(creation.dwHighDateTime) << 32) | creation.dwLowDateTime;
        const auto ownPath = normalizedExecutablePath(
            QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath());
        if (!sameIdentity(current, expected) ||
            current.executablePath.compare(ownPath, Qt::CaseInsensitive) != 0) {
            error = QStringLiteral("primary_identity_changed");
            return false;
        }
        if (!TerminateProcess(handle, 1)) {
            error = QStringLiteral("terminate_process_failed");
            return false;
        }
        // Wait for the real process to leave before the caller re-enters the
        // lock race. A timeout does not grant permission to force ownership.
        if (WaitForSingleObject(handle, 15000) != WAIT_OBJECT_0) {
            error = QStringLiteral("primary_exit_not_confirmed");
            return false;
        }
        return true;
    }

    bool captureLockOwner(const QString &dataDirectory, ProcessIdentity &identity,
                          QString &error) {
        QLockFile lock(SingleInstanceIdentity::lockFilePath(
            SingleInstanceIdentity::normalizeDataDirectory(dataDirectory)));
        qint64 pid = 0;
        QString hostname;
        QString applicationName;
        if (!lock.getLockInfo(&pid, &hostname, &applicationName) || pid <= 0) {
            error = QStringLiteral("lock_identity_unavailable");
            return false;
        }
        return captureProcessIdentity(pid, identity, error);
    }
#endif

    QString hostModeName(const QString &name) {
        return name.isEmpty() ? QStringLiteral("unknown") : name;
    }

    QString stateText(const HandoffRequestState state, const PrimaryStartupStage stage,
                      const qint64 heartbeatAgeMs, const QString &primaryHostMode,
                      const bool ownGui) {
        switch (state) {
            case HandoffRequestState::Taken:
                if (ownGui && primaryHostMode.contains(QStringLiteral("headless"),
                                                       Qt::CaseInsensitive))
                    return QObject::tr("Taken over by a background instance. No window will open.");
                return QObject::tr("Request taken over by the previous instance.");
            case HandoffRequestState::Rejected:
                return QObject::tr("The previous instance rejected the request.");
            case HandoffRequestState::Cancelled:
                return QObject::tr("The handoff request was cancelled.");
            case HandoffRequestState::Unknown:
                return QObject::tr("Handoff result is unknown.");
            case HandoffRequestState::Received:
            case HandoffRequestState::Pending:
                break;
        }
        if (stage == PrimaryStartupStage::Stopping)
            return QObject::tr("The previous instance is shutting down.");
        if (heartbeatAgeMs < 0 || heartbeatAgeMs > 3000)
            return QObject::tr("The previous instance's main thread is not responding.");
        return QObject::tr("The previous instance is starting.");
    }

    QString outcomeName(const HandoffOutcome outcome) {
        switch (outcome) {
            case HandoffOutcome::Taken:
                return QStringLiteral("taken");
            case HandoffOutcome::Rejected:
                return QStringLiteral("rejected");
            case HandoffOutcome::Cancelled:
                return QStringLiteral("cancelled");
            case HandoffOutcome::Unknown:
                return QStringLiteral("unknown");
            case HandoffOutcome::Incompatible:
                return QStringLiteral("incompatible");
            case HandoffOutcome::Failed:
                return QStringLiteral("failed");
        }
        return QStringLiteral("failed");
    }

    bool sendControlCommand(const QString &serverName, const SingleInstanceRequest &request,
                            SingleInstanceHandoffResponse &response, QString &error) {
        QLocalSocket socket;
        socket.connectToServer(serverName, QIODevice::ReadWrite);
        if (!socket.waitForConnected(1000)) {
            error = QStringLiteral("control_connection_failed");
            return false;
        }
        const auto message =
            SingleInstanceProtocol::frame(SingleInstanceProtocol::encodeRequest(request));
        if (socket.write(message) != message.size() || !socket.waitForBytesWritten(1000)) {
            error = QStringLiteral("control_write_failed");
            return false;
        }
        QByteArray buffer;
        if (!socket.waitForReadyRead(1500)) {
            error = QStringLiteral("control_response_timeout");
            return false;
        }
        buffer.append(socket.readAll());
        QByteArray payload;
        if (!SingleInstanceProtocol::takeFrame(buffer, payload, error))
            return false;
        return SingleInstanceProtocol::decodeHandoffResponse(payload, response, error);
    }

    SingleInstanceRequest statusQuery(const QString &targetRequestId) {
        SingleInstanceRequest request;
        request.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        request.command = SingleInstanceCommand::ApplicationQuery;
        request.targetRequestId = targetRequestId;
        return request;
    }
}

// A deliberately small, self-contained dialog. It must not reference
// AppContext, inference or audio: the primary may be stuck before those exist.
class HandoffFeedbackDialog final : public QDialog {
public:
    explicit HandoffFeedbackDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle(QObject::tr("Opening in the running instance"));
        setModal(false);
        auto *layout = new QVBoxLayout(this);
        m_status = new QLabel(QObject::tr("Waiting for the running instance..."), this);
        m_status->setWordWrap(true);
        layout->addWidget(m_status);
        m_details = new QLabel(this);
        m_details->setWordWrap(true);
        m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(m_details);
        auto *buttonRow = new QHBoxLayout;
        m_continue = new QPushButton(QObject::tr("Keep waiting"), this);
        m_cancel = new QPushButton(QObject::tr("Cancel pending handoff"), this);
        m_copy = new QPushButton(QObject::tr("Copy diagnostics"), this);
        m_terminate = new QPushButton(QObject::tr("End previous instance and retry"), this);
        buttonRow->addWidget(m_continue);
        buttonRow->addWidget(m_cancel);
        buttonRow->addWidget(m_copy);
        buttonRow->addWidget(m_terminate);
        layout->addLayout(buttonRow);
        setRecoveryVisible(false);
    }

    void setStatus(const QString &text) {
        m_status->setText(text);
    }
    void setDetails(const QString &text) {
        m_details->setText(text);
    }
    void setRecoveryVisible(const bool visible) {
        m_continue->setVisible(visible);
        m_cancel->setVisible(visible);
        m_copy->setVisible(visible);
        m_terminate->setVisible(visible);
    }
    void setTerminateEnabled(const bool enabled) {
        m_terminate->setEnabled(enabled);
    }

    QPushButton *continueButton() const {
        return m_continue;
    }
    QPushButton *cancelButton() const {
        return m_cancel;
    }
    QPushButton *copyButton() const {
        return m_copy;
    }
    QPushButton *terminateButton() const {
        return m_terminate;
    }

private:
    QLabel *m_status = nullptr;
    QLabel *m_details = nullptr;
    QPushButton *m_continue = nullptr;
    QPushButton *m_cancel = nullptr;
    QPushButton *m_copy = nullptr;
    QPushButton *m_terminate = nullptr;
};

SecondaryHandoffController::SecondaryHandoffController(QString serverName,
                                                       QString dataDirectory,
                                                       const AppHostMode ownHostMode,
                                                       QObject *parent)
    : QObject(parent), m_serverName(std::move(serverName)),
      m_dataDirectory(std::move(dataDirectory)), m_ownHostMode(ownHostMode) {
}

void SecondaryHandoffController::appendDiagnostic(const QString &key, const QString &value) {
    if (!m_diagnostics.isEmpty())
        m_diagnostics.append(QStringLiteral("\n"));
    m_diagnostics.append(key).append(QStringLiteral("=")).append(value);
}

void SecondaryHandoffController::publishStatus(const QString &status) {
    if (status == m_status)
        return;
    m_status = status;
    if (!m_options.interactive)
        QTextStream(stderr) << "single-instance handoff: " << status << Qt::endl;
    else if (m_feedback)
        m_feedback->setStatus(status);
}

void SecondaryHandoffController::refreshFeedback() {
    if (!m_feedback)
        return;
    m_feedback->setStatus(m_status);
    m_feedback->setDetails(m_diagnostics);
}

HandoffResult SecondaryHandoffController::run(const SingleInstanceRequest &startupRequest,
                                             const SecondaryHandoffOptions &options) {
    m_options = options;
    m_result = {};
    m_finished = false;
    m_statusPollCount = 0;
    m_restartAfterRecovery = false;
    m_recoveryOffered = false;
    m_diagnostics.clear();
    m_status.clear();
    m_readBuffer.clear();
    m_requestId = startupRequest.requestId;
    m_stage = PrimaryStartupStage::Starting;
    m_heartbeatAgeMs = -1;

    appendDiagnostic(QStringLiteral("requestId"), m_requestId);
    appendDiagnostic(QStringLiteral("ownHostMode"), appHostModeName(m_ownHostMode));
    appendDiagnostic(QStringLiteral("server"), m_serverName);
    appendDiagnostic(QStringLiteral("handoffVersion"),
                     QString::number(SingleInstanceProtocol::handoffVersion));

    SingleInstanceRequest handoff = startupRequest;
    handoff.command = SingleInstanceCommand::ApplicationHandoff;
    handoff.handoffVersion = SingleInstanceProtocol::handoffVersion;

    QEventLoop loop;
    m_stopLoop = [&loop] { loop.quit(); };

    m_socket = new QLocalSocket(this);
    const auto sendHandoff = [this, handoff] {
        appendDiagnostic(QStringLiteral("connected"), QStringLiteral("yes"));
        // Restore the foreground grant that the legacy forwardRequest() path
        // used to make. Without it Windows may refuse the primary's
        // activateWindow() call when the request arrives from the background.
        allowPrimaryToTakeForeground(m_dataDirectory);
        const auto message =
            SingleInstanceProtocol::frame(SingleInstanceProtocol::encodeRequest(handoff));
        if (m_socket->write(message) != message.size())
            finish(HandoffOutcome::Failed, QStringLiteral("Failed to send the handoff request"));
    };

    int connectAttempts = 0;
    const auto connectToPrimary = [this, &connectAttempts] {
        ++connectAttempts;
        m_socket->abort();
        m_socket->connectToServer(m_serverName, QIODevice::ReadWrite);
    };

    QObject::connect(m_socket, &QLocalSocket::connected, &loop, sendHandoff);
    QObject::connect(m_socket, &QLocalSocket::readyRead, &loop, [this] {
        m_readBuffer.append(m_socket->readAll());
        while (true) {
            QByteArray payload;
            QString frameError;
            if (!SingleInstanceProtocol::takeFrame(m_readBuffer, payload, frameError)) {
                if (!frameError.isEmpty())
                    finish(HandoffOutcome::Failed, frameError);
                return;
            }
            onFrame(payload);
            if (m_finished)
                return;
        }
    });
    QObject::connect(m_socket, &QLocalSocket::errorOccurred, &loop,
                     [this, &loop, &connectToPrimary, &connectAttempts](
                         const QLocalSocket::LocalSocketError socketError) {
                         if (m_finished || socketError == QLocalSocket::PeerClosedError)
                             return;
                         if (connectAttempts * 100 < m_options.connectTimeoutMs)
                             QTimer::singleShot(100, &loop, connectToPrimary);
                         else
                             finish(HandoffOutcome::Failed,
                                    QObject::tr("Could not connect to the running instance."));
                     });
    QObject::connect(m_socket, &QLocalSocket::disconnected, &loop, [this] {
        if (m_finished)
            return;
        // A terminal frame may have been lost after takeover. Query the ledger
        // instead of blindly replaying an open-project command.
        queryPendingResult();
    });

    m_progressTimer = new QTimer(this);
    m_progressTimer->setSingleShot(true);
    QObject::connect(m_progressTimer, &QTimer::timeout, &loop, [this, &loop] {
        publishStatus(stateText(HandoffRequestState::Pending, m_stage, m_heartbeatAgeMs,
                                m_primaryHostMode, m_ownHostMode == AppHostMode::Gui));
        if (m_options.interactive && !m_feedback) {
            m_feedback = new HandoffFeedbackDialog;
            refreshFeedback();
            m_feedback->show();
            QObject::connect(m_feedback->cancelButton(), &QPushButton::clicked, &loop,
                             [this] { cancelPendingRequest(); });
            QObject::connect(m_feedback->copyButton(), &QPushButton::clicked, &loop, [this] {
                if (auto *gui = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
                    if (auto *clipboard = gui->clipboard())
                        clipboard->setText(m_diagnostics);
                } else {
                    QTextStream(stderr) << m_diagnostics << Qt::endl;
                }
            });
            QObject::connect(m_feedback->continueButton(), &QPushButton::clicked, &loop,
                             [this] {
                                 m_budgetTimer->start(m_options.totalBudgetMs);
                                 m_recoveryTimer->start(m_options.recoveryDelayMs);
                             });
            QObject::connect(m_feedback->terminateButton(), &QPushButton::clicked, &loop,
                             [this] {
                                 if (recoverStuckPrimary())
                                     finish(HandoffOutcome::Failed,
                                            QObject::tr("The previous instance was ended. "
                                                        "Restarting the handoff."));
                             });
        }
    });
    m_progressTimer->start(m_options.progressDelayMs);

    m_recoveryTimer = new QTimer(this);
    m_recoveryTimer->setSingleShot(true);
    QObject::connect(m_recoveryTimer, &QTimer::timeout, &loop, [this] {
        m_recoveryOffered = true;
        if (!m_options.interactive) {
            QTextStream(stderr)
                << "single-instance handoff: still waiting after "
                << m_options.recoveryDelayMs
                << " ms. The previous instance may be stuck; see diagnostics." << Qt::endl;
            return;
        }
        if (m_feedback) {
            m_feedback->setRecoveryVisible(true);
#ifndef Q_OS_WIN
            m_feedback->setTerminateEnabled(false);
#else
            m_feedback->setTerminateEnabled(m_options.allowTerminateRecovery);
#endif
            refreshFeedback();
        }
    });
    m_recoveryTimer->start(m_options.recoveryDelayMs);

    m_budgetTimer = new QTimer(this);
    m_budgetTimer->setSingleShot(true);
    QObject::connect(m_budgetTimer, &QTimer::timeout, &loop, [this] {
        // A timeout is not proof that the process is permanently hung. Report
        // an unknown result and never replay the open-project command.
        finish(HandoffOutcome::Unknown,
               QObject::tr("The previous instance did not confirm the handoff in time. "
                           "The result is unknown."));
    });
    m_budgetTimer->start(m_options.totalBudgetMs);

    // The primary only pushes a frame when the request changes state. A main
    // thread that stalls after the receipt never sends anything, so poll the
    // ledger to keep the heartbeat age, startup stage and status text current.
    m_statusPollTimer = new QTimer(this);
    QObject::connect(m_statusPollTimer, &QTimer::timeout, &loop,
                     [this] { pollPendingState(); });
    m_statusPollTimer->start(1000);

    connectToPrimary();
    loop.exec();

    m_stopLoop = {};
    return m_result;
}

void SecondaryHandoffController::onFrame(const QByteArray &payload) {
    SingleInstanceHandoffResponse response;
    QString error;
    if (SingleInstanceProtocol::decodeHandoffResponse(payload, response, error)) {
        if (!response.requestId.isEmpty() && response.requestId != m_requestId)
            return;
        m_primaryInstanceId = response.instanceId;
        m_primaryProcessId = response.primaryProcessId;
        m_primaryHostMode = response.hostMode;
        m_stage = response.stage;
        m_heartbeatAgeMs = response.heartbeatAgeMs;
        appendDiagnostic(QStringLiteral("primaryInstanceId"), m_primaryInstanceId);
        appendDiagnostic(QStringLiteral("primaryPid"),
                         QString::number(m_primaryProcessId));
        appendDiagnostic(QStringLiteral("primaryHostMode"),
                         hostModeName(m_primaryHostMode));
        appendDiagnostic(QStringLiteral("stage"),
                         SingleInstanceProtocol::startupStageName(m_stage));
        appendDiagnostic(QStringLiteral("heartbeatAgeMs"),
                         QString::number(m_heartbeatAgeMs));
        appendDiagnostic(QStringLiteral("state"),
                         SingleInstanceProtocol::handoffStateName(response.state));
        refreshFeedback();
        publishStatus(stateText(response.state, m_stage, m_heartbeatAgeMs, m_primaryHostMode,
                                m_ownHostMode == AppHostMode::Gui));
        switch (response.state) {
            case HandoffRequestState::Taken:
                finish(HandoffOutcome::Taken, {});
                return;
            case HandoffRequestState::Rejected:
                finish(HandoffOutcome::Rejected, response.error);
                return;
            case HandoffRequestState::Cancelled:
                finish(HandoffOutcome::Cancelled, response.error);
                return;
            case HandoffRequestState::Unknown:
                finish(HandoffOutcome::Unknown, response.error);
                return;
            case HandoffRequestState::Received:
            case HandoffRequestState::Pending:
                return;
        }
    }

    // A legacy primary answers with the old response shape. Never treat its
    // early ACK as success and never start a second primary.
    SingleInstanceResponse legacy;
    QString legacyError;
    if (SingleInstanceProtocol::decodeResponse(payload, legacy, legacyError)) {
        appendDiagnostic(QStringLiteral("compatibility"),
                         QStringLiteral("legacy_primary_without_handoff"));
        finish(HandoffOutcome::Incompatible,
               QObject::tr("The running instance does not support the versioned handoff "
                           "protocol. Close it manually or update both installations."));
        return;
    }
    finish(HandoffOutcome::Failed, error);
}

void SecondaryHandoffController::finish(const HandoffOutcome outcome, QString error,
                                        const bool closeSocket) {
    if (m_finished)
        return;
    m_finished = true;
    m_result.outcome = outcome;
    m_result.error = std::move(error);
    m_result.instanceId = m_primaryInstanceId;
    m_result.primaryProcessId = m_primaryProcessId;
    m_result.hostMode = m_primaryHostMode;
    m_result.stage = m_stage;
    m_result.heartbeatAgeMs = m_heartbeatAgeMs;
    m_result.status = m_status;
    appendDiagnostic(QStringLiteral("statusPollCount"), QString::number(m_statusPollCount));
    m_result.diagnostics = m_diagnostics;
    m_result.restartAfterRecovery = m_restartAfterRecovery;

    for (auto *timer : {m_progressTimer, m_statusPollTimer, m_recoveryTimer, m_budgetTimer}) {
        if (timer)
            timer->stop();
    }
    if (closeSocket && m_socket) {
        QObject::disconnect(m_socket, nullptr, nullptr, nullptr);
        m_socket->abort();
    }
    if (m_feedback) {
        m_feedback->hide();
        m_feedback->deleteLater();
        m_feedback = nullptr;
    }
    if (!m_options.interactive) {
        QTextStream(stderr) << "single-instance handoff result=" << outcomeName(outcome)
                            << " error=" << m_result.error << Qt::endl;
        QTextStream(stderr) << m_diagnostics << Qt::endl;
    }
    if (m_stopLoop)
        m_stopLoop();
}

void SecondaryHandoffController::cancelPendingRequest() {
    SingleInstanceRequest request;
    request.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.command = SingleInstanceCommand::ApplicationCancel;
    request.targetRequestId = m_requestId;
    SingleInstanceHandoffResponse response;
    QString error;
    if (sendControlCommand(m_serverName, request, response, error)) {
        appendDiagnostic(QStringLiteral("cancelState"),
                         SingleInstanceProtocol::handoffStateName(response.state));
        switch (SingleInstanceProtocol::cancelDisposition(response.state)) {
            case HandoffCancelDisposition::Cancelled:
                appendDiagnostic(QStringLiteral("cancel"), QStringLiteral("confirmed"));
                finish(HandoffOutcome::Cancelled, {});
                return;
            case HandoffCancelDisposition::AlreadyTaken:
                finish(HandoffOutcome::Taken, {});
                return;
            case HandoffCancelDisposition::Rejected:
                finish(HandoffOutcome::Rejected, response.error);
                return;
            case HandoffCancelDisposition::NotConfirmed:
                // The primary already dispatched the request to its main
                // thread. Cancelling is impossible, so keep observing the
                // original handoff instead of claiming a successful cancel.
                appendDiagnostic(QStringLiteral("cancel"), QStringLiteral("not_confirmed"));
                publishStatus(QObject::tr(
                    "Cancellation was not confirmed. The request was already dispatched, "
                    "so its result is unknown. Still waiting for the previous instance."));
                return;
        }
    }
    appendDiagnostic(QStringLiteral("cancelError"), error);
    finish(HandoffOutcome::Unknown,
           QObject::tr("Could not confirm the cancellation. The handoff result is unknown."));
}

void SecondaryHandoffController::queryPendingResult() {
    if (m_finished)
        return;
    SingleInstanceHandoffResponse response;
    QString error;
    if (!sendControlCommand(m_serverName, statusQuery(m_requestId), response, error)) {
        finish(HandoffOutcome::Unknown,
               QObject::tr("Lost the connection to the running instance. The handoff result "
                           "is unknown; the project was not reopened."));
        return;
    }
    appendDiagnostic(QStringLiteral("queryState"),
                     SingleInstanceProtocol::handoffStateName(response.state));
    // The connection was lost, so a non-terminal answer means the result can no
    // longer be observed on this channel.
    applyQueriedState(response, true);
}

void SecondaryHandoffController::pollPendingState() {
    if (m_finished)
        return;
    SingleInstanceHandoffResponse response;
    QString error;
    if (!sendControlCommand(m_serverName, statusQuery(m_requestId), response, error)) {
        // A transient control-connection failure is not proof of a lost
        // handoff. Keep observing; the budget timer remains the final authority.
        return;
    }
    ++m_statusPollCount;
    applyQueriedState(response, false);
}

void SecondaryHandoffController::applyQueriedState(
    const SingleInstanceHandoffResponse &response, const bool requireTerminal) {
    // Query responses carry their own requestId, so they cannot be routed
    // through onFrame's request-id filter.
    if (!response.instanceId.isEmpty())
        m_primaryInstanceId = response.instanceId;
    if (response.primaryProcessId > 0)
        m_primaryProcessId = response.primaryProcessId;
    if (!response.hostMode.isEmpty())
        m_primaryHostMode = response.hostMode;
    m_stage = response.stage;
    m_heartbeatAgeMs = response.heartbeatAgeMs;
    publishStatus(stateText(response.state, m_stage, m_heartbeatAgeMs, m_primaryHostMode,
                            m_ownHostMode == AppHostMode::Gui));
    refreshFeedback();

    switch (response.state) {
        case HandoffRequestState::Taken:
            finish(HandoffOutcome::Taken, {});
            return;
        case HandoffRequestState::Rejected:
            finish(HandoffOutcome::Rejected, response.error);
            return;
        case HandoffRequestState::Cancelled:
            finish(HandoffOutcome::Cancelled, response.error);
            return;
        case HandoffRequestState::Unknown:
            finish(HandoffOutcome::Unknown,
                   response.error.isEmpty()
                       ? QObject::tr("The handoff result is unknown; the project was not reopened.")
                       : response.error);
            return;
        case HandoffRequestState::Received:
        case HandoffRequestState::Pending:
            if (requireTerminal)
                finish(HandoffOutcome::Unknown,
                       QObject::tr("The running instance has not reported a terminal handoff "
                                   "result."));
            return;
    }
}

bool SecondaryHandoffController::recoverStuckPrimary() {
#ifndef Q_OS_WIN
    QMessageBox::information(nullptr, QObject::tr("Manual recovery required"),
                             QObject::tr("Ending the previous instance is only supported on "
                                         "Windows. Please close it manually."));
    return false;
#else
    if (!m_options.allowTerminateRecovery || m_finished)
        return false;

    // Bind the target BEFORE the modal confirmations. While the user reads
    // those dialogs the stuck primary can exit and a healthy instance can take
    // the lock. That healthy instance must never become the kill target.
    ProcessIdentity target;
    QString error;
    if (!captureLockOwner(m_dataDirectory, target, error)) {
        appendDiagnostic(QStringLiteral("recovery"), error);
        return false;
    }
    appendDiagnostic(QStringLiteral("recoveryTargetPid"), QString::number(target.pid));
    appendDiagnostic(QStringLiteral("recoveryTargetPath"), target.executablePath);

    const auto warning = QMessageBox::warning(
        nullptr, QObject::tr("End the previous instance?"),
        QObject::tr("The previous instance has not responded. Ending it may discard unsaved "
                    "project changes. This cannot be undone.\n\nOnly continue if you have "
                    "confirmed that no work needs to be saved."),
        QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel);
    if (warning != QMessageBox::Yes)
        return false;
    const auto confirmation = QMessageBox::warning(
        nullptr, QObject::tr("Confirm ending the previous instance"),
        QObject::tr("Confirm a second time: end the previous instance and retry the handoff?"),
        QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel);
    if (confirmation != QMessageBox::Yes)
        return false;

    // The original handoff may have completed while the dialogs were open.
    if (m_finished) {
        appendDiagnostic(QStringLiteral("recovery"),
                         QStringLiteral("handoff_already_finished"));
        return false;
    }

    // Never end a primary that already resolved this request, and never end
    // anything when the result cannot be verified.
    SingleInstanceRequest query;
    query.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    query.command = SingleInstanceCommand::ApplicationQuery;
    query.targetRequestId = m_requestId;
    SingleInstanceHandoffResponse queryResponse;
    QString queryError;
    if (!sendControlCommand(m_serverName, query, queryResponse, queryError) ||
        queryResponse.state == HandoffRequestState::Taken ||
        queryResponse.state == HandoffRequestState::Rejected ||
        queryResponse.state == HandoffRequestState::Cancelled ||
        queryResponse.state == HandoffRequestState::Unknown) {
        appendDiagnostic(QStringLiteral("recovery"),
                         QStringLiteral("handoff_resolved_or_unverifiable"));
        return false;
    }

    // Re-read the lock owner after confirmation. A different PID, image path
    // or creation time means the target was replaced while we were asking.
    ProcessIdentity confirmedTarget;
    if (!captureLockOwner(m_dataDirectory, confirmedTarget, error) ||
        !sameIdentity(target, confirmedTarget)) {
        appendDiagnostic(QStringLiteral("recovery"), QStringLiteral("target_changed"));
        QMessageBox::information(
            nullptr, QObject::tr("Recovery cancelled"),
            QObject::tr("The running instance changed while you were confirming. No process "
                        "was ended. Please check the running instances manually."));
        return false;
    }

    if (!terminateVerifiedPrimary(confirmedTarget, error)) {
        appendDiagnostic(QStringLiteral("recovery"), error);
        QMessageBox::information(nullptr, QObject::tr("Manual recovery required"),
                                 QObject::tr("The previous instance could not be ended safely "
                                             "(%1). Please close it manually.")
                                     .arg(error));
        return false;
    }
    appendDiagnostic(QStringLiteral("recovery"), QStringLiteral("primary_terminated"));
    m_restartAfterRecovery = true;
    return true;
#endif
}
