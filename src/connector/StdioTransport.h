#ifndef DSCONNECTOR_STDIOTRANSPORT_H
#define DSCONNECTOR_STDIOTRANSPORT_H

#include <QByteArray>
#include <QObject>

#include <memory>

class QThread;

namespace DsConnector {

    struct StdioTransportState;

    class StdioTransport final : public QObject {
        Q_OBJECT

    public:
        explicit StdioTransport(QObject *parent = nullptr);
        ~StdioTransport() override;

        bool start(QString *error = nullptr);

    public slots:
        void writeLine(const QByteArray &line);
        void beginPendingResponse();
        void endPendingResponse();

    signals:
        void lineReceived(const QByteArray &line);
        void inputClosed();
        void transportError(const QString &error);

    private:
        void drainInput();
        void writerDrained();
        void writerFailed(const QString &error);
        void closeOutputIfReady();
        void fail(const QString &error);
        void stopThreads();

        std::shared_ptr<StdioTransportState> m_state;
        QThread *m_reader = nullptr;
        QThread *m_writer = nullptr;
        QString m_inputTerminalError;
        qsizetype m_pendingResponses = 0;
        bool m_started = false;
        bool m_inputTerminal = false;
        bool m_terminalDelivered = false;
    };

}

#endif // DSCONNECTOR_STDIOTRANSPORT_H
