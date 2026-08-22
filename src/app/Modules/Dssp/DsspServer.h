#ifndef DSSP_SERVER_H
#define DSSP_SERVER_H

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>
#include <thread>

namespace httplib {
    class Server;
}

class DsspServer final : public QObject {
    Q_OBJECT

public:
    explicit DsspServer(QObject *parent = nullptr);
    ~DsspServer() override;

    Q_DISABLE_COPY_MOVE(DsspServer)

    /// Start the HTTP service. Blocks until the socket is bound (or failed).
    bool start(const QString &host, int port, QString *errorMessage = nullptr);
    /// Stop the service and wait for the listener and all in-flight handlers
    /// to finish. Safe to call from any thread other than the listener.
    void stop();
    bool isRunning() const;

private:
    void registerRoutes();

    std::unique_ptr<httplib::Server> m_server;
    std::thread m_listenThread;
    std::atomic<bool> m_running{false};
};

#endif // DSSP_SERVER_H
