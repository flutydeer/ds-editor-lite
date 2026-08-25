#include "TaskAutomationFacade.h"
#include "OperationIds.h"

namespace Automation {
    namespace {
        QByteArray taskFingerprint(const TaskId &taskId) {
            return taskId.toString().toUtf8();
        }

        bool canRequestCancel(const AutomationTaskState state) {
            return state == AutomationTaskState::Queued || state == AutomationTaskState::Running ||
                   state == AutomationTaskState::CancelRequested;
        }
    }

    TaskAutomationFacade::TaskAutomationFacade(OperationCatalog &catalog,
                                               AutomationDispatcher &dispatcher,
                                               AutomationTaskManager &tasks)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_tasks(tasks) {
        registerOperations();
    }

    AutomationResult<AutomationTaskSnapshot>
        TaskAutomationFacade::getTask(const DocumentId &documentId, const TaskId &taskId) {
        return m_dispatcher.dispatchDocumentQuery<AutomationTaskSnapshot>(
            OperationIds::tasks::get, documentId, [this, taskId](DocumentSession &session) {
                return m_tasks.get(session.documentId(), taskId);
            });
    }

    AutomationResult<QList<AutomationTaskSnapshot>>
        TaskAutomationFacade::listTasks(const DocumentId &documentId) {
        return m_dispatcher.dispatchDocumentQuery<QList<AutomationTaskSnapshot>>(
            OperationIds::tasks::list, documentId, [this](DocumentSession &session) {
                return AutomationResult<QList<AutomationTaskSnapshot>>(
                    m_tasks.list(session.documentId()));
            });
    }

    AutomationResult<AutomationTaskSnapshot>
        TaskAutomationFacade::cancelTask(const CommandContext &context, const TaskId &taskId) {
        return m_dispatcher.dispatchDocumentCommandResult<AutomationTaskSnapshot>(
            OperationIds::tasks::cancel, context, taskFingerprint(taskId),
            [this, taskId](DocumentSession &session, const bool validateOnly) {
                if (!validateOnly)
                    return m_tasks.requestCancel(session.documentId(), taskId);
                auto current = m_tasks.get(session.documentId(), taskId);
                if (!current)
                    return current;
                auto preview = current.get();
                preview.validatedOnly = true;
                if (preview.state == AutomationTaskState::Committing) {
                    AutomationError error;
                    error.code = AutomationErrorCode::OperationNotCancelable;
                    error.taskId = taskId;
                    error.message = QStringLiteral("Automation task can no longer be canceled");
                    return AutomationResult<AutomationTaskSnapshot>(std::move(error));
                }
                if (canRequestCancel(preview.state)) {
                    preview.state = AutomationTaskState::CancelRequested;
                    preview.cancelable = false;
                }
                return AutomationResult<AutomationTaskSnapshot>(std::move(preview));
            });
    }

    void TaskAutomationFacade::registerOperations() {
        const auto add = [this](OperationDescriptor descriptor) {
            const auto result = m_catalog.add(std::move(descriptor));
            Q_ASSERT(result);
        };
        const auto addQuery = [&add](const OperationId &id) {
            add({
                .id = id,
                .category = QStringLiteral("tasks"),
                .kind = OperationKind::Query,
                .syncMode = SyncMode::Synchronous,
                .documentPolicy = DocumentPolicy::Read,
                .revisionPolicy = RevisionPolicy::None,
                .historyPolicy = HistoryPolicy::None,
                .fileAccess = FileAccessPolicy::None,
                .hostAvailability = HostAvailability::Core,
                .safety = SafetyClass::ReadOnly,
                .exposure = ExposurePolicy::InternalOnly,
                .idempotency = IdempotencyPolicy::Unsupported,
            });
        };
        addQuery(OperationIds::tasks::get);
        addQuery(OperationIds::tasks::list);
        add({
            .id = OperationIds::tasks::cancel,
            .category = QStringLiteral("tasks"),
            .kind = OperationKind::Command,
            .syncMode = SyncMode::Synchronous,
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::Reversible,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
    }

} // namespace Automation
