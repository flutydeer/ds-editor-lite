#ifndef EDITORAUTOMATIONFACADE_H
#define EDITORAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"

namespace Automation {

    struct EditorStateDto {
        DocumentVersion document;
        WindowId windowId;
        QString projectPath;
        QString projectName;
        bool documentBusy = false;
    };

    struct EditorCapabilitiesDto {
        int maxConcurrentDocuments = 1;
        int maxConcurrentWindows = 1;
        QStringList operationIds;
    };

    class EditorAutomationFacade final {
    public:
        EditorAutomationFacade(DocumentSession &session,
                               SingleWindowContext &windowContext,
                               OperationCatalog &catalog,
                               AutomationDispatcher &dispatcher);

        AutomationResult<EditorStateDto> getEditorState(const WindowId &windowId);
        AutomationResult<EditorCapabilitiesDto> getEditorCapabilities();

    private:
        void registerOperations();

        DocumentSession &m_session;
        SingleWindowContext &m_windowContext;
        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
    };

} // namespace Automation

#endif // EDITORAUTOMATIONFACADE_H
