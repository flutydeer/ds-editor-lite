#ifndef SINGLEINSTANCECOORDINATOR_H
#define SINGLEINSTANCECOORDINATOR_H

#include "AppHostMode.h"
#include "SingleInstanceProtocol.h"

#include <QMutex>
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

    explicit SingleInstanceCoordinator(AppHostMode hostMode, QObject *parent = nullptr);
    explicit SingleInstanceCoordinator(QString dataDirectory = {}, QString serverName = {},
                                       QObject *parent = nullptr);
    ~SingleInstanceCoordinator() override;

    Q_DISABLE_COPY_MOVE(SingleInstanceCoordinator)

    StartResult start();
    bool forwardRequest(const SingleInstanceRequest &request, QString &error);
    void setRequestHandler(std::function<void(const SingleInstanceRequest &)> handler);
    void pauseRequestDispatchAndFlush();
    void resumeRequestDispatch();
    void updateAutomationState(const SingleInstanceAutomationStatus &status);
    void updateAutomationState(SingleInstanceAutomationState state, bool serverEnabled,
                               QString serverEndpoint = {}, QString error = {});
    void broadcastAutomationState();
    void stopAcceptingRequests();
    void shutdown();

    [[nodiscard]] QString errorString() const;
    [[nodiscard]] QString serverName() const;
    [[nodiscard]] SingleInstanceAutomationStatus automationState() const;

private:
    friend class SingleInstanceIpcWorker;

    void receiveRequest(const SingleInstanceRequest &request);
    void allowPrimaryToTakeForeground(qint64 processId) const;
    void applyAutomationState(const SingleInstanceAutomationStatus &status, bool broadcast);

    QString m_dataDirectory;
    QString m_serverName;
    QString m_error;
    std::unique_ptr<QLockFile> m_lockFile;
    QThread *m_ipcThread = nullptr;
    SingleInstanceIpcWorker *m_worker = nullptr;
    std::function<void(const SingleInstanceRequest &)> m_requestHandler;
    QList<SingleInstanceRequest> m_pendingRequests;
    mutable QMutex m_automationStateMutex;
    SingleInstanceAutomationStatus m_automationStatus;
};

#endif // SINGLEINSTANCECOORDINATOR_H
