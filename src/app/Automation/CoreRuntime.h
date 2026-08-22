#ifndef CORERUNTIME_H
#define CORERUNTIME_H

#include "DocumentSessionResolver.h"
#include "EditorAutomationFacade.h"
#include "HistoryAutomationFacade.h"
#include "TimelineAutomationFacade.h"

class AppModel;
class HistoryManager;

namespace Automation {

    class CoreRuntime final {
    public:
        CoreRuntime(AppModel *model, HistoryManager *historyManager);

        [[nodiscard]] DocumentVersion documentVersion() const;
        [[nodiscard]] const WindowId &windowId() const;

        EditorAutomationFacade &facade();
        const EditorAutomationFacade &facade() const;
        OperationCatalog &catalog();
        const OperationCatalog &catalog() const;
        AutomationDispatcher &dispatcher();
        HistoryAutomationFacade &history();
        TimelineAutomationFacade &timeline();

        DocumentVersion replaceDocumentGeneration(QString path, QString projectName);

    private:
        DocumentSession m_session;
        SingleDocumentSessionResolver m_documentResolver;
        SingleWindowContext m_windowContext;
        OperationCatalog m_catalog;
        AutomationDispatcher m_dispatcher;
        CommandCommitter m_committer;
        EditorAutomationFacade m_facade;
        HistoryAutomationFacade m_historyFacade;
        TimelineAutomationFacade m_timelineFacade;
    };

} // namespace Automation

#endif // CORERUNTIME_H
