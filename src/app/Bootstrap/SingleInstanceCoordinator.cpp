#include "SingleInstanceCoordinator.h"

#include "SingleInstanceIdentity.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QPointer>
#include <QQueue>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QUuid>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {
    constexpr int connectionTimeoutMs = 3000;
    constexpr int connectionAttemptMs = 100;
    constexpr int heartbeatIntervalMs = 1000;

    QString executablePath() {
        const QFileInfo executable(QCoreApplication::applicationFilePath());
        const auto canonical = executable.canonicalFilePath();
        return canonical.isEmpty() ? executable.absoluteFilePath() : canonical;
    }

    SingleInstanceAutomationStatus initialAutomationStatus(const AppHostMode hostMode) {
        return {
            SingleInstanceAutomationState::EditorStarting,
            QUuid::createUuid().toString(QUuid::WithoutBraces),
            executablePath(),
            QCoreApplication::applicationVersion(),
            {},
            appHostModeName(hostMode),
            false,
            {},
            {},
        };
    }
}

class SingleInstanceIpcWorker final : public QObject {
public:
    explicit SingleInstanceIpcWorker(SingleInstanceCoordinator *coordinator)
        : m_coordinator(coordinator) {
        m_clock.start();
    }

    bool start(const QString &serverName, const SingleInstanceAutomationStatus &status,
               const QString &instanceId, const QString &hostMode, QString &error) {
        m_automationStatus = status;
        m_instanceId = instanceId;
        m_hostMode = hostMode;
        m_server = new QLocalServer(this);
        m_server->setSocketOptions(QLocalServer::UserAccessOption);
        m_server->setMaxPendingConnections(
            static_cast<int>(SingleInstanceProtocol::maxConnectionCount));
        // On Windows, listen() can deliver pending connections before it returns.
        connect(m_server, &QLocalServer::newConnection, this, [this] { acceptConnections(); });
        if (!m_server->listen(serverName)) {
            QLocalServer::removeServer(serverName);
            if (!m_server->listen(serverName)) {
                error = m_server->errorString();
                return false;
            }
        }
        return true;
    }

    void updateAutomationState(const SingleInstanceAutomationStatus &status, const bool broadcast) {
        m_automationStatus = status;
        if (broadcast)
            broadcastAutomationState();
    }

    void broadcastAutomationState() {
        const auto watchers = m_watchers.values();
        for (auto *socket : watchers)
            sendAutomationSnapshot(socket, {}, false);
    }

    void setPrimaryStage(const PrimaryStartupStage stage) {
        m_stage = stage;
    }

    void noteHeartbeat() {
        m_lastHeartbeatMs = m_clock.elapsed();
    }

    void setAdmissionOpen() {
        if (m_admissionOpen)
            return;
        m_admissionOpen = true;
        dispatchAllEligible();
    }

    void stop(const SingleInstanceAutomationStatus &status) {
        if (m_stopping)
            return;
        m_stopping = true;
        m_stage = PrimaryStartupStage::Stopping;
        m_automationStatus = status;
        if (m_server)
            m_server->close();

        // Requests that never reached the main thread must not be left hanging:
        // tell their secondaries that the primary is shutting down.
        const auto pendingIds = m_ledger.keys();
        for (const auto &id : pendingIds) {
            auto it = m_ledger.find(id);
            if (it == m_ledger.end() || it->terminal || it->dispatchCommitted)
                continue;
            it->state = HandoffRequestState::Rejected;
            it->error = QStringLiteral("primary_stopping");
            finalize(*it);
        }

        const auto sockets = m_connections.keys();
        const auto finalMessage =
            SingleInstanceProtocol::frame(SingleInstanceProtocol::encodeAutomationSnapshot(
                {{}, QCoreApplication::applicationPid(), m_automationStatus}));
        for (auto *socket : sockets) {
            if (m_watchers.contains(socket) && !finalMessage.isEmpty() &&
                socket->state() == QLocalSocket::ConnectedState) {
                socket->write(finalMessage);
                socket->flush();
                if (socket->bytesToWrite() > 0)
                    socket->waitForBytesWritten(100);
                socket->disconnectFromServer();
            } else {
                socket->abort();
            }
        }
        m_watchers.clear();
    }

    void pauseRequestDispatch() {
        m_requestDispatchPaused = true;
    }

    void resumeRequestDispatch() {
        if (!m_requestDispatchPaused)
            return;
        m_requestDispatchPaused = false;
        dispatchAllEligible();
    }

    // Called on the IPC thread after the main thread has admitted (or refused)
    // a request. Nothing else may transition an entry to a terminal state.
    void finishRequest(const QString &requestId, const AdmissionResult &result) {
        auto it = m_ledger.find(requestId);
        if (it == m_ledger.end() || it->terminal)
            return;
        if (it->state == HandoffRequestState::Cancelled) {
            // A cancel won the race before the main thread reported back. Keep
            // the cancellation instead of claiming a successful takeover.
            finalize(*it);
            return;
        }
        it->state = result.taken ? HandoffRequestState::Taken : HandoffRequestState::Rejected;
        it->error = result.taken ? QString() : result.error;
        finalize(*it);
    }

private:
    struct ConnectionState {
        QByteArray readBuffer;
        QQueue<QByteArray> writeQueue;
        qsizetype pendingWriteBytes = 0;
        qint64 activeWriteBytes = 0;
        QTimer *initialReadTimer = nullptr;
        bool initialized = false;
        bool closeWhenDrained = false;
    };

    struct LedgerEntry {
        QString requestId;
        QByteArray digest;
        HandoffRequestState state = HandoffRequestState::Received;
        QString error;
        QPointer<QLocalSocket> socket;
        bool legacy = false;
        bool dispatchCommitted = false;
        bool terminal = false;
        SingleInstanceRequest request;
        qint64 createdMs = 0;
        qint64 finishedMs = 0;
    };

    void acceptConnections() {
        while (m_server->hasPendingConnections()) {
            auto *socket = m_server->nextPendingConnection();
            if (m_connections.size() >= SingleInstanceProtocol::maxConnectionCount) {
                socket->disconnectFromServer();
                socket->deleteLater();
                continue;
            }

            auto *initialReadTimer = new QTimer(socket);
            initialReadTimer->setSingleShot(true);
            initialReadTimer->setInterval(SingleInstanceProtocol::initialReadTimeoutMs);
            ConnectionState state;
            state.initialReadTimer = initialReadTimer;
            m_connections.insert(socket, std::move(state));
            connect(socket, &QLocalSocket::readyRead, socket,
                    [this, socket] { readRequests(socket); });
            connect(socket, &QLocalSocket::bytesWritten, socket,
                    [this, socket](const qint64 bytes) { accountBytesWritten(socket, bytes); });
            connect(socket, &QLocalSocket::disconnected, socket, [this, socket] {
                m_watchers.remove(socket);
                dropUnclaimedForSocket(socket);
                m_connections.remove(socket);
                socket->deleteLater();
            });
            connect(initialReadTimer, &QTimer::timeout, socket, [this, socket] {
                const auto it = m_connections.constFind(socket);
                if (it != m_connections.constEnd() && !it->initialized)
                    dropConnection(socket);
            });
            initialReadTimer->start();
            // A pending connection can already contain the initial frame.
            readRequests(socket);
        }
    }

    void readRequests(QLocalSocket *socket) {
        auto it = m_connections.find(socket);
        if (it == m_connections.end())
            return;
        it->readBuffer.append(socket->readAll());

        while (true) {
            QByteArray payload;
            QString frameError;
            if (!SingleInstanceProtocol::takeFrame(it->readBuffer, payload, frameError)) {
                if (!frameError.isEmpty())
                    reject(socket, {}, frameError);
                return;
            }
            if (it->initialized) {
                reject(socket, {}, QStringLiteral("Bootstrap connection is already initialized"));
                return;
            }
            it->initialized = true;
            if (it->initialReadTimer)
                it->initialReadTimer->stop();
            handleRequest(socket, payload);
            it = m_connections.find(socket);
            if (it == m_connections.end() || it->readBuffer.isEmpty())
                return;
        }
    }

    void handleRequest(QLocalSocket *socket, const QByteArray &payload) {
        SingleInstanceRequest request;
        QString error;
        if (!SingleInstanceProtocol::decodeRequest(payload, request, error)) {
            reject(socket, request.requestId, error);
            return;
        }

        switch (request.command) {
            case SingleInstanceCommand::Activate:
            case SingleInstanceCommand::OpenProjects:
                beginApplicationRequest(socket, request, true);
                return;
            case SingleInstanceCommand::ApplicationHandoff:
                beginApplicationRequest(socket, request, false);
                return;
            case SingleInstanceCommand::ApplicationQuery:
                handleQuery(socket, request);
                return;
            case SingleInstanceCommand::ApplicationCancel:
                handleCancel(socket, request);
                return;
            case SingleInstanceCommand::AutomationDiscover:
                sendAutomationSnapshot(socket, request.requestId, true);
                return;
            case SingleInstanceCommand::AutomationWatch:
                if (m_watchers.size() >= SingleInstanceProtocol::maxWatcherCount) {
                    reject(socket, request.requestId, QStringLiteral("too_many_requests"));
                    return;
                }
                m_watchers.insert(socket);
                sendAutomationSnapshot(socket, request.requestId, false);
                return;
        }
    }

    void beginApplicationRequest(QLocalSocket *socket, const SingleInstanceRequest &request,
                                 const bool legacy) {
        const auto digest = SingleInstanceProtocol::requestDigest(request);
        auto existing = m_ledger.find(request.requestId);
        if (existing != m_ledger.end()) {
            if (existing->digest != digest) {
                rejectHandoff(socket, request.requestId,
                              QStringLiteral("duplicate_request_id_with_different_payload"), legacy);
                return;
            }
            // Idempotent replay: return the state we already own, never enqueue twice.
            existing->socket = socket;
            sendCurrentState(*existing, existing->terminal);
            return;
        }
        if (m_stopping || m_stage == PrimaryStartupStage::Stopping) {
            rejectHandoff(socket, request.requestId, QStringLiteral("primary_stopping"), legacy);
            return;
        }
        if (inFlightCount() >= SingleInstanceProtocol::maxInFlightRequests) {
            rejectHandoff(socket, request.requestId,
                          QStringLiteral("too_many_in_flight_requests"), legacy);
            return;
        }

        LedgerEntry entry;
        entry.requestId = request.requestId;
        entry.digest = digest;
        entry.socket = socket;
        entry.legacy = legacy;
        entry.request = request;
        entry.createdMs = m_clock.elapsed();
        m_ledger.insert(entry.requestId, entry);
        auto &stored = m_ledger[entry.requestId];

        // A Received frame only proves the IPC worker is alive. It is never a
        // success terminal state and never lets a secondary exit successfully.
        if (!legacy)
            sendHandoffState(stored.socket.data(), makeResponse(stored), false);
        maybeDispatch(stored);
    }

    void maybeDispatch(LedgerEntry &entry) {
        if (!m_admissionOpen || m_requestDispatchPaused || m_stopping || entry.terminal ||
            entry.dispatchCommitted)
            return;
        entry.dispatchCommitted = true;
        entry.state = HandoffRequestState::Pending;
        const auto requestId = entry.requestId;
        const auto request = entry.request;
        QMetaObject::invokeMethod(
            m_coordinator,
            [coordinator = m_coordinator, requestId, request] {
                coordinator->admitRequest(requestId, request);
            },
            Qt::QueuedConnection);
    }

    void dispatchAllEligible() {
        // Copy the ids: dispatch itself only mutates entry fields, but keeping a
        // stable iteration set makes the transition order deterministic.
        const auto ids = m_ledger.keys();
        for (const auto &id : ids) {
            auto it = m_ledger.find(id);
            if (it != m_ledger.end())
                maybeDispatch(*it);
        }
    }

    void finalize(LedgerEntry &entry) {
        if (entry.terminal)
            return;
        entry.terminal = true;
        entry.finishedMs = m_clock.elapsed();
        sendCurrentState(entry.socket, entry, true);
        m_terminalOrder.enqueue(entry.requestId);
        pruneTerminalRecords();
    }

    void pruneTerminalRecords() {
        const auto now = m_clock.elapsed();
        while (!m_terminalOrder.isEmpty()) {
            // Copy by value: head() returns a reference into the queue, and
            // dequeue() invalidates it. Using that reference afterwards is UB.
            const auto oldest = m_terminalOrder.head();
            const auto it = m_ledger.constFind(oldest);
            const bool expired =
                it != m_ledger.constEnd() && it->terminal &&
                now - it->finishedMs > SingleInstanceProtocol::terminalRecordLifetimeMs;
            if (m_terminalOrder.size() <= SingleInstanceProtocol::maxTerminalRecords && !expired)
                break;
            m_terminalOrder.dequeue();
            auto victim = m_ledger.find(oldest);
            // In-flight entries are never evicted: losing their ownership would
            // allow a duplicate submission to enqueue the same request twice.
            if (victim != m_ledger.end() && victim->terminal)
                m_ledger.erase(victim);
        }
    }

    qsizetype inFlightCount() const {
        qsizetype count = 0;
        for (const auto &entry : m_ledger) {
            if (!entry.terminal)
                ++count;
        }
        return count;
    }

    void dropUnclaimedForSocket(QLocalSocket *socket) {
        const auto ids = m_ledger.keys();
        for (const auto &id : ids) {
            auto it = m_ledger.find(id);
            if (it == m_ledger.end() || it->socket != socket)
                continue;
            it->socket = nullptr;
            if (it->terminal || it->dispatchCommitted)
                continue;
            it->state = HandoffRequestState::Cancelled;
            it->error = QStringLiteral("secondary_disconnected");
            finalize(*it);
        }
    }

    void handleQuery(QLocalSocket *socket, const SingleInstanceRequest &request) {
        auto response = makeResponse(request.requestId);
        const auto it = m_ledger.constFind(request.targetRequestId);
        if (it == m_ledger.constEnd()) {
            response.state = HandoffRequestState::Unknown;
            response.error = QStringLiteral("result_unknown");
        } else {
            response.state = it->state;
            response.error = it->error;
        }
        enqueue(socket,
                SingleInstanceProtocol::frame(
                    SingleInstanceProtocol::encodeHandoffResponse(response)),
                true);
    }

    void handleCancel(QLocalSocket *socket, const SingleInstanceRequest &request) {
        auto response = makeResponse(request.requestId);
        auto it = m_ledger.find(request.targetRequestId);
        if (it == m_ledger.end()) {
            response.state = HandoffRequestState::Unknown;
            response.error = QStringLiteral("result_unknown");
        } else if (it->terminal && it->state == HandoffRequestState::Taken) {
            response.state = HandoffRequestState::Taken;
            response.error = QStringLiteral("request_already_taken");
        } else if (it->dispatchCommitted) {
            response.state = it->state;
            response.error = QStringLiteral("request_already_dispatched");
        } else if (!it->terminal) {
            it->state = HandoffRequestState::Cancelled;
            it->error = QStringLiteral("cancelled_by_secondary");
            finalize(*it);
            response.state = HandoffRequestState::Cancelled;
        } else {
            response.state = it->state;
            response.error = it->error;
        }
        enqueue(socket,
                SingleInstanceProtocol::frame(
                    SingleInstanceProtocol::encodeHandoffResponse(response)),
                true);
    }

    SingleInstanceHandoffResponse makeResponse(const QString &requestId) const {
        SingleInstanceHandoffResponse response;
        response.requestId = requestId;
        response.instanceId = m_instanceId;
        response.primaryProcessId = QCoreApplication::applicationPid();
        response.hostMode = m_hostMode;
        response.stage = m_stage;
        response.heartbeatAgeMs =
            m_lastHeartbeatMs < 0 ? -1 : m_clock.elapsed() - m_lastHeartbeatMs;
        response.handoffVersion = SingleInstanceProtocol::handoffVersion;
        return response;
    }

    SingleInstanceHandoffResponse makeResponse(const LedgerEntry &entry) const {
        auto response = makeResponse(entry.requestId);
        response.state = entry.state;
        response.error = entry.error;
        return response;
    }

    void sendCurrentState(const LedgerEntry &entry, const bool closeWhenDrained) {
        sendCurrentState(entry.socket, entry, closeWhenDrained);
    }

    void sendCurrentState(const QPointer<QLocalSocket> &socket, const LedgerEntry &entry,
                          const bool closeWhenDrained) {
        auto *rawSocket = socket.data();
        if (!rawSocket)
            return;
        if (entry.legacy) {
            const SingleInstanceResponse response{
                entry.requestId,
                entry.terminal && entry.state == HandoffRequestState::Taken,
                entry.state == HandoffRequestState::Taken ? QString() : entry.error,
                QCoreApplication::applicationPid(),
            };
            sendResponse(rawSocket, response, closeWhenDrained);
            return;
        }
        sendHandoffState(rawSocket, makeResponse(entry), closeWhenDrained);
    }

    void sendHandoffState(QLocalSocket *socket,
                          const SingleInstanceHandoffResponse &response,
                          const bool closeWhenDrained) {
        if (!socket)
            return;
        enqueue(socket,
                SingleInstanceProtocol::frame(
                    SingleInstanceProtocol::encodeHandoffResponse(response)),
                closeWhenDrained);
    }

    void rejectHandoff(QLocalSocket *socket, const QString &requestId, const QString &error,
                       const bool legacy) {
        if (legacy) {
            reject(socket, requestId, error);
            return;
        }
        auto response = makeResponse(requestId);
        response.state = HandoffRequestState::Rejected;
        response.error = error;
        sendHandoffState(socket, response, true);
    }

    void reject(QLocalSocket *socket, const QString &requestId, const QString &error) {
        sendResponse(socket,
                     {requestId.isEmpty() ? QStringLiteral("invalid-request") : requestId, false,
                      error, QCoreApplication::applicationPid()},
                     true);
    }

    void sendResponse(QLocalSocket *socket, const SingleInstanceResponse &response,
                      const bool closeWhenDrained) {
        enqueue(socket,
                SingleInstanceProtocol::frame(SingleInstanceProtocol::encodeResponse(response)),
                closeWhenDrained);
    }

    void sendAutomationSnapshot(QLocalSocket *socket, const QString &requestId,
                                const bool closeWhenDrained) {
        const SingleInstanceAutomationSnapshot snapshot{
            requestId,
            QCoreApplication::applicationPid(),
            m_automationStatus,
        };
        enqueue(socket,
                SingleInstanceProtocol::frame(
                    SingleInstanceProtocol::encodeAutomationSnapshot(snapshot)),
                closeWhenDrained);
    }

    void enqueue(QLocalSocket *socket, QByteArray message, const bool closeWhenDrained) {
        auto it = m_connections.find(socket);
        if (it == m_connections.end() || message.isEmpty()) {
            dropConnection(socket);
            return;
        }
        const auto pendingFrameCount = it->writeQueue.size() + (it->activeWriteBytes > 0 ? 1 : 0);
        if (pendingFrameCount >= SingleInstanceProtocol::maxPendingWriteFrames ||
            it->pendingWriteBytes + message.size() > SingleInstanceProtocol::maxPendingWriteBytes) {
            dropConnection(socket);
            return;
        }

        it->pendingWriteBytes += message.size();
        it->writeQueue.enqueue(std::move(message));
        it->closeWhenDrained = it->closeWhenDrained || closeWhenDrained;
        pumpWrites(socket);
    }

    void pumpWrites(QLocalSocket *socket) {
        auto it = m_connections.find(socket);
        if (it == m_connections.end() || it->activeWriteBytes > 0)
            return;
        if (it->writeQueue.isEmpty()) {
            if (it->closeWhenDrained)
                socket->disconnectFromServer();
            return;
        }

        const auto message = it->writeQueue.dequeue();
        it->activeWriteBytes = message.size();
        if (socket->write(message) != message.size()) {
            dropConnection(socket);
            return;
        }
        socket->flush();
    }

    void accountBytesWritten(QLocalSocket *socket, const qint64 bytes) {
        auto it = m_connections.find(socket);
        if (it == m_connections.end() || m_stopping)
            return;
        const auto accounted = qMin(bytes, it->activeWriteBytes);
        it->activeWriteBytes -= accounted;
        it->pendingWriteBytes -= accounted;
        if (it->activeWriteBytes == 0)
            pumpWrites(socket);
    }

    void dropConnection(QLocalSocket *socket) {
        m_watchers.remove(socket);
        dropUnclaimedForSocket(socket);
        if (socket)
            socket->abort();
    }

    SingleInstanceCoordinator *m_coordinator;
    QLocalServer *m_server = nullptr;
    QHash<QLocalSocket *, ConnectionState> m_connections;
    QSet<QLocalSocket *> m_watchers;
    QHash<QString, LedgerEntry> m_ledger;
    QQueue<QString> m_terminalOrder;
    SingleInstanceAutomationStatus m_automationStatus;
    QElapsedTimer m_clock;
    QString m_instanceId;
    QString m_hostMode;
    PrimaryStartupStage m_stage = PrimaryStartupStage::Starting;
    qint64 m_lastHeartbeatMs = -1;
    bool m_admissionOpen = false;
    bool m_requestDispatchPaused = false;
    bool m_stopping = false;
};

SingleInstanceCoordinator::SingleInstanceCoordinator(QString dataDirectory, QString serverName,
                                                     QObject *parent)
    : QObject(parent),
      m_dataDirectory(SingleInstanceIdentity::normalizeDataDirectory(dataDirectory)),
      m_serverName(serverName.isEmpty() ? SingleInstanceIdentity::serviceName(m_dataDirectory)
                                        : std::move(serverName)),
      m_automationStatus(initialAutomationStatus(AppHostMode::Gui)) {
}

SingleInstanceCoordinator::SingleInstanceCoordinator(const AppHostMode hostMode, QObject *parent)
    : QObject(parent), m_dataDirectory(SingleInstanceIdentity::normalizeDataDirectory({})),
      m_serverName(SingleInstanceIdentity::serviceName(m_dataDirectory)),
      m_automationStatus(initialAutomationStatus(hostMode)) {
}

SingleInstanceCoordinator::~SingleInstanceCoordinator() {
    shutdown();
}

SingleInstanceCoordinator::StartResult SingleInstanceCoordinator::start() {
    QDir directory(m_dataDirectory);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        m_error = tr("Failed to create the application data directory: %1").arg(m_dataDirectory);
        return StartResult::Error;
    }

    m_lockFile = std::make_unique<QLockFile>(SingleInstanceIdentity::lockFilePath(m_dataDirectory));
    m_lockFile->setStaleLockTime(0);
    if (!m_lockFile->tryLock(0)) {
        if (m_lockFile->error() == QLockFile::LockFailedError)
            return StartResult::Secondary;
        m_error = tr("Failed to access the single-instance lock file");
        return StartResult::Error;
    }

    m_ipcThread = new QThread;
    m_worker = new SingleInstanceIpcWorker(this);
    m_worker->moveToThread(m_ipcThread);
    m_ipcThread->start();

    bool started = false;
    const auto status = automationState();
    const auto instanceId = status.editorInstanceId;
    const auto hostMode = status.hostMode;
    QMetaObject::invokeMethod(
        m_worker,
        [this, &started, status, instanceId, hostMode] {
            started = m_worker->start(m_serverName, status, instanceId, hostMode, m_error);
        },
        Qt::BlockingQueuedConnection);
    if (!started) {
        shutdown();
        return StartResult::Error;
    }

    // The heartbeat is driven by the main thread. If the main thread stalls in
    // AppContext construction, the IPC worker keeps serving but the heartbeat
    // ages out, which the secondary can report without blocking on a window.
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(heartbeatIntervalMs);
    connect(m_heartbeatTimer, &QTimer::timeout, this,
            [this] { noteMainThreadHeartbeat(); });
    m_heartbeatTimer->start();
    noteMainThreadHeartbeat();
    setPrimaryStage(PrimaryStartupStage::Starting);

    return StartResult::Primary;
}

bool SingleInstanceCoordinator::forwardRequest(const SingleInstanceRequest &request,
                                               QString &error) {
    QElapsedTimer timer;
    timer.start();
    QLocalSocket socket;
    while (timer.elapsed() < connectionTimeoutMs) {
        socket.abort();
        socket.connectToServer(m_serverName, QIODevice::ReadWrite);
        if (socket.waitForConnected(
                qMin(connectionAttemptMs, connectionTimeoutMs - static_cast<int>(timer.elapsed()))))
            break;
        QThread::msleep(50);
    }
    if (socket.state() != QLocalSocket::ConnectedState) {
        error = tr("The running instance did not accept the request within %1 seconds")
                    .arg(connectionTimeoutMs / 1000);
        return false;
    }
    const auto connectedMs = timer.elapsed();

    if (m_lockFile) {
        qint64 primaryProcessId = 0;
        QString hostname;
        QString applicationName;
        if (m_lockFile->getLockInfo(&primaryProcessId, &hostname, &applicationName))
            allowPrimaryToTakeForeground(primaryProcessId);
    }

    const auto message =
        SingleInstanceProtocol::frame(SingleInstanceProtocol::encodeRequest(request));
    // The pipe may finish writing before the wait; the ACK below still confirms delivery.
    const auto written = socket.write(message);
    bool writeCompleted = true;
    if (written == message.size() && socket.bytesToWrite() > 0) {
        const auto waited = socket.waitForBytesWritten(
            qMax(1, connectionTimeoutMs - static_cast<int>(timer.elapsed())));
        writeCompleted = waited || socket.bytesToWrite() == 0;
    }
    if (written != message.size() || !writeCompleted) {
        error = tr("Failed to send the request to the running instance");
        return false;
    }
    const auto writtenMs = timer.elapsed();

    QByteArray buffer;
    QByteArray payload;
    QString frameError;
    while (timer.elapsed() < connectionTimeoutMs) {
        if (!socket.bytesAvailable() &&
            !socket.waitForReadyRead(
                qMax(1, connectionTimeoutMs - static_cast<int>(timer.elapsed()))))
            break;
        buffer.append(socket.readAll());
        if (SingleInstanceProtocol::takeFrame(buffer, payload, frameError))
            break;
        if (!frameError.isEmpty()) {
            error = frameError;
            return false;
        }
    }
    if (payload.isEmpty()) {
        qWarning() << "Single-instance ACK missing: connected_ms=" << connectedMs
                   << "written_ms=" << writtenMs << "elapsed_ms=" << timer.elapsed()
                   << "state=" << socket.state() << "error=" << socket.errorString()
                   << "available=" << socket.bytesAvailable() << "buffered=" << buffer.size();
        error = tr("The running instance did not acknowledge the request. Its main thread may be "
                   "stalled; the request was not handed over.");
        return false;
    }

    SingleInstanceResponse response;
    if (!SingleInstanceProtocol::decodeResponse(payload, response, error))
        return false;
    if (response.requestId != request.requestId) {
        error = tr("The running instance returned a mismatched response");
        return false;
    }
    if (!response.accepted) {
        error = response.error.isEmpty() ? tr("The running instance rejected the request")
                                         : response.error;
        return false;
    }
    return true;
}

void SingleInstanceCoordinator::installHandler(AdmissionHandler handler) {
    m_requestHandler = std::move(handler);
    if (!m_requestHandler)
        return;
    const auto pending = std::move(m_pendingRequests);
    m_pendingRequests.clear();
    for (const auto &request : pending)
        admitRequest(request.requestId, request);
}

void SingleInstanceCoordinator::setAdmissionHandler(AdmissionHandler handler) {
    installHandler(std::move(handler));
}

void SingleInstanceCoordinator::setRequestHandler(
    std::function<void(const SingleInstanceRequest &)> handler) {
    if (!handler) {
        installHandler({});
        return;
    }
    installHandler([handler = std::move(handler)](const SingleInstanceRequest &request) {
        handler(request);
        return AdmissionResult::accepted();
    });
}

void SingleInstanceCoordinator::enableAdmission() {
    if (!m_worker || !m_ipcThread)
        return;
    // BlockingQueuedConnection is intentional: the Headless path opens
    // admission before QCoreApplication::exec(), so there is no main-thread
    // event loop yet to service a plain queued call. The worker never waits on
    // the main thread here, so this cannot deadlock.
    QMetaObject::invokeMethod(
        m_worker,
        [this] {
            m_worker->setPrimaryStage(PrimaryStartupStage::Ready);
            m_worker->setAdmissionOpen();
        },
        Qt::BlockingQueuedConnection);
}

void SingleInstanceCoordinator::setPrimaryStage(const PrimaryStartupStage stage) {
    if (!m_worker || !m_ipcThread)
        return;
    QMetaObject::invokeMethod(
        m_worker, [this, stage] { m_worker->setPrimaryStage(stage); }, Qt::QueuedConnection);
}

void SingleInstanceCoordinator::noteMainThreadHeartbeat() {
    if (!m_worker || !m_ipcThread)
        return;
    QMetaObject::invokeMethod(
        m_worker, [this] { m_worker->noteHeartbeat(); }, Qt::QueuedConnection);
}

void SingleInstanceCoordinator::admitRequest(const QString &requestId,
                                             const SingleInstanceRequest &request) {
    AdmissionResult result;
    if (m_requestHandler) {
        result = m_requestHandler(request);
    } else {
        m_pendingRequests.append(request);
        return;
    }
    if (!m_worker || !m_ipcThread)
        return;
    QMetaObject::invokeMethod(
        m_worker,
        [this, requestId, result] { m_worker->finishRequest(requestId, result); },
        Qt::QueuedConnection);
}

void SingleInstanceCoordinator::pauseRequestDispatchAndFlush() {
    if (!m_worker || !m_ipcThread)
        return;

    Q_ASSERT(QThread::currentThread() == thread());
    QMetaObject::invokeMethod(
        m_worker, [this] { m_worker->pauseRequestDispatch(); }, Qt::BlockingQueuedConnection);
    QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
}

void SingleInstanceCoordinator::resumeRequestDispatch() {
    if (!m_worker || !m_ipcThread)
        return;

    Q_ASSERT(QThread::currentThread() == thread());
    QMetaObject::invokeMethod(
        m_worker, [this] { m_worker->resumeRequestDispatch(); }, Qt::BlockingQueuedConnection);
}

void SingleInstanceCoordinator::updateAutomationState(
    const SingleInstanceAutomationStatus &status) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this, [this, status] { applyAutomationState(status, true); }, Qt::QueuedConnection);
        return;
    }
    applyAutomationState(status, true);
}

void SingleInstanceCoordinator::updateAutomationState(const SingleInstanceAutomationState state,
                                                      const bool serverEnabled,
                                                      QString serverEndpoint, QString error) {
    auto status = automationState();
    status.state = state;
    status.serverEnabled = serverEnabled;
    status.serverEndpoint = std::move(serverEndpoint);
    status.error = std::move(error);
    updateAutomationState(status);
}

void SingleInstanceCoordinator::broadcastAutomationState() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this, [this] { broadcastAutomationState(); }, Qt::QueuedConnection);
        return;
    }
    applyAutomationState(automationState(), true);
}

void SingleInstanceCoordinator::stopAcceptingRequests() {
    m_requestHandler = {};
    m_pendingRequests.clear();
    if (m_heartbeatTimer)
        m_heartbeatTimer->stop();
    if (m_worker && m_ipcThread) {
        auto status = automationState();
        status.state = SingleInstanceAutomationState::EditorStopping;
        status.serverEndpoint.clear();
        status.error.clear();
        {
            const QMutexLocker locker(&m_automationStateMutex);
            m_automationStatus = status;
        }
        QMetaObject::invokeMethod(
            m_worker, [this, status] { m_worker->stop(status); }, Qt::BlockingQueuedConnection);
    }
}

void SingleInstanceCoordinator::shutdown() {
    stopAcceptingRequests();
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
        m_heartbeatTimer->deleteLater();
        m_heartbeatTimer = nullptr;
    }
    if (m_worker && m_ipcThread) {
        auto *coordinatorThread = thread();
        QMetaObject::invokeMethod(
            m_worker, [this, coordinatorThread] { m_worker->moveToThread(coordinatorThread); },
            Qt::BlockingQueuedConnection);
        m_ipcThread->quit();
        m_ipcThread->wait();
        delete m_worker;
        delete m_ipcThread;
        m_worker = nullptr;
        m_ipcThread = nullptr;
    }
    if (m_lockFile) {
        if (m_lockFile->isLocked())
            m_lockFile->unlock();
        m_lockFile.reset();
    }
}

QString SingleInstanceCoordinator::errorString() const {
    return m_error;
}

QString SingleInstanceCoordinator::serverName() const {
    return m_serverName;
}

SingleInstanceAutomationStatus SingleInstanceCoordinator::automationState() const {
    const QMutexLocker locker(&m_automationStateMutex);
    return m_automationStatus;
}

QString SingleInstanceCoordinator::instanceId() const {
    return automationState().editorInstanceId;
}

qint64 SingleInstanceCoordinator::primaryProcessIdFromLock() const {
    if (!m_lockFile)
        return 0;
    qint64 processId = 0;
    QString hostname;
    QString applicationName;
    if (!m_lockFile->getLockInfo(&processId, &hostname, &applicationName))
        return 0;
    return processId;
}

void SingleInstanceCoordinator::allowPrimaryToTakeForeground(const qint64 processId) const {
#ifdef Q_OS_WIN
    if (processId > 0)
        AllowSetForegroundWindow(static_cast<DWORD>(processId));
#else
    Q_UNUSED(processId)
#endif
}

void SingleInstanceCoordinator::applyAutomationState(const SingleInstanceAutomationStatus &status,
                                                     const bool broadcast) {
    {
        const QMutexLocker locker(&m_automationStateMutex);
        m_automationStatus = status;
    }
    if (m_worker && m_ipcThread) {
        QMetaObject::invokeMethod(
            m_worker,
            [this, status, broadcast] { m_worker->updateAutomationState(status, broadcast); },
            Qt::QueuedConnection);
    }
}
