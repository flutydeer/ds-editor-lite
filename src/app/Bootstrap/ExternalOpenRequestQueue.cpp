#include "ExternalOpenRequestQueue.h"
#include "UI/Window/DocumentWindowManager.h"

#include <QTimer>

namespace {
    constexpr qsizetype maxQueuedProjectPaths = 64;
}

ExternalOpenRequestQueue::ExternalOpenRequestQueue(DocumentWindowManager &windows)
    : m_windows(windows) {
}

AdmissionResult ExternalOpenRequestQueue::take(const SingleInstanceRequest &request) {
    if (m_windows.terminating())
        return AdmissionResult::rejected(QStringLiteral("primary_shutting_down"));
    const auto paths = request.command == SingleInstanceCommand::OpenProjects ||
                               request.command == SingleInstanceCommand::ApplicationHandoff
                           ? request.paths
                           : QStringList{};
    if (m_queuedPaths + paths.size() > maxQueuedProjectPaths)
        return AdmissionResult::rejected(QStringLiteral("request_queue_full"));

    m_queuedPaths += paths.size();
    QTimer::singleShot(0, this, [this, paths] {
        m_queuedPaths -= paths.size();
        if (m_windows.terminating())
            return;
        if (!paths.isEmpty())
            m_windows.open(paths);
        else
            m_windows.activateApplication();
    });
    // Admission transfers the request to the primary. Loading and activation are asynchronous.
    return AdmissionResult::accepted();
}
