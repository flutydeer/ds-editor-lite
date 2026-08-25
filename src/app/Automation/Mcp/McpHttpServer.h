#ifndef MCPHTTPSERVER_H
#define MCPHTTPSERVER_H

#include <lite/AutomationWire/McpProtocol.h>

#include <QObject>

#include <atomic>
#include <functional>
#include <memory>

class QThread;

namespace Automation {

    class McpHttpTransportState;

    struct McpHttpLimits {
        qsizetype maximumRequestBytes = 1024 * 1024;
        qsizetype maximumResponseBytes = 8 * 1024 * 1024;
        int maximumJsonDepth = 64;
        qsizetype maximumJsonNodes = 100000;
        int maximumGlobalInFlight = 32;
        int maximumPeerInFlight = 32;
        int maximumClientInFlight = 32;
        double globalTokenCapacity = 100.0;
        double globalTokensPerSecond = 50.0;
        double peerTokenCapacity = 100.0;
        double peerTokensPerSecond = 50.0;
        double clientTokenCapacity = 64.0;
        double clientTokensPerSecond = 10.0;
        int requestDeadlineMs = 30000;
    };

    class McpHttpServer final : public QObject {
        Q_OBJECT

    public:
        using RequestHandler = std::function<QJsonObject(
            const AutomationWire::Mcp::RequestEnvelope &, const QString &clientId)>;

        explicit McpHttpServer(RequestHandler handler, QObject *parent = nullptr);
        McpHttpServer(RequestHandler handler, McpHttpLimits limits, QObject *parent = nullptr);
        McpHttpServer(QObject *handlerContext, RequestHandler handler,
                      QObject *parent = nullptr);
        McpHttpServer(QObject *handlerContext, RequestHandler handler, McpHttpLimits limits,
                      QObject *parent = nullptr);
        ~McpHttpServer() override;

        Q_DISABLE_COPY_MOVE(McpHttpServer)

        bool start(quint16 requestedPort, QString &error);
        void requestStop();
        void stop();

        [[nodiscard]] bool isListening() const;
        [[nodiscard]] bool isStopping() const;
        [[nodiscard]] quint16 port() const;
        [[nodiscard]] QString endpoint() const;

    signals:
        void stopped();

    private:
        class Worker;

        void finalizeAsyncStop();

        RequestHandler m_handler;
        McpHttpLimits m_limits;
        QObject *m_handlerContext = nullptr;
        std::shared_ptr<McpHttpTransportState> m_transportState;
        QThread *m_thread = nullptr;
        Worker *m_worker = nullptr;
        quint16 m_port = 0;
        std::atomic_bool m_accepting = false;
        bool m_stopping = false;
    };

} // namespace Automation

#endif // MCPHTTPSERVER_H
