#include "SingleInstanceCoordinator.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QThread>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {
    constexpr int connectionTimeoutMs = 3000;
    constexpr int connectionAttemptMs = 100;

    QString defaultDataDirectory() {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }

    QString defaultServerName(const QString &dataDirectory) {
        const auto identity = QFileInfo(dataDirectory).absoluteFilePath().toUtf8();
        const auto suffix =
            QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(16);
        return QStringLiteral("OpenVPI.DsEditorLite.v1-%1").arg(QString::fromLatin1(suffix));
    }
}

class SingleInstanceIpcWorker final : public QObject {
public:
    explicit SingleInstanceIpcWorker(SingleInstanceCoordinator *coordinator)
        : m_coordinator(coordinator) {
    }

    bool start(const QString &serverName, QString &error) {
        m_server = new QLocalServer(this);
        m_server->setSocketOptions(QLocalServer::UserAccessOption);
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

    void stop() {
        if (m_server)
            m_server->close();
    }

private:
    void acceptConnections() {
        while (m_server->hasPendingConnections()) {
            auto *socket = m_server->nextPendingConnection();
            connect(socket, &QLocalSocket::readyRead, socket, [this, socket] {
                if (socket->property("requestHandled").toBool())
                    return;
                auto &buffer = m_buffers[socket];
                buffer.append(socket->readAll());
                QByteArray payload;
                QString frameError;
                if (!SingleInstanceProtocol::takeFrame(buffer, payload, frameError)) {
                    if (!frameError.isEmpty())
                        sendResponse(socket,
                                     {{}, false, frameError, QCoreApplication::applicationPid()});
                    return;
                }

                socket->setProperty("requestHandled", true);
                SingleInstanceRequest request;
                QString error;
                const bool accepted =
                    SingleInstanceProtocol::decodeRequest(payload, request, error);
                sendResponse(socket, {request.requestId, accepted, error,
                                      QCoreApplication::applicationPid()});
                if (accepted) {
                    QMetaObject::invokeMethod(
                        m_coordinator,
                        [coordinator = m_coordinator, request] {
                            coordinator->receiveRequest(request);
                        },
                        Qt::QueuedConnection);
                }
            });
            connect(socket, &QLocalSocket::disconnected, socket, [this, socket] {
                m_buffers.remove(socket);
                socket->deleteLater();
            });
        }
    }

    static void sendResponse(QLocalSocket *socket, const SingleInstanceResponse &response) {
        const auto message =
            SingleInstanceProtocol::frame(SingleInstanceProtocol::encodeResponse(response));
        socket->setProperty("responseBytesRemaining", message.size());
        QObject::connect(socket, &QLocalSocket::bytesWritten, socket, [socket](const qint64 bytes) {
            const auto remaining = socket->property("responseBytesRemaining").toLongLong() - bytes;
            socket->setProperty("responseBytesRemaining", remaining);
            if (remaining <= 0)
                socket->disconnectFromServer();
        });
        socket->write(message);
        socket->flush();
    }

    SingleInstanceCoordinator *m_coordinator;
    QLocalServer *m_server = nullptr;
    QHash<QLocalSocket *, QByteArray> m_buffers;
};

SingleInstanceCoordinator::SingleInstanceCoordinator(QString dataDirectory, QString serverName,
                                                     QObject *parent)
    : QObject(parent),
      m_dataDirectory(dataDirectory.isEmpty() ? defaultDataDirectory() : std::move(dataDirectory)),
      m_serverName(serverName.isEmpty() ? defaultServerName(m_dataDirectory)
                                        : std::move(serverName)) {
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

    m_lockFile =
        std::make_unique<QLockFile>(directory.filePath(QStringLiteral("instance-v1.lock")));
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
    QMetaObject::invokeMethod(
        m_worker, [this, &started] { started = m_worker->start(m_serverName, m_error); },
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

void SingleInstanceCoordinator::stopAcceptingRequests() {
    m_requestHandler = {};
    m_pendingRequests.clear();
    if (m_worker && m_ipcThread) {
        QMetaObject::invokeMethod(
            m_worker, [this] { m_worker->stop(); }, Qt::BlockingQueuedConnection);
    }
}

void SingleInstanceCoordinator::shutdown() {
    stopAcceptingRequests();
    if (m_worker && m_ipcThread) {
        auto *mainThread = QCoreApplication::instance()->thread();
        QMetaObject::invokeMethod(
            m_worker, [this, mainThread] { m_worker->moveToThread(mainThread); },
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
