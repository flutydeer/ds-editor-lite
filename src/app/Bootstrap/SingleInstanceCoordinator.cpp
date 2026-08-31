#include "SingleInstanceCoordinator.h"

#include "SingleInstanceIdentity.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
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

    QString executablePath() {
        const QFileInfo executable(QCoreApplication::applicationFilePath());
        const auto canonical = executable.canonicalFilePath();
        return canonical.isEmpty() ? executable.absoluteFilePath() : canonical;
    }

    SingleInstanceAutomationStatus initialAutomationStatus() {
        return {
            SingleInstanceAutomationState::EditorStarting,
            QUuid::createUuid().toString(QUuid::WithoutBraces),
            executablePath(),
            QCoreApplication::applicationVersion(),
            {},
            QStringLiteral("gui"),
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
    }

    bool start(const QString &serverName, const SingleInstanceAutomationStatus &status,
               QString &error) {
        m_automationStatus = status;
        m_server = new QLocalServer(this);
        m_server->setSocketOptions(QLocalServer::UserAccessOption);
        m_server->setMaxPendingConnections(
            static_cast<int>(SingleInstanceProtocol::maxConnectionCount));
        if (!m_server->listen(serverName)) {
            QLocalServer::removeServer(serverName);
            if (!m_server->listen(serverName)) {
                error = m_server->errorString();
                return false;
            }
        }
        connect(m_server, &QLocalServer::newConnection, this, [this] { acceptConnections(); });
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

    void stop(const SingleInstanceAutomationStatus &status) {
        if (m_stopping)
            return;
        m_stopping = true;
        m_automationStatus = status;
        if (m_server)
            m_server->close();

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
                m_connections.remove(socket);
                socket->deleteLater();
            });
            connect(initialReadTimer, &QTimer::timeout, socket, [this, socket] {
                const auto it = m_connections.constFind(socket);
                if (it != m_connections.constEnd() && !it->initialized)
                    dropConnection(socket);
            });
            initialReadTimer->start();
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
                sendResponse(socket,
                             {request.requestId, true, {}, QCoreApplication::applicationPid()},
                             true);
                QMetaObject::invokeMethod(
                    m_coordinator,
                    [coordinator = m_coordinator, request] {
                        coordinator->receiveRequest(request);
                    },
                    Qt::QueuedConnection);
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
        if (socket)
            socket->abort();
    }

    SingleInstanceCoordinator *m_coordinator;
    QLocalServer *m_server = nullptr;
    QHash<QLocalSocket *, ConnectionState> m_connections;
    QSet<QLocalSocket *> m_watchers;
    SingleInstanceAutomationStatus m_automationStatus;
    bool m_stopping = false;
};

SingleInstanceCoordinator::SingleInstanceCoordinator(QString dataDirectory, QString serverName,
                                                     QObject *parent)
    : QObject(parent),
      m_dataDirectory(SingleInstanceIdentity::normalizeDataDirectory(dataDirectory)),
      m_serverName(serverName.isEmpty() ? SingleInstanceIdentity::serviceName(m_dataDirectory)
                                        : std::move(serverName)),
      m_automationStatus(initialAutomationStatus()) {
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
    QMetaObject::invokeMethod(
        m_worker,
        [this, &started, status] { started = m_worker->start(m_serverName, status, m_error); },
        Qt::BlockingQueuedConnection);
    if (!started) {
        shutdown();
        return StartResult::Error;
    }

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

    if (m_lockFile) {
        qint64 primaryProcessId = 0;
        QString hostname;
        QString applicationName;
        if (m_lockFile->getLockInfo(&primaryProcessId, &hostname, &applicationName))
            allowPrimaryToTakeForeground(primaryProcessId);
    }

    const auto message =
        SingleInstanceProtocol::frame(SingleInstanceProtocol::encodeRequest(request));
    if (socket.write(message) != message.size() ||
        !socket.waitForBytesWritten(
            qMax(1, connectionTimeoutMs - static_cast<int>(timer.elapsed())))) {
        error = tr("Failed to send the request to the running instance");
        return false;
    }

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
        error = tr("The running instance did not acknowledge the request");
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

void SingleInstanceCoordinator::setRequestHandler(
    std::function<void(const SingleInstanceRequest &)> handler) {
    m_requestHandler = std::move(handler);
    if (!m_requestHandler)
        return;
    const auto pending = std::move(m_pendingRequests);
    m_pendingRequests.clear();
    for (const auto &request : pending)
        m_requestHandler(request);
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
                                                      const bool serverEnabled, QString serverEndpoint,
                                                      QString error) {
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

void SingleInstanceCoordinator::receiveRequest(const SingleInstanceRequest &request) {
    if (m_requestHandler)
        m_requestHandler(request);
    else
        m_pendingRequests.append(request);
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
