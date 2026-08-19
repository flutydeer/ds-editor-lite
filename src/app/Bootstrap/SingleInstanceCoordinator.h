#ifndef SINGLEINSTANCECOORDINATOR_H
#define SINGLEINSTANCECOORDINATOR_H

#include "SingleInstanceProtocol.h"

#include <QObject>

#include <functional>
#include <memory>

class QLockFile;
class QThread;
class SingleInstanceIpcWorker;

class SingleInstanceCoordinator final : public QObject {
    Q_OBJECT
public:
    enum class StartResult { Primary, Secondary, Error };

    explicit SingleInstanceCoordinator(QString dataDirectory = {}, QString serverName = {},
                                       QObject *parent = nullptr);
    ~SingleInstanceCoordinator() override;

    Q_DISABLE_COPY_MOVE(SingleInstanceCoordinator)

    StartResult start();
    bool forwardRequest(const SingleInstanceRequest &request, QString &error);
    void setRequestHandler(std::function<void(const SingleInstanceRequest &)> handler);
    void stopAcceptingRequests();
    void shutdown();

    [[nodiscard]] QString errorString() const;
    [[nodiscard]] QString serverName() const;

private:
    friend class SingleInstanceIpcWorker;

    void receiveRequest(const SingleInstanceRequest &request);
    void allowPrimaryToTakeForeground(qint64 processId) const;

    QString m_dataDirectory;
    QString m_serverName;
    QString m_error;
    std::unique_ptr<QLockFile> m_lockFile;
    QThread *m_ipcThread = nullptr;
    SingleInstanceIpcWorker *m_worker = nullptr;
    std::function<void(const SingleInstanceRequest &)> m_requestHandler;
    QList<SingleInstanceRequest> m_pendingRequests;
};

#endif // SINGLEINSTANCECOORDINATOR_H
