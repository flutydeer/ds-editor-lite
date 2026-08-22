#ifndef CORERUNTIME_H
#define CORERUNTIME_H

#include "DocumentSessionResolver.h"
#include "DocumentAutomationFacade.h"
#include "EditorAutomationFacade.h"
#include "HistoryAutomationFacade.h"
#include "NoteAutomationFacade.h"
#include "ParameterAutomationFacade.h"
#include "ProjectAutomationFacade.h"
#include "TimelineAutomationFacade.h"
#include "TaskAutomationFacade.h"

class AppModel;
class HistoryManager;

namespace Automation {

    class CoreRuntime final {
    public:
        CoreRuntime(AppModel *model,
                    HistoryManager *historyManager,
                    DocumentRuntimeServices documentServices = {});

        [[nodiscard]] DocumentVersion documentVersion() const;
        [[nodiscard]] const WindowId &windowId() const;

        EditorAutomationFacade &facade();
        const EditorAutomationFacade &facade() const;
        OperationCatalog &catalog();
        const OperationCatalog &catalog() const;
        AutomationDispatcher &dispatcher();
        DocumentAutomationFacade &documents();
        HistoryAutomationFacade &history();
        NoteAutomationFacade &notes();
        ParameterAutomationFacade &parameters();
        ProjectAutomationFacade &project();
        TaskAutomationFacade &tasks();
        AutomationTaskManager &automationTasks();
        TimelineAutomationFacade &timeline();

        DocumentVersion replaceDocumentGeneration(QString path, QString projectName);
        bool setDocumentBusy(const DocumentId &documentId, bool busy);
        bool setDocumentIdentity(const DocumentId &documentId, QString path, QString projectName);

    private:
        DocumentSession m_session;
        SingleDocumentSessionResolver m_documentResolver;
        SingleWindowContext m_windowContext;
        OperationCatalog m_catalog;
        AutomationDispatcher m_dispatcher;
        CommandCommitter m_committer;
        DocumentObjectResolver m_objectResolver;
        AutomationTaskManager m_taskManager;
        DocumentAutomationFacade m_documentFacade;
        EditorAutomationFacade m_facade;
        HistoryAutomationFacade m_historyFacade;
        NoteAutomationFacade m_noteFacade;
        ParameterAutomationFacade m_parameterFacade;
        ProjectAutomationFacade m_projectFacade;
        TaskAutomationFacade m_taskFacade;
        TimelineAutomationFacade m_timelineFacade;
    };

} // namespace Automation

#endif // CORERUNTIME_H
