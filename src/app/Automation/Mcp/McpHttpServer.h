#ifndef MCPHTTPSERVER_H
#define MCPHTTPSERVER_H

#include <lite/AutomationWire/McpProtocol.h>

#include <QJsonValue>
#include <QObject>
#include <QPointer>

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
        qsizetype maximumLegacySessions = 128;
        int requestDeadlineMs = 30000;
    };

    struct AutomationHttpRoutes {
        bool mcp = false;
        bool native = false;

        friend bool operator==(const AutomationHttpRoutes &,
                               const AutomationHttpRoutes &) = default;
    };

    class McpHttpServer final : public QObject {
        Q_OBJECT

    public:
        using RequestHandler = std::function<QJsonObject(
            const AutomationWire::Mcp::RequestEnvelope &, const QString &clientId)>;
        using NativeRequestHandler =
            std::function<QJsonObject(const QJsonValue &, const QString &clientId)>;

        explicit McpHttpServer(RequestHandler handler, QObject *parent = nullptr);
        McpHttpServer(RequestHandler handler, McpHttpLimits limits, QObject *parent = nullptr);
        McpHttpServer(QObject *handlerContext, RequestHandler handler, QObject *parent = nullptr);
        McpHttpServer(QObject *handlerContext, RequestHandler handler, McpHttpLimits limits,
                      QObject *parent = nullptr);
        McpHttpServer(QObject *handlerContext, RequestHandler handler,
                      NativeRequestHandler nativeHandler, QObject *parent = nullptr);
        McpHttpServer(QObject *handlerContext, RequestHandler handler,
                      NativeRequestHandler nativeHandler, McpHttpLimits limits,
                      QObject *parent = nullptr);
        ~McpHttpServer() override;

        Q_DISABLE_COPY_MOVE(McpHttpServer)

        bool start(quint16 requestedPort, QString &error);
        bool start(quint16 requestedPort, AutomationHttpRoutes routes, QString &error);
        bool setRoutes(AutomationHttpRoutes routes, QString &error);
        void requestStop();
        void stop();

        [[nodiscard]] bool isListening() const;
        [[nodiscard]] bool isStopping() const;
        [[nodiscard]] quint16 port() const;
        [[nodiscard]] QString endpoint() const;
        [[nodiscard]] QString nativeEndpoint() const;
        [[nodiscard]] AutomationHttpRoutes routes() const;

    signals:
        void stopped();

    private:
        class Worker;

        void finalizeAsyncStop();

        RequestHandler m_handler;
        NativeRequestHandler m_nativeHandler;
        McpHttpLimits m_limits;
        QPointer<QObject> m_handlerContext;
        std::shared_ptr<McpHttpTransportState> m_transportState;
        QThread *m_thread = nullptr;
        Worker *m_worker = nullptr;
        quint16 m_port = 0;
        AutomationHttpRoutes m_routes;
        std::atomic_bool m_accepting = false;
        bool m_stopping = false;
    };

} // namespace Automation

#endif // MCPHTTPSERVER_H
