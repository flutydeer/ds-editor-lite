#include "CoreRuntime.h"

namespace Automation {

    CoreRuntime::CoreRuntime(AppModel *model, HistoryManager *historyManager,
                             DocumentRuntimeServices documentServices)
        : m_session(model, historyManager), m_documentResolver(m_session),
          m_dispatcher(m_documentResolver, m_windowContext, m_catalog),
          m_documentFacade(m_catalog, m_dispatcher, m_committer, m_taskManager,
                           std::move(documentServices)),
          m_facade(m_session, m_windowContext, m_catalog, m_dispatcher),
          m_historyFacade(m_catalog, m_dispatcher, m_committer),
          m_noteFacade(m_catalog, m_dispatcher, m_committer, m_objectResolver),
          m_parameterFacade(m_catalog, m_dispatcher, m_committer, m_objectResolver),
          m_projectFacade(m_catalog, m_dispatcher, m_committer, m_objectResolver),
          m_taskFacade(m_catalog, m_dispatcher, m_taskManager),
          m_timelineFacade(m_catalog, m_dispatcher, m_committer) {
    }

    DocumentVersion CoreRuntime::documentVersion() const {
        return m_session.version();
    }

    const WindowId &CoreRuntime::windowId() const {
        return m_windowContext.windowId();
    }

    EditorAutomationFacade &CoreRuntime::facade() {
        return m_facade;
    }

    const EditorAutomationFacade &CoreRuntime::facade() const {
        return m_facade;
    }

    OperationCatalog &CoreRuntime::catalog() {
        return m_catalog;
    }

    const OperationCatalog &CoreRuntime::catalog() const {
        return m_catalog;
    }

    AutomationDispatcher &CoreRuntime::dispatcher() {
        return m_dispatcher;
    }

    DocumentAutomationFacade &CoreRuntime::documents() {
        return m_documentFacade;
    }

    HistoryAutomationFacade &CoreRuntime::history() {
        return m_historyFacade;
    }

    NoteAutomationFacade &CoreRuntime::notes() {
        return m_noteFacade;
    }

    ParameterAutomationFacade &CoreRuntime::parameters() {
        return m_parameterFacade;
    }

    ProjectAutomationFacade &CoreRuntime::project() {
        return m_projectFacade;
    }

    TaskAutomationFacade &CoreRuntime::tasks() {
        return m_taskFacade;
    }

    AutomationTaskManager &CoreRuntime::automationTasks() {
        return m_taskManager;
    }

    TimelineAutomationFacade &CoreRuntime::timeline() {
        return m_timelineFacade;
    }

    DocumentVersion CoreRuntime::replaceDocumentGeneration(QString path, QString projectName) {
        m_taskManager.discardDocumentGeneration(m_session.documentId());
        return m_session.replaceGeneration(std::move(path), std::move(projectName));
    }

    bool CoreRuntime::setDocumentBusy(const DocumentId &documentId, const bool busy) {
        if (documentId != m_session.documentId())
            return false;
        m_session.setBusy(busy);
        return true;
    }

    bool CoreRuntime::setDocumentIdentity(const DocumentId &documentId, QString path,
                                          QString projectName) {
        if (documentId != m_session.documentId())
            return false;
        m_session.setPathAndProjectName(std::move(path), std::move(projectName));
        return true;
    }

} // namespace Automation
