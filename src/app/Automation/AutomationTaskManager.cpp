#include "AutomationTaskManager.h"

#include <algorithm>

namespace Automation {

    AutomationTaskSnapshot AutomationTaskManager::createTask(OperationId operationId,
                                                             DocumentVersion baseDocument,
                                                             std::optional<ObjectRef> target,
                                                             CancelCallback cancelCallback,
                                                             QString createdByClientId) {
        AutomationTaskSnapshot snapshot;
        snapshot.taskId = TaskId::create();
        snapshot.operationId = std::move(operationId);
        snapshot.baseDocument = std::move(baseDocument);
        snapshot.target = std::move(target);
        snapshot.createdByClientId = std::move(createdByClientId);

        const QMutexLocker locker(&m_mutex);
        m_records.insert(snapshot.taskId, {snapshot, std::move(cancelCallback)});
        return snapshot;
    }

    bool AutomationTaskManager::setUnsuccessfulCallback(const TaskId &taskId,
                                                        UnsuccessfulCallback callback) {
        const QMutexLocker locker(&m_mutex);
        auto it = m_records.find(taskId);
        if (it == m_records.end() || isTerminal(it->snapshot.state))
            return false;
        it->unsuccessful = std::move(callback);
        return true;
    }

    AutomationResult<AutomationTaskSnapshot>
        AutomationTaskManager::get(const DocumentId &documentId, const TaskId &taskId) const {
        const QMutexLocker locker(&m_mutex);
        return findLocked(documentId, taskId);
    }

    QList<AutomationTaskSnapshot> AutomationTaskManager::list(const DocumentId &documentId) const {
        const QMutexLocker locker(&m_mutex);
        QList<AutomationTaskSnapshot> result;
        for (const auto &record : m_records) {
            if (record.snapshot.baseDocument.documentId == documentId)
                result.append(record.snapshot);
        }
        std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
            return left.taskId.toString() < right.taskId.toString();
        });
        return result;
    }

    AutomationResult<AutomationTaskSnapshot>
        AutomationTaskManager::requestCancel(const DocumentId &documentId, const TaskId &taskId) {
        CancelCallback cancelCallback;
        AutomationTaskSnapshot snapshot;
        {
            const QMutexLocker locker(&m_mutex);
            auto found = findLocked(documentId, taskId);
            if (!found)
                return found.getError();
            auto it = m_records.find(taskId);
            if (isTerminal(it->snapshot.state) ||
                it->snapshot.state == AutomationTaskState::CancelRequested) {
                return it->snapshot;
            }
            if (it->snapshot.state == AutomationTaskState::Committing)
                return notCancelable(taskId);
            it->snapshot.state = AutomationTaskState::CancelRequested;
            it->snapshot.cancelable = false;
            snapshot = it->snapshot;
            cancelCallback = it->cancel;
        }
        if (cancelCallback)
            cancelCallback();
        return snapshot;
    }

    AutomationResult<bool> AutomationTaskManager::beginCommitting(const TaskId &taskId) {
        UnsuccessfulCallback unsuccessful;
        std::optional<AutomationTaskSnapshot> snapshot;
        {
            const QMutexLocker locker(&m_mutex);
            auto it = m_records.find(taskId);
            if (it == m_records.end())
                return AutomationError::taskNotFound(taskId);
            if (it->snapshot.state == AutomationTaskState::CancelRequested) {
                it->snapshot.state = AutomationTaskState::Canceled;
                it->snapshot.cancelable = false;
                unsuccessful = std::move(it->unsuccessful);
                snapshot = it->snapshot;
            } else {
                if (it->snapshot.state != AutomationTaskState::Queued &&
                    it->snapshot.state != AutomationTaskState::Running) {
                    return notCancelable(taskId);
                }
                it->snapshot.state = AutomationTaskState::Committing;
                it->snapshot.cancelable = false;
                return true;
            }
        }
        if (unsuccessful)
            unsuccessful(*snapshot);
        return false;
    }

    bool AutomationTaskManager::markRunning(const TaskId &taskId) {
        const QMutexLocker locker(&m_mutex);
        auto it = m_records.find(taskId);
        if (it == m_records.end() || it->snapshot.state != AutomationTaskState::Queued)
            return false;
        it->snapshot.state = AutomationTaskState::Running;
        return true;
    }

    bool AutomationTaskManager::updateProgress(const TaskId &taskId,
                                               AutomationTaskProgress progress, QString message) {
        const QMutexLocker locker(&m_mutex);
        auto it = m_records.find(taskId);
        if (it == m_records.end() || isTerminal(it->snapshot.state) ||
            it->snapshot.state == AutomationTaskState::Committing)
            return false;
        it->snapshot.progress = std::move(progress);
        it->snapshot.message = std::move(message);
        return true;
    }

    bool AutomationTaskManager::succeed(const TaskId &taskId, MutationResult mutation) {
        const QMutexLocker locker(&m_mutex);
        auto it = m_records.find(taskId);
        if (it == m_records.end() || it->snapshot.state != AutomationTaskState::Committing)
            return false;
        it->snapshot.state = AutomationTaskState::Succeeded;
        it->snapshot.cancelable = false;
        it->snapshot.mutation = std::move(mutation);
        it->snapshot.error.reset();
        it->unsuccessful = {};
        return true;
    }

    bool AutomationTaskManager::fail(const TaskId &taskId, AutomationError error) {
        UnsuccessfulCallback unsuccessful;
        AutomationTaskSnapshot snapshot;
        {
            const QMutexLocker locker(&m_mutex);
            auto it = m_records.find(taskId);
            if (it == m_records.end() || isTerminal(it->snapshot.state))
                return false;
            it->snapshot.state = AutomationTaskState::Failed;
            it->snapshot.cancelable = false;
            it->snapshot.error = std::move(error);
            unsuccessful = std::move(it->unsuccessful);
            snapshot = it->snapshot;
        }
        if (unsuccessful)
            unsuccessful(snapshot);
        return true;
    }

    bool AutomationTaskManager::cancel(const TaskId &taskId) {
        UnsuccessfulCallback unsuccessful;
        AutomationTaskSnapshot snapshot;
        {
            const QMutexLocker locker(&m_mutex);
            auto it = m_records.find(taskId);
            if (it == m_records.end() || isTerminal(it->snapshot.state) ||
                it->snapshot.state == AutomationTaskState::Committing) {
                return false;
            }
            it->snapshot.state = AutomationTaskState::Canceled;
            it->snapshot.cancelable = false;
            unsuccessful = std::move(it->unsuccessful);
            snapshot = it->snapshot;
        }
        if (unsuccessful)
            unsuccessful(snapshot);
        return true;
    }

    bool AutomationTaskManager::isCancellationRequested(const TaskId &taskId) const {
        const QMutexLocker locker(&m_mutex);
        const auto it = m_records.constFind(taskId);
        return it != m_records.cend() && it->snapshot.state == AutomationTaskState::CancelRequested;
    }

    void AutomationTaskManager::discardDocumentGeneration(const DocumentId &documentId) {
        QList<CancelCallback> callbacks;
        {
            const QMutexLocker locker(&m_mutex);
            for (auto it = m_records.begin(); it != m_records.end();) {
                if (it->snapshot.baseDocument.documentId != documentId) {
                    ++it;
                    continue;
                }
                if (!isTerminal(it->snapshot.state) &&
                    it->snapshot.state != AutomationTaskState::Committing && it->cancel) {
                    callbacks.append(it->cancel);
                }
                it = m_records.erase(it);
            }
        }
        for (const auto &callback : callbacks)
            callback();
    }

    qsizetype AutomationTaskManager::size() const {
        const QMutexLocker locker(&m_mutex);
        return m_records.size();
    }

    bool AutomationTaskManager::isTerminal(const AutomationTaskState state) {
        return state == AutomationTaskState::Succeeded || state == AutomationTaskState::Failed ||
               state == AutomationTaskState::Canceled;
    }

    AutomationError AutomationTaskManager::notCancelable(const TaskId &taskId) {
        AutomationError error;
        error.code = AutomationErrorCode::OperationNotCancelable;
        error.taskId = taskId;
        error.message = QStringLiteral("Automation task can no longer be canceled");
        return error;
    }

    AutomationResult<AutomationTaskSnapshot>
        AutomationTaskManager::findLocked(const DocumentId &documentId,
                                          const TaskId &taskId) const {
        const auto it = m_records.constFind(taskId);
        if (it == m_records.cend())
            return AutomationError::taskNotFound(taskId);
        if (it->snapshot.baseDocument.documentId != documentId)
            return AutomationError::documentChanged(documentId,
                                                    it->snapshot.baseDocument.documentId);
        return it->snapshot;
    }

} // namespace Automation
