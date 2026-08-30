#ifndef AUTOMATIONTASKMANAGER_H
#define AUTOMATIONTASKMANAGER_H

#include "AutomationTypes.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMutex>

#include <functional>
#include <optional>

namespace Automation {

    enum class AutomationTaskScope {
        Document,
        Application,
    };

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
        AutomationTaskScope scope = AutomationTaskScope::Document;
        DocumentVersion baseDocument;
        std::optional<ObjectRef> target;
        AutomationTaskState state = AutomationTaskState::Queued;
        AutomationTaskProgress progress;
        QString message;
        std::optional<MutationResult> mutation;
        std::optional<QJsonObject> applicationResult;
        std::optional<AutomationError> error;
        QString createdByClientId;
        QJsonObject metadata;
        bool cancelable = true;
        bool validatedOnly = false;

        friend bool operator==(const AutomationTaskSnapshot &,
                               const AutomationTaskSnapshot &) = default;
    };

    struct TaskAcceptedResult {
        TaskId taskId;
        DocumentVersion document;
        bool validatedOnly = false;
        AutomationTaskScope scope = AutomationTaskScope::Document;

        friend bool operator==(const TaskAcceptedResult &, const TaskAcceptedResult &) = default;
    };

    class AutomationTaskManager final {
    public:
        static constexpr qsizetype MaximumRetainedDocumentTasks = 128;
        static constexpr qsizetype MaximumRetainedApplicationTasks = 128;

        using CancelCallback = std::function<void()>;
        using UnsuccessfulCallback = std::function<void(const AutomationTaskSnapshot &)>;
        using TerminalCallback = std::function<void(const AutomationTaskSnapshot &)>;

        AutomationTaskSnapshot createTask(OperationId operationId, DocumentVersion baseDocument,
                                          std::optional<ObjectRef> target = std::nullopt,
                                          CancelCallback cancel = {},
                                          QString createdByClientId = {});
        AutomationTaskSnapshot createTask(OperationId operationId, DocumentVersion baseDocument,
                                          std::optional<ObjectRef> target, CancelCallback cancel,
                                          QString createdByClientId, QJsonObject metadata);
        AutomationTaskSnapshot createApplicationTask(OperationId operationId,
                                                     CancelCallback cancel = {},
                                                     QString createdByClientId = {},
                                                     QJsonObject metadata = {});

        AutomationResult<AutomationTaskSnapshot> get(const DocumentId &documentId,
                                                     const TaskId &taskId) const;
        AutomationResult<AutomationTaskSnapshot> getApplication(const TaskId &taskId) const;
        QList<AutomationTaskSnapshot> list(const DocumentId &documentId) const;
        QList<AutomationTaskSnapshot> listApplication() const;

        AutomationResult<AutomationTaskSnapshot> requestCancel(const DocumentId &documentId,
                                                               const TaskId &taskId);
        AutomationResult<AutomationTaskSnapshot> requestCancelApplication(const TaskId &taskId);
        AutomationResult<bool>
            beginCommitting(const TaskId &taskId,
                            std::optional<MutationResult> unsuccessfulMutation = std::nullopt);
        bool setUnsuccessfulCallback(const TaskId &taskId, UnsuccessfulCallback callback);
        bool setTerminalCallback(const TaskId &taskId, TerminalCallback callback);
        bool markRunning(const TaskId &taskId);
        bool updateProgress(const TaskId &taskId, AutomationTaskProgress progress,
                            QString message = {});
        bool succeed(const TaskId &taskId, MutationResult mutation);
        bool succeedApplication(const TaskId &taskId, QJsonObject result);
        bool fail(const TaskId &taskId, AutomationError error,
                  std::optional<MutationResult> mutation = std::nullopt);
        bool cancel(const TaskId &taskId, std::optional<MutationResult> mutation = std::nullopt);
        [[nodiscard]] bool isCancellationRequested(const TaskId &taskId) const;

        void discardDocumentGeneration(const DocumentId &documentId);
        void replaceDocumentGeneration(const DocumentId &oldDocumentId,
                                       const DocumentVersion &newDocument,
                                       const TaskId &preservedTaskId = {});
        [[nodiscard]] qsizetype size() const;

    private:
        struct Record {
            AutomationTaskSnapshot snapshot;
            CancelCallback cancel;
            UnsuccessfulCallback unsuccessful;
            TerminalCallback terminal;
        };

        static bool isTerminal(AutomationTaskState state);
        static AutomationError notCancelable(const TaskId &taskId);
        void retainTerminalDocumentLocked(const TaskId &taskId);
        void retainTerminalApplicationLocked(const TaskId &taskId);
        AutomationResult<AutomationTaskSnapshot> findLocked(const DocumentId &documentId,
                                                            const TaskId &taskId) const;
        AutomationResult<AutomationTaskSnapshot> findApplicationLocked(const TaskId &taskId) const;

        mutable QMutex m_mutex;
        QHash<TaskId, Record> m_records;
        QHash<DocumentId, QList<TaskId>> m_terminalDocumentOrder;
        QList<TaskId> m_terminalApplicationOrder;
    };

} // namespace Automation

#endif // AUTOMATIONTASKMANAGER_H
