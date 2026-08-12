#ifndef DOCUMENTMANAGER_H
#define DOCUMENTMANAGER_H

#include "Bootstrap/OpenDocumentRegistry.h"
#include "Bootstrap/PlaybackArbiter.h"
#include "Bootstrap/SingleInstanceProtocol.h"

#include <QObject>
#include <QPointer>
#include <QSet>

#include <memory>
#include <vector>

class DocumentSession;
class MainWindow;
class WindowPlacement;

class DocumentManager final : public QObject {
public:
    explicit DocumentManager(QObject *parent = nullptr);
    ~DocumentManager() override;

    Q_DISABLE_COPY_MOVE(DocumentManager)

    void handleRequest(const SingleInstanceRequest &request);
    MainWindow *createUntitledWindow();
    MainWindow *openProject(const QString &path);

    [[nodiscard]] int windowCount() const;

private:
    struct Entry;

    MainWindow *createWindow(const QString &reservedPath = {});
    Entry *entryForWindow(const MainWindow *window) const;
    Entry *entryForOwner(const void *owner) const;
    void activateWindow(MainWindow *window);
    void reconcileIdentity(Entry *entry);
    void requestDocumentClose(MainWindow *window);
    void requestApplicationClose(bool restart);
    void closeNextWindow();
    void approveWindowClose(MainWindow *window);
    void removeWindow(MainWindow *window);
    void rememberGeometry(MainWindow *window);
    void saveRememberedGeometry();

    std::vector<std::unique_ptr<Entry>> m_entries;
    OpenDocumentRegistry m_registry;
    OpenDocumentRegistry m_pendingSaveRegistry;
    PlaybackArbiter m_playbackArbiter;
    QPointer<MainWindow> m_lastActiveWindow;
    QPointer<MainWindow> m_pendingCloseWindow;
    QSet<MainWindow *> m_shutdownApprovedWindows;
    QByteArray m_lastActiveGeometry;
    bool m_createdFirstWindow = false;
    bool m_shuttingDown = false;
    bool m_restartRequested = false;
    bool m_closeApprovalPending = false;
    bool m_waitingForWorkflowIdle = false;
};

#endif // DOCUMENTMANAGER_H
