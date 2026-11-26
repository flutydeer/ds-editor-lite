#include "DocumentWindowManager.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Bootstrap/WindowPlacement.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowPathUtils.h"
#include "MainWindow.h"
#include "DetachedPanelWindow.h"
#include "Model/AppOptions/AppOptions.h"
#include "Controller/PlaybackController.h"
#include <lite/History/HistoryManager.h>
#include <lite/Tasking/TaskManager.h>
#include <lite/Tasking/Task.h>
#include <lite/GUI/Utils/WindowDialogs.h>

#include <QApplication>
#include <QEvent>
#include <QScopedValueRollback>
#include <QScopeGuard>

namespace {
    DocumentWindowManager *manager = nullptr;
}

DocumentWindowManager::Document::~Document() {
    const SingletonRegistry::Scope scope(context->services());
    placement.reset();
    window.reset();
}

DocumentWindowManager::DocumentWindowManager(AppContext &process) : m_process(process) {
    Q_ASSERT(!manager);
    manager = this;
    qApp->installEventFilter(this);
}

DocumentWindowManager::~DocumentWindowManager() {
    qApp->removeEventFilter(this);
    m_documents.clear();
    manager = nullptr;
}

DocumentWindowManager *DocumentWindowManager::instance() { return manager; }

DocumentWindowManager::Document &DocumentWindowManager::createWindow() {
    auto entry = std::make_unique<Document>();
    if (!m_documents.empty())
        entry->ownedContext = std::make_unique<AppContext>(m_process);
    entry->context = entry->ownedContext ? entry->ownedContext.get() : &m_process;
    const SingletonRegistry::Scope scope(entry->context->services());
    documentWorkflowController->setWindowNumber(m_nextWindowNumber++);
    entry->window = std::make_unique<MainWindow>();
    entry->placement = std::make_unique<WindowPlacement>(*entry->window);
    entry->placement->restoreOrPlace(appOptions->window()->mainWindowGeometry());
    auto &document = *entry;
    m_documents.push_back(std::move(entry));
    if (!m_active)
        m_active = &document;
    connect(documentWorkflowController, &DocumentWorkflowController::documentIdentityChanged, timer, schedule);
    connect(documentWorkflowController, &DocumentWorkflowController::busyChanged,
            document.window.get(), [&document](bool busy) {
                if (!busy)
                    document.openingPath.clear();
            });
    activate(document);
    return document;
}

DocumentWindowManager::Document *DocumentWindowManager::find(const Automation::DocumentId &id) const {
    for (const auto &entry : m_documents)
        if (!entry->closing && entry->context->m_coreRuntime->documentVersion().documentId == id)
            return entry.get();
    return nullptr;
}

DocumentWindowManager::Document *DocumentWindowManager::find(const AppContext *context) const {
    for (const auto &entry : m_documents)
        if (!entry->closing && entry->context == context)
            return entry.get();
    return nullptr;
}

DocumentWindowManager::Document *DocumentWindowManager::findPath(const QString &path) const {
    using namespace DocumentWorkflowPathUtils;
    if (path.isEmpty())
        return nullptr;
    const auto normalized = normalizedProjectPath(path);
    for (const auto &entry : m_documents) {
        if (entry->closing)
            continue;
        const auto &saved = entry->context->m_coreRuntime->documentPath();
        if ((!saved.isEmpty() && projectPathsEqual(normalized, normalizedProjectPath(saved))) ||
            projectPathsEqual(normalized, entry->openingPath))
            return entry.get();
    }
    return nullptr;
}

DocumentWindowManager::Document *DocumentWindowManager::active() const {
    return m_active;
}

void DocumentWindowManager::activate(Document &document) const {
    auto &window = *document.window;
    if (window.isMinimized())
        window.setWindowState(window.windowState() & ~Qt::WindowMinimized);
    window.show();
    window.raise();
    window.activateWindow();
}

bool DocumentWindowManager::eventFilter(QObject *watched, QEvent *event) {
    if (!m_terminating && event->type() == QEvent::WindowActivate) {
        for (const auto &entry : m_documents) {
            if (!entry->closing &&
                (watched == entry->window.get() || watched == entry->window->detachedPanelWindow())) {
                m_active = entry.get();
                break;
            }
        }
    }
    return false;
}

void DocumentWindowManager::open(const QString &path, bool reuseBlank, Document *replace) {
    if (m_terminating || path.isEmpty())
        return;
    if (auto *existing = findPath(path)) {
        activate(*existing);
        return;
    }
    if (!replace && reuseBlank) {
        for (const auto &entry : m_documents) {
            const auto &runtime = *entry->context->m_coreRuntime;
            if (!entry->closing && entry->openingPath.isEmpty() && runtime.documentPath().isEmpty() &&
                !runtime.documentBusy(runtime.documentVersion().documentId) &&
                entry->context->m_historyManager->isOnSavePoint()) {
                replace = entry.get();
                break;
            }
        }
    }
    auto &document = replace ? *replace : createWindow();
    const SingletonRegistry::Scope scope(document.context->services());
    if (documentWorkflowController->busy())
        return;
    document.openingPath = DocumentWorkflowPathUtils::normalizedProjectPath(path);
    documentWorkflowController->requestOpen(document.openingPath);
}

bool DocumentWindowManager::confirmClose(Document &document, TerminationSavePolicy policy,
                                         const QString &progress) {
    const SingletonRegistry::Scope scope(document.context->services());
    auto &runtime = *document.context->m_coreRuntime;
    auto version = runtime.documentVersion();
    if (document.closing || runtime.documentBusy(version.documentId))
        return false;
    if (document.context->m_historyManager->isOnSavePoint() || policy == TerminationSavePolicy::Discard)
        return true;
    if (policy == TerminationSavePolicy::RejectUnsaved)
        return false;
    auto *workflow = documentWorkflowController;
    workflow->setCloseDecisionPending(true);
    const auto release = qScopeGuard([workflow] { workflow->setCloseDecisionPending(false); });
    switch (document.window->askCloseDecision(progress, m_terminating)) {
    case SaveDecision::Cancel:
        return false;
    case SaveDecision::Discard:
        return runtime.documentVersion() == version;
    case SaveDecision::Save:
        break;
    }
    QString path = runtime.documentPath();
    if (path.isEmpty())
        path = document.window->chooseDocumentSavePath(DocumentWorkflowPathUtils::suggestedSavePath(
            {}, documentWorkflowController->lastProjectFolder(), documentWorkflowController->projectName()));
    if (path.isEmpty())
        return false;
    const auto result = runtime.documents().saveDocument(
        {.expected = version, .source = Automation::InvocationSource::TrustedGui}, path);
    if (!result)
        document.window->showDocumentWorkflowError({tr("Failed to save project"), result.getError().message});
    return bool(result);
}

void DocumentWindowManager::retire(Document &document) {
    const SingletonRegistry::Scope scope(document.context->services());
    document.closing = true;
    if (m_active == &document)
        m_active = nullptr;
    document.window->dockPanel();
    document.window->hide();
    document.context->initializeDefaultDocument();
    QTimer::singleShot(0, this, [this, entry = &document] { releaseRetired(entry); });
}

void DocumentWindowManager::releaseRetired(Document *document) {
    const auto &tasks = TaskManager::instance()->tasks();
    const auto hasWorkers = std::any_of(tasks.cbegin(), tasks.cend(), [document](const Task *task) {
        return ObjectContext::of(task) == document->context->services() && !task->stopped();
    });
    if (hasWorkers) {
        QTimer::singleShot(50, this, [this, document] { releaseRetired(document); });
        return;
    }
    m_documents.remove_if([document](const auto &entry) { return entry.get() == document; });
}

bool DocumentWindowManager::close(Document &document, TerminationSavePolicy policy) {
    if (m_terminating || !confirmClose(document, policy))
        return false;
    const SingletonRegistry::Scope scope(document.context->services());
    const auto others = std::any_of(m_documents.begin(), m_documents.end(), [&document](const auto &entry) {
        return entry.get() != &document && !entry->closing;
    });
    if (others) {
        retire(document);
    } else {
        document.window->dockPanel();
        documentWorkflowController->initializeNewDocument();
    }
    return true;
}

TerminationRequestResult DocumentWindowManager::terminate(TerminationMode mode, TerminationSavePolicy policy) {
    if (m_terminating)
        return TerminationRequestResult::Busy;
    QList<Document *> remaining;
    int dirty = 0;
    for (const auto &entry : m_documents) {
        if (entry->closing)
            continue;
        auto &runtime = *entry->context->m_coreRuntime;
        if (runtime.documentBusy(runtime.documentVersion().documentId))
            return TerminationRequestResult::Busy;
        if (!entry->context->m_historyManager->isOnSavePoint())
            ++dirty;
        remaining.append(entry.get());
    }
    if (dirty && policy == TerminationSavePolicy::RejectUnsaved)
        return TerminationRequestResult::UnsavedChanges;
    if (remaining.isEmpty())
        return TerminationRequestResult::Accepted;
    m_terminating = true;
    auto *geometrySource = m_active ? m_active : remaining.front();
    QByteArray mainGeometry, detachedGeometry;
    int index = 0;
    for (auto *entry : remaining) {
        QString progress;
        if (!entry->context->m_historyManager->isOnSavePoint())
            progress = QStringLiteral("%1/%2").arg(++index).arg(dirty);
        if (!confirmClose(*entry, policy, progress)) {
            m_terminating = false;
            m_active = entry;
            activate(*entry);
            return TerminationRequestResult::Busy;
        }
        if (entry == geometrySource) {
            mainGeometry = entry->placement->saveGeometry();
            detachedGeometry = entry->window->detachedGeometry();
        }
        if (entry != remaining.back())
            retire(*entry);
    }
    auto *last = remaining.back();
    const SingletonRegistry::Scope scope(last->context->services());
    m_process.commitShutdown();
    last->context->m_coreRuntime->commitShutdown();
    auto *options = m_process.m_appOptions;
    options->window()->setMainWindowGeometry(mainGeometry);
    if (!detachedGeometry.isEmpty())
        options->window()->setDetachedPanelGeometry(detachedGeometry);
    options->saveAndNotify(AppOptionsGlobal::Window);
    last->window->finishApplicationExit(mode);
    return TerminationRequestResult::Accepted;
}
