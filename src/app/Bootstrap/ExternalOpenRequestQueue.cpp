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

void ExternalOpenRequestQueue::enqueue(const SingleInstanceRequest &request) {
    if (m_workflow->shutdownCommitted())
        return;
    m_activatePending = true;
    if (request.command == SingleInstanceCommand::OpenProjects) {
        for (const auto &path : request.paths)
            m_paths.enqueue(DocumentWorkflowPathUtils::normalizedProjectPath(path));
    }
    QTimer::singleShot(0, this, [this] { dispatchNext(); });
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
