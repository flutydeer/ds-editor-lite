#include "HeadlessOpenRequestQueue.h"

#include "Automation/CoreRuntime.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowPathUtils.h"

#include <QEventLoop>
#include <QPointer>
#include <QTimer>

#include <utility>

HeadlessOpenRequestQueue::HeadlessOpenRequestQueue(Automation::CoreRuntime &runtime,
                                                   OpenDocument openDocument, QObject *parent)
    : QObject(parent), m_runtime(runtime), m_openDocument(std::move(openDocument)) {
}

void HeadlessOpenRequestQueue::enqueue(const SingleInstanceRequest &request) {
    if (request.command == SingleInstanceCommand::OpenProjects) {
        for (const auto &path : request.paths)
            m_paths.enqueue(DocumentWorkflowPathUtils::normalizedProjectPath(path));
    }
    QTimer::singleShot(0, this, [this] { dispatchNext(); });
}

void HeadlessOpenRequestQueue::waitUntilIdle() {
    if (!m_busy && m_paths.isEmpty())
        return;

    QEventLoop loop;
    m_idleCallback = [&loop] { loop.quit(); };
    loop.exec();
    m_idleCallback = {};
}

void HeadlessOpenRequestQueue::dispatchNext() {
    if (m_busy || !m_openDocument)
        return;

    while (!m_paths.isEmpty()) {
        const auto path = m_paths.dequeue();
        const auto current =
            m_runtime.documents().getDocument(m_runtime.documentVersion().documentId);
        if (current && !current.get().path.isEmpty() &&
            DocumentWorkflowPathUtils::projectPathsEqual(path, current.get().path)) {
            continue;
        }

        Automation::PublicDocumentOpenRequest request;
        request.command = {
            .expected = m_runtime.documentVersion(),
            .source = Automation::InvocationSource::InternalAutomation,
        };
        request.canonicalPath = path;
        request.unsavedPolicy = Automation::PublicUnsavedPolicy::Reject;
        const auto accepted = m_openDocument(request);
        if (!accepted) {
            qWarning() << "Headless external project open was rejected:" << path
                       << accepted.getError().message;
            continue;
        }
        if (accepted.get().validatedOnly || accepted.get().taskId.isNull())
            continue;

        m_busy = true;
        const QPointer<HeadlessOpenRequestQueue> guard(this);
        if (!m_runtime.automationTasks().setTerminalCallback(
                accepted.get().taskId, [guard](const Automation::AutomationTaskSnapshot &snapshot) {
                    if (!guard)
                        return;
                    QMetaObject::invokeMethod(
                        guard,
                        [guard, snapshot] {
                            if (guard)
                                guard->taskFinished(snapshot);
                        },
                        Qt::QueuedConnection);
                })) {
            m_busy = false;
            qWarning() << "Headless external project task disappeared before observation:"
                       << accepted.get().taskId.toString();
            continue;
        }
        return;
    }
    completeIdleWait();
}

void HeadlessOpenRequestQueue::taskFinished(const Automation::AutomationTaskSnapshot &snapshot) {
    m_busy = false;
    if (snapshot.state != Automation::AutomationTaskState::Succeeded) {
        qWarning() << "Headless external project open did not succeed:"
                   << snapshot.taskId.toString()
                   << (snapshot.error ? snapshot.error->message : QString());
    }
    QTimer::singleShot(0, this, [this] { dispatchNext(); });
}

void HeadlessOpenRequestQueue::completeIdleWait() {
    auto callback = std::exchange(m_idleCallback, {});
    if (callback)
        callback();
}
