#include "DocumentManager.h"

#include "Bootstrap/WindowPlacement.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "DocumentSession.h"
#include "Model/AppOptions/AppOptions.h"
#include "UI/Window/MainWindow.h"

#include <QApplication>
#include <QTimer>

#include <algorithm>

struct DocumentManager::Entry {
    ~Entry() {
        if (session)
            session->activate();
        placement.reset();
        window.reset();
        session.reset();
    }

    std::unique_ptr<DocumentSession> session;
    std::unique_ptr<MainWindow> window;
    std::unique_ptr<WindowPlacement> placement;
};

DocumentManager::DocumentManager(QObject *parent) : QObject(parent), m_playbackArbiter(this) {
}

DocumentManager::~DocumentManager() {
    if (m_lastActiveWindow)
        rememberGeometry(m_lastActiveWindow);
    saveRememberedGeometry();
    m_entries.clear();
}

void DocumentManager::handleRequest(const SingleInstanceRequest &request) {
    if (m_shuttingDown)
        return;

    if (request.command == SingleInstanceCommand::OpenProjects) {
        for (const auto &path : request.paths)
            openProject(path);
    }
    if (m_entries.empty())
        createUntitledWindow();
    else if (request.command == SingleInstanceCommand::Activate)
        activateWindow(m_lastActiveWindow ? m_lastActiveWindow.data()
                                          : m_entries.front()->window.get());
}

MainWindow *DocumentManager::createUntitledWindow() {
    return createWindow();
}

MainWindow *DocumentManager::openProject(const QString &path) {
    if (path.isEmpty())
        return nullptr;
    const auto owner = ownerForPath(path);
    if (owner) {
        if (auto *entry = entryForOwner(owner)) {
            activateWindow(entry->window.get());
            return entry->window.get();
        }
    }
    return createWindow(path);
}

int DocumentManager::windowCount() const {
    return static_cast<int>(m_entries.size());
}

MainWindow *DocumentManager::createWindow(const QString &reservedPath) {
    auto entry = std::make_unique<Entry>();
    if (!reservedPath.isEmpty() && !m_registry.reserve(entry.get(), reservedPath)) {
        if (const auto owner = m_registry.ownerForPath(reservedPath)) {
            if (auto *existing = entryForOwner(owner)) {
                activateWindow(existing->window.get());
                return existing->window.get();
            }
        }
        return nullptr;
    }

    entry->session = std::make_unique<DocumentSession>();
    entry->window = std::make_unique<MainWindow>(entry->session.get());
    auto *window = entry->window.get();
    auto *session = entry->session.get();
    auto *workflow = session->workflow();
    entry->placement = std::make_unique<WindowPlacement>(*window);

    connect(window, &MainWindow::newWindowRequested, this, [this] { createUntitledWindow(); });
    connect(window, &MainWindow::openDocumentRequested, this,
            [this, window](const QString &path, const OpenDocumentMode mode) {
                if (mode == OpenDocumentMode::CurrentWindow)
                    openProjectInWindow(window, path);
                else
                    openProject(path);
            });
    connect(window, &MainWindow::applicationCloseRequested, this,
            &DocumentManager::requestApplicationClose);
    connect(window, &MainWindow::documentCloseRequested, this,
            [this, window] { requestDocumentClose(window); });
    connect(window, &MainWindow::documentClosed, this, [this, window] {
        QTimer::singleShot(0, this, [this, window] { removeWindow(window); });
    });
    connect(window, &MainWindow::windowActivated, this,
            [this, window] { m_lastActiveWindow = window; });
    window->setProjectPathValidator([this, rawEntry = entry.get()](const QString &path) {
        const auto owner = ownerForPath(path);
        if (!owner || owner == rawEntry)
            return m_pendingSaveRegistry.update(rawEntry, path);
        if (auto *existing = entryForOwner(owner))
            QTimer::singleShot(0, this,
                               [this, existing] { activateWindow(existing->window.get()); });
        return false;
    });
    connect(workflow, &DocumentWorkflowController::documentIdentityChanged, this,
            [this, rawEntry = entry.get()] { reconcileIdentity(rawEntry); });
    connect(workflow, &DocumentWorkflowController::terminationApproved, this,
            [this, window](TerminationMode) { approveWindowClose(window); });
    connect(workflow, &DocumentWorkflowController::busyChanged, this,
            [this, window, rawEntry = entry.get()](const bool busy) {
                if (busy)
                    return;
                reconcileIdentity(rawEntry);
                m_pendingOpenRegistry.release(rawEntry);
                m_pendingSaveRegistry.release(rawEntry);
                if (m_pendingCloseWindow != window || !m_closeApprovalPending)
                    return;
                if (m_waitingForWorkflowIdle) {
                    m_waitingForWorkflowIdle = false;
                    QTimer::singleShot(0, this, [this, window] {
                        if (m_pendingCloseWindow != window || !m_closeApprovalPending)
                            return;
                        if (auto *entry = entryForWindow(window)) {
                            entry->session->activate();
                            entry->session->workflow()->requestTermination(
                                m_shuttingDown && m_restartRequested ? TerminationMode::Restart
                                                                     : TerminationMode::Exit);
                        }
                    });
                    return;
                }
                QTimer::singleShot(0, this, [this, window] {
                    if (m_pendingCloseWindow == window && m_closeApprovalPending) {
                        m_shuttingDown = false;
                        m_restartRequested = false;
                        m_pendingCloseWindow.clear();
                        m_shutdownApprovedWindows.clear();
                        m_closeApprovalPending = false;
                        m_waitingForWorkflowIdle = false;
                    }
                });
            });

    m_playbackArbiter.addSession(session);
    const bool firstWindow = !m_createdFirstWindow;
    m_createdFirstWindow = true;
    if (firstWindow) {
        entry->placement->restoreOrPlace(appOptions->window()->mainWindowGeometry());
    } else if (m_lastActiveWindow) {
        entry->placement->placeCascaded(m_lastActiveWindow->geometry());
    } else {
        entry->placement->restoreOrPlace({});
    }

    m_entries.push_back(std::move(entry));
    m_lastActiveWindow = window;
    window->show();
#if defined(WITH_DIRECT_MANIPULATION)
    window->registerDirectManipulation();
#endif
    if (!reservedPath.isEmpty())
        QTimer::singleShot(0, workflow,
                           [workflow, reservedPath] { workflow->requestOpen(reservedPath); });
    return window;
}

MainWindow *DocumentManager::openProjectInWindow(MainWindow *window, const QString &path) {
    auto *entry = entryForWindow(window);
    if (!entry || path.isEmpty())
        return nullptr;

    if (const auto owner = ownerForPath(path)) {
        if (auto *existing = entryForOwner(owner)) {
            activateWindow(existing->window.get());
            return existing->window.get();
        }
    }

    auto *workflow = entry->session->workflow();
    if (workflow->busy() || !m_pendingOpenRegistry.reserve(entry, path))
        return nullptr;

    entry->session->activate();
    workflow->requestOpen(path);
    return window;
}

DocumentManager::Entry *DocumentManager::entryForWindow(const MainWindow *window) const {
    const auto it = std::find_if(m_entries.begin(), m_entries.end(), [window](const auto &entry) {
        return entry->window.get() == window;
    });
    return it == m_entries.end() ? nullptr : it->get();
}

DocumentManager::Entry *DocumentManager::entryForOwner(const void *owner) const {
    const auto it = std::find_if(m_entries.begin(), m_entries.end(),
                                 [owner](const auto &entry) { return entry.get() == owner; });
    return it == m_entries.end() ? nullptr : it->get();
}

const void *DocumentManager::ownerForPath(const QString &path) const {
    if (auto owner = m_registry.ownerForPath(path))
        return owner;
    if (auto owner = m_pendingOpenRegistry.ownerForPath(path))
        return owner;
    return m_pendingSaveRegistry.ownerForPath(path);
}

void DocumentManager::activateWindow(MainWindow *window) {
    if (!window)
        return;
    if (window->isMinimized())
        window->setWindowState(window->windowState() & ~Qt::WindowMinimized);
    window->show();
    window->raise();
    window->activateWindow();
    m_lastActiveWindow = window;
}

void DocumentManager::reconcileIdentity(Entry *entry) {
    if (!entryForOwner(entry))
        return;
    const auto path = entry->session->workflow()->projectPath();
    if (path.isEmpty()) {
        if (!entry->session->workflow()->busy())
            m_registry.release(entry);
        return;
    }
    if (!m_registry.update(entry, path)) {
        if (const auto owner = m_registry.ownerForPath(path)) {
            if (auto *existing = entryForOwner(owner))
                activateWindow(existing->window.get());
        }
    }
}

void DocumentManager::requestDocumentClose(MainWindow *window) {
    auto *entry = entryForWindow(window);
    if (!entry || m_closeApprovalPending)
        return;
    m_pendingCloseWindow = window;
    m_closeApprovalPending = true;
    entry->session->activate();
    if (entry->session->workflow()->busy()) {
        m_waitingForWorkflowIdle = true;
        return;
    }
    m_waitingForWorkflowIdle = false;
    entry->session->workflow()->requestTermination(
        m_shuttingDown && m_restartRequested ? TerminationMode::Restart : TerminationMode::Exit);
}

void DocumentManager::requestApplicationClose(const bool restart) {
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;
    m_restartRequested = restart;
    m_shutdownApprovedWindows.clear();
    if (m_lastActiveWindow)
        rememberGeometry(m_lastActiveWindow);
    closeNextWindow();
}

void DocumentManager::closeNextWindow() {
    if (!m_shuttingDown || m_closeApprovalPending)
        return;
    if (m_entries.empty()) {
        saveRememberedGeometry();
        if (m_restartRequested)
            qApp->setProperty("restart", true);
        qApp->quit();
        return;
    }
    MainWindow *window = nullptr;
    if (m_lastActiveWindow && !m_shutdownApprovedWindows.contains(m_lastActiveWindow))
        window = m_lastActiveWindow;
    if (!window) {
        const auto it =
            std::find_if(m_entries.rbegin(), m_entries.rend(), [this](const auto &entry) {
                return !m_shutdownApprovedWindows.contains(entry->window.get());
            });
        if (it != m_entries.rend())
            window = (*it)->window.get();
    }
    if (window) {
        window->close();
        return;
    }

    // Every document has passed its save guard. Close them only after the
    // aggregate decision succeeds, so canceling one window keeps all windows.
    m_entries.back()->window->completeDocumentClose();
}

void DocumentManager::approveWindowClose(MainWindow *window) {
    if (!entryForWindow(window))
        return;
    m_closeApprovalPending = false;
    m_waitingForWorkflowIdle = false;
    m_pendingCloseWindow.clear();
    if (m_shuttingDown) {
        m_shutdownApprovedWindows.insert(window);
        QTimer::singleShot(0, this, [this] { closeNextWindow(); });
        return;
    }
    if (!m_shuttingDown && m_lastActiveWindow == window)
        rememberGeometry(window);
    QTimer::singleShot(0, window, [window] { window->completeDocumentClose(); });
}

void DocumentManager::removeWindow(MainWindow *window) {
    const auto it = std::find_if(m_entries.begin(), m_entries.end(), [window](const auto &entry) {
        return entry->window.get() == window;
    });
    if (it == m_entries.end())
        return;

    auto *entry = it->get();
    m_registry.release(entry);
    m_pendingOpenRegistry.release(entry);
    m_pendingSaveRegistry.release(entry);
    m_playbackArbiter.removeSession(entry->session.get());
    m_shutdownApprovedWindows.remove(window);
    if (m_lastActiveWindow == window)
        m_lastActiveWindow.clear();
    auto ownedEntry = std::move(*it);
    m_entries.erase(it);
    ownedEntry->placement.reset();
    ownedEntry->session->activate();
    ownedEntry->window.reset();
    ownedEntry->session.reset();

    if (!m_entries.empty() && !m_lastActiveWindow)
        m_lastActiveWindow = m_entries.back()->window.get();
    if (m_lastActiveWindow) {
        if (auto *activeEntry = entryForWindow(m_lastActiveWindow))
            activeEntry->session->activate();
    }
    if (m_shuttingDown)
        closeNextWindow();
    else if (m_entries.empty()) {
        saveRememberedGeometry();
        qApp->quit();
    }
}

void DocumentManager::rememberGeometry(MainWindow *window) {
    if (auto *entry = entryForWindow(window))
        m_lastActiveGeometry = entry->placement->saveGeometry();
}

void DocumentManager::saveRememberedGeometry() {
    if (m_lastActiveGeometry.isEmpty())
        return;
    appOptions->window()->setMainWindowGeometry(m_lastActiveGeometry);
    if (!appOptions->saveAndNotify(AppOptionsGlobal::Window))
        qWarning("Failed to save main-window placement");
    m_lastActiveGeometry.clear();
}
