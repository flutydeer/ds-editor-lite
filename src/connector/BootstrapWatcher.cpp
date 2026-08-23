#include "BootstrapWatcher.h"

#include <Bootstrap/SingleInstanceIdentity.h>

#include <QLocalSocket>
#include <QRandomGenerator>
#include <QTimer>
#include <QUuid>

#include <algorithm>

namespace DsConnector {

    BootstrapWatcher::BootstrapWatcher(QString connectorInstanceId, QString connectorVersion,
                                       QString serviceName, QObject *parent)
        : QObject(parent), m_connectorInstanceId(std::move(connectorInstanceId)),
          m_connectorVersion(std::move(connectorVersion)),
          m_serviceName(serviceName.isEmpty() ? SingleInstanceIdentity::serviceName()
                                              : std::move(serviceName)),
          m_socket(new QLocalSocket(this)), m_reconnectTimer(new QTimer(this)),
          m_responseTimer(new QTimer(this)) {
        m_reconnectTimer->setSingleShot(true);
        m_responseTimer->setSingleShot(true);
        connect(m_reconnectTimer, &QTimer::timeout, this, &BootstrapWatcher::connectNow);
        connect(m_responseTimer, &QTimer::timeout, this, [this] {
            publishError(QStringLiteral("bootstrap_timeout"));
            m_socket->abort();
        });
        connect(m_socket, &QLocalSocket::connected, this, &BootstrapWatcher::sendWatchRequest);
        connect(m_socket, &QLocalSocket::readyRead, this, &BootstrapWatcher::readFrames);
        connect(m_socket, &QLocalSocket::disconnected, this, &BootstrapWatcher::handleDisconnected);
        connect(m_socket, &QLocalSocket::errorOccurred, this,
                [this](const QLocalSocket::LocalSocketError error) {
                    if (error == QLocalSocket::ServerNotFoundError ||
                        error == QLocalSocket::ConnectionRefusedError) {
                        publishError(QStringLiteral("editor_not_running"));
                    } else {
                        publishError(QStringLiteral("bootstrap_connection_error: %1")
                                         .arg(m_socket->errorString()));
                    }
                });
    }

    BootstrapWatcher::~BootstrapWatcher() {
        stop();
    }

    void BootstrapWatcher::start() {
        if (m_running)
            return;
        m_running = true;
        m_reconnectAttempt = 0;
        connectNow();
    }

    void BootstrapWatcher::reconnect() {
        if (!m_running)
            m_running = true;
        m_reconnectTimer->stop();
        m_responseTimer->stop();
        m_reconnectAttempt = 0;
        m_buffer.clear();
        m_observation = {};
        m_socket->abort();
        connectNow();
        emit observationChanged();
    }

    void BootstrapWatcher::stop() {
        m_running = false;
        m_reconnectTimer->stop();
        m_responseTimer->stop();
        m_socket->abort();
        m_observation.connected = false;
    }

    const BootstrapObservation &BootstrapWatcher::observation() const {
        return m_observation;
    }

    void BootstrapWatcher::connectNow() {
        if (!m_running || m_socket->state() != QLocalSocket::UnconnectedState)
            return;
        m_buffer.clear();
        m_socket->connectToServer(m_serviceName, QIODevice::ReadWrite);
        m_responseTimer->start(2000);
    }

    void BootstrapWatcher::sendWatchRequest() {
        m_requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_watchEstablished = false;
        SingleInstanceRequest request;
        request.requestId = m_requestId;
        request.command = SingleInstanceCommand::AutomationWatch;
        request.connector.instanceId = m_connectorInstanceId;
        request.connector.version = m_connectorVersion;
        const auto message =
            SingleInstanceProtocol::frame(SingleInstanceProtocol::encodeRequest(request));
        if (message.isEmpty() || m_socket->write(message) != message.size()) {
            publishError(QStringLiteral("bootstrap_write_failed"));
            m_socket->abort();
            return;
        }
        m_responseTimer->start(2000);
        m_observation.connected = true;
        m_observation.protocolSupported = true;
        m_observation.error.clear();
        emit observationChanged();
    }

    void BootstrapWatcher::readFrames() {
        m_buffer.append(m_socket->readAll());
        while (true) {
            QByteArray payload;
            QString frameError;
            if (!SingleInstanceProtocol::takeFrame(m_buffer, payload, frameError)) {
                if (!frameError.isEmpty()) {
                    publishError(QStringLiteral("bootstrap_protocol_error: %1").arg(frameError),
                                 false);
                    m_socket->abort();
                }
                return;
            }

            SingleInstanceAutomationSnapshot snapshot;
            QString decodeError;
            if (SingleInstanceProtocol::decodeAutomationSnapshot(payload, snapshot, decodeError)) {
                const auto validRequestId = m_watchEstablished ? snapshot.requestId.isEmpty()
                                                               : snapshot.requestId == m_requestId;
                if (!validRequestId) {
                    publishError(QStringLiteral("bootstrap_request_id_mismatch"), false);
                    m_socket->abort();
                    return;
                }
                m_observation.connected = true;
                m_observation.protocolSupported = true;
                m_observation.error.clear();
                m_reconnectAttempt = 0;
                m_responseTimer->stop();
                m_observation.snapshotSequence = ++m_snapshotSequence;
                m_observation.snapshot = std::move(snapshot);
                m_watchEstablished = true;
                emit observationChanged();
                continue;
            }

            SingleInstanceResponse response;
            QString responseError;
            if (SingleInstanceProtocol::decodeResponse(payload, response, responseError) &&
                response.requestId == m_requestId && !response.accepted) {
                publishError(response.error.isEmpty()
                                 ? QStringLiteral("bootstrap_capability_missing")
                                 : response.error,
                             false);
            } else {
                publishError(QStringLiteral("bootstrap_protocol_error: %1").arg(decodeError),
                             false);
            }
            m_socket->abort();
            return;
        }
    }

    void BootstrapWatcher::handleDisconnected() {
        m_responseTimer->stop();
        if (m_observation.connected) {
            m_observation.connected = false;
            emit observationChanged();
        }
        scheduleReconnect();
    }

    void BootstrapWatcher::scheduleReconnect() {
        if (!m_running || m_reconnectTimer->isActive())
            return;
        const auto exponent = std::min(m_reconnectAttempt++, 6);
        const auto baseDelay = std::min(10000, 250 * (1 << exponent));
        const auto jitter = QRandomGenerator::global()->bounded(std::max(1, baseDelay / 4));
        m_reconnectTimer->start(baseDelay + jitter);
    }

    void BootstrapWatcher::publishError(const QString &error, const bool protocolSupported) {
        const auto changed = m_observation.error != error ||
                             m_observation.protocolSupported != protocolSupported;
        m_observation.error = error;
        m_observation.protocolSupported = protocolSupported;
        if (changed)
            emit observationChanged();
        scheduleReconnect();
    }

}
