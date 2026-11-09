#include "ExternalOpenRequestQueue.h"

#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowPathUtils.h"

#include <QTimer>
#include <QWidget>

ExternalOpenRequestQueue::ExternalOpenRequestQueue(DocumentWorkflowController *workflow,
                                                   QWidget *window, QObject *parent)
    : QObject(parent), m_workflow(workflow), m_window(window) {
    connect(m_workflow, &DocumentWorkflowController::busyChanged, this, [this](const bool busy) {
        if (!busy)
            QTimer::singleShot(0, this, [this] { dispatchNext(); });
    });
}

namespace {
    constexpr qsizetype maxQueuedProjectPaths = 64;
}

AdmissionResult ExternalOpenRequestQueue::take(const SingleInstanceRequest &request,
                                               const bool localStartup) {
    if (m_workflow->shutdownCommitted())
        return AdmissionResult::rejected(QStringLiteral("primary_shutting_down"));
    const auto incoming = request.command == SingleInstanceCommand::OpenProjects ||
                                  request.command == SingleInstanceCommand::ApplicationHandoff
                              ? request.paths.size()
                              : 0;
    if (!localStartup && incoming > 0 &&
        m_paths.size() + incoming > maxQueuedProjectPaths)
        return AdmissionResult::rejected(QStringLiteral("request_queue_full"));

    m_activatePending = true;
    if (incoming > 0) {
        for (const auto &path : request.paths)
            m_paths.enqueue(DocumentWorkflowPathUtils::normalizedProjectPath(path));
    }
    QTimer::singleShot(0, this, [this] { dispatchNext(); });
    // Takeover means the request is owned by this queue. It does not mean the
    // project has finished loading, nor that the OS granted foreground focus.
    return AdmissionResult::accepted();
}

void ExternalOpenRequestQueue::dispatchNext() {
    if (m_activatePending) {
        m_activatePending = false;
        activateWindow();
    }
    // The single-instance coordinator stops handing out new requests on exit, but a
    // dispatch that is already queued still runs. Opening a project after the host
    // committed the exit would unfreeze the document that is going away, so the
    // queue has to check the same fence instead of relying on the coordinator.
    if (m_workflow->shutdownCommitted()) {
        m_paths.clear();
        return;
    }
    if (m_workflow->busy())
        return;

    while (!m_paths.isEmpty()) {
        const auto path = m_paths.dequeue();
        if (!m_workflow->projectPath().isEmpty() &&
            DocumentWorkflowPathUtils::projectPathsEqual(
                path, DocumentWorkflowPathUtils::normalizedProjectPath(m_workflow->projectPath())))
            continue;
        m_workflow->requestOpen(path);
        return;
    }
}

void ExternalOpenRequestQueue::activateWindow() {
    if (m_window->isMinimized())
        m_window->setWindowState(m_window->windowState() & ~Qt::WindowMinimized);
    m_window->show();
    m_window->raise();
    m_window->activateWindow();
}
