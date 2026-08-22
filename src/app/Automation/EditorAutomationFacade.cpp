#include "EditorAutomationFacade.h"

namespace Automation {

    EditorAutomationFacade::EditorAutomationFacade(DocumentSession &session,
                                                   SingleWindowContext &windowContext,
                                                   OperationCatalog &catalog,
                                                   AutomationDispatcher &dispatcher)
        : m_session(session), m_windowContext(windowContext), m_catalog(catalog),
          m_dispatcher(dispatcher) {
        registerOperations();
    }

    AutomationResult<EditorStateDto>
    EditorAutomationFacade::getEditorState(const WindowId &windowId) {
        return m_dispatcher.dispatchGuiQuery<EditorStateDto>(
            QStringLiteral("editor.get_state"), windowId, [this] {
                EditorStateDto result;
                result.document = m_session.version();
                result.windowId = m_windowContext.windowId();
                result.projectPath = m_session.path();
                result.projectName = m_session.projectName();
                result.documentBusy = m_session.isBusy();
                return AutomationResult<EditorStateDto>(std::move(result));
            });
    }

    AutomationResult<EditorCapabilitiesDto>
    EditorAutomationFacade::getEditorCapabilities() {
        return m_dispatcher.dispatchApplicationQuery<EditorCapabilitiesDto>(
            QStringLiteral("editor.get_capabilities"), [this] {
                EditorCapabilitiesDto result;
                result.operationIds = m_catalog.operationIds();
                return AutomationResult<EditorCapabilitiesDto>(std::move(result));
            });
    }

    void EditorAutomationFacade::registerOperations() {
        m_catalog.add({
            .id = QStringLiteral("editor.get_capabilities"),
            .category = QStringLiteral("editor"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.Empty.v1"),
            .outputContract = QStringLiteral("automation.EditorCapabilities.v1"),
            .documentPolicy = DocumentPolicy::None,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        m_catalog.add({
            .id = QStringLiteral("editor.get_state"),
            .category = QStringLiteral("editor"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.WindowRef.v1"),
            .outputContract = QStringLiteral("automation.EditorState.v1"),
            .documentPolicy = DocumentPolicy::None,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::GuiOnly,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
    }

} // namespace Automation
