#ifndef DSCONNECTOR_BOOTSTRAPWATCHER_H
#define DSCONNECTOR_BOOTSTRAPWATCHER_H

#include <Bootstrap/SingleInstanceProtocol.h>

#include <QObject>
#include <QString>

#include <optional>

class QLocalSocket;
class QTimer;

namespace DsConnector {

    struct BootstrapObservation {
        bool connected = false;
        bool protocolSupported = true;
        QString error;
        quint64 snapshotSequence = 0;
        std::optional<SingleInstanceAutomationSnapshot> snapshot;
    };

    class BootstrapWatcher final : public QObject {
        Q_OBJECT

    public:
        explicit BootstrapWatcher(QString connectorInstanceId, QString connectorVersion,
                                  QString serviceName = {}, QObject *parent = nullptr);
        ~BootstrapWatcher() override;

        void start();
        void reconnect();
        void stop();
        const BootstrapObservation &observation() const;

    signals:
        void observationChanged();

    private:
        void connectNow();
        void sendWatchRequest();
        void readFrames();
        void handleDisconnected();
        void scheduleReconnect();
        void publishError(const QString &error, bool protocolSupported = true);

        QString m_connectorInstanceId;
        QString m_connectorVersion;
        QString m_serviceName;
        QLocalSocket *m_socket = nullptr;
        QTimer *m_reconnectTimer = nullptr;
        QTimer *m_responseTimer = nullptr;
        QByteArray m_buffer;
        QString m_requestId;
        BootstrapObservation m_observation;
        quint64 m_snapshotSequence = 0;
        int m_reconnectAttempt = 0;
        bool m_running = false;
        bool m_watchEstablished = false;
    };

}

#endif // DSCONNECTOR_BOOTSTRAPWATCHER_H
