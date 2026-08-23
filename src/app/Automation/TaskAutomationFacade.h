#ifndef TASKAUTOMATIONFACADE_H
#define TASKAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "AutomationTaskManager.h"

namespace Automation {

    class TaskAutomationFacade final {
    public:
        TaskAutomationFacade(OperationCatalog &catalog,
                             AutomationDispatcher &dispatcher,
                             AutomationTaskManager &tasks);

        AutomationResult<AutomationTaskSnapshot> getTask(const DocumentId &documentId,
                                                         const TaskId &taskId);
        AutomationResult<QList<AutomationTaskSnapshot>> listTasks(const DocumentId &documentId);
        AutomationResult<AutomationTaskSnapshot> cancelTask(const CommandContext &context,
                                                            const TaskId &taskId);

    private:
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        AutomationTaskManager &m_tasks;
    };

} // namespace Automation

#endif // TASKAUTOMATIONFACADE_H
