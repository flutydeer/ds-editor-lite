#ifndef AUTOMATIONTASKMANAGER_H
#define AUTOMATIONTASKMANAGER_H

#include "AutomationTypes.h"

#include <QHash>
#include <QMutex>

#include <functional>
#include <optional>

namespace Automation {

    enum class AutomationTaskState {
        Queued,
        Running,
        CancelRequested,
        Committing,
        Succeeded,
        Failed,
        Canceled,
    };

    struct AutomationTaskProgress {
        int minimum = 0;
        int maximum = 100;
        int value = 0;
        bool indeterminate = true;

        friend bool operator==(const AutomationTaskProgress &,
                               const AutomationTaskProgress &) = default;
    };

    struct AutomationTaskSnapshot {
        TaskId taskId;
        OperationId operationId;
        DocumentVersion baseDocument;
        std::optional<ObjectRef> target;
        AutomationTaskState state = AutomationTaskState::Queued;
        AutomationTaskProgress progress;
        QString message;
        std::optional<MutationResult> mutation;
        std::optional<AutomationError> error;
        bool cancelable = true;
        bool validatedOnly = false;

        friend bool operator==(const AutomationTaskSnapshot &,
                               const AutomationTaskSnapshot &) = default;
    };

    struct TaskAcceptedResult {
        TaskId taskId;
        DocumentVersion document;
        bool validatedOnly = false;

        friend bool operator==(const TaskAcceptedResult &, const TaskAcceptedResult &) = default;
    };

    class AutomationTaskManager final {
    public:
        using CancelCallback = std::function<void()>;

        AutomationTaskSnapshot createTask(OperationId operationId,
                                          DocumentVersion baseDocument,
                                          std::optional<ObjectRef> target = std::nullopt,
                                          CancelCallback cancel = {});

        AutomationResult<AutomationTaskSnapshot> get(const DocumentId &documentId,
                                                     const TaskId &taskId) const;
        QList<AutomationTaskSnapshot> list(const DocumentId &documentId) const;

        AutomationResult<AutomationTaskSnapshot> requestCancel(const DocumentId &documentId,
                                                               const TaskId &taskId);
        AutomationResult<bool> beginCommitting(const TaskId &taskId);
        bool markRunning(const TaskId &taskId);
        bool updateProgress(const TaskId &taskId,
                            AutomationTaskProgress progress,
                            QString message = {});
        bool succeed(const TaskId &taskId, MutationResult mutation);
        bool fail(const TaskId &taskId, AutomationError error);
        bool cancel(const TaskId &taskId);
        [[nodiscard]] bool isCancellationRequested(const TaskId &taskId) const;

        void discardDocumentGeneration(const DocumentId &documentId);
        [[nodiscard]] qsizetype size() const;

    private:
        struct Record {
            AutomationTaskSnapshot snapshot;
            CancelCallback cancel;
        };

        static bool isTerminal(AutomationTaskState state);
        static AutomationError notCancelable(const TaskId &taskId);
        AutomationResult<AutomationTaskSnapshot> findLocked(const DocumentId &documentId,
                                                            const TaskId &taskId) const;

        mutable QMutex m_mutex;
        QHash<TaskId, Record> m_records;
    };

} // namespace Automation

#endif // AUTOMATIONTASKMANAGER_H
