#include "TaskAutomationFacade.h"
#include "OperationIds.h"

namespace Automation {
    namespace {
        bool canRequestCancel(const AutomationTaskState state) {
            return state == AutomationTaskState::Queued || state == AutomationTaskState::Running ||
                   state == AutomationTaskState::CancelRequested;
        }
    }

    TaskAutomationFacade::TaskAutomationFacade(AutomationDispatcher &dispatcher,
                                               AutomationTaskManager &tasks)
        : m_dispatcher(dispatcher), m_tasks(tasks) {
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
        return m_dispatcher
            .dispatchDocumentControlCommandResultWithoutRevisionCheck<AutomationTaskSnapshot>(
                OperationIds::tasks::cancel, context,
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

} // namespace Automation
