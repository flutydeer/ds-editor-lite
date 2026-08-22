#include "CoreRuntime.h"

namespace Automation {

    CoreRuntime::CoreRuntime(AppModel *model, HistoryManager *historyManager)
        : m_session(model, historyManager), m_documentResolver(m_session),
          m_dispatcher(m_documentResolver, m_windowContext, m_catalog),
          m_facade(m_session, m_windowContext, m_catalog, m_dispatcher) {
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

    DocumentVersion CoreRuntime::replaceDocumentGeneration(QString path, QString projectName) {
        return m_session.replaceGeneration(std::move(path), std::move(projectName));
    }

} // namespace Automation
