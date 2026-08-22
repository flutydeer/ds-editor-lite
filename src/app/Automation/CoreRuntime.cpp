#include "CoreRuntime.h"

namespace Automation {

    CoreRuntime::CoreRuntime(AppModel *model, HistoryManager *historyManager)
        : m_session(model, historyManager), m_documentResolver(m_session),
          m_dispatcher(m_documentResolver, m_windowContext, m_catalog),
          m_facade(m_session, m_windowContext, m_catalog, m_dispatcher),
          m_historyFacade(m_catalog, m_dispatcher, m_committer),
          m_noteFacade(m_catalog, m_dispatcher, m_committer, m_objectResolver),
          m_parameterFacade(m_catalog, m_dispatcher, m_committer, m_objectResolver),
          m_projectFacade(m_catalog, m_dispatcher, m_committer, m_objectResolver),
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

    TimelineAutomationFacade &CoreRuntime::timeline() {
        return m_timelineFacade;
    }

    DocumentVersion CoreRuntime::replaceDocumentGeneration(QString path, QString projectName) {
        return m_session.replaceGeneration(std::move(path), std::move(projectName));
    }

} // namespace Automation
