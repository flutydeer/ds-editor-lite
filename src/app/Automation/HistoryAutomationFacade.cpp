#include "HistoryAutomationFacade.h"

#include <lite/History/HistoryManager.h>

namespace Automation {

    HistoryAutomationFacade::HistoryAutomationFacade(OperationCatalog &catalog,
                                                     AutomationDispatcher &dispatcher,
                                                     CommandCommitter &committer)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_committer(committer) {
        registerOperations();
    }

    AutomationResult<HistoryStateDto>
    HistoryAutomationFacade::getState(const DocumentId &documentId) {
        return m_dispatcher.dispatchDocumentQuery<HistoryStateDto>(
            QStringLiteral("history.get_state"), documentId, [](DocumentSession &session) {
                auto *history = session.history();
                if (!history) {
                    AutomationError error;
                    error.code = AutomationErrorCode::InternalError;
                    error.message = QStringLiteral("Document session has no HistoryManager");
                    return AutomationResult<HistoryStateDto>(std::move(error));
                }

                HistoryStateDto result;
                result.document = session.version();
                result.canUndo = history->canUndo();
                result.canRedo = history->canRedo();
                result.onSavePoint = history->isOnSavePoint();
                result.undoName = history->undoActionName();
                result.redoName = history->redoActionName();
                return AutomationResult<HistoryStateDto>(std::move(result));
            });
    }

    AutomationResult<MutationResult>
    HistoryAutomationFacade::undo(const CommandContext &context) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("history.undo"), context, QByteArrayLiteral("undo"),
            [this](DocumentSession &session, const bool validateOnly) {
                if (validateOnly && session.history())
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, session.history()->canUndo()));
                return m_committer.undo(session);
            });
    }

    AutomationResult<MutationResult>
    HistoryAutomationFacade::redo(const CommandContext &context) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("history.redo"), context, QByteArrayLiteral("redo"),
            [this](DocumentSession &session, const bool validateOnly) {
                if (validateOnly && session.history())
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, session.history()->canRedo()));
                return m_committer.redo(session);
            });
    }

    void HistoryAutomationFacade::registerOperations() {
        const auto add = [this](OperationDescriptor descriptor) {
            const auto result = m_catalog.add(std::move(descriptor));
            Q_ASSERT(result);
        };

        add({
            .id = QStringLiteral("history.get_state"),
            .category = QStringLiteral("history"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.DocumentRef.v1"),
            .outputContract = QStringLiteral("automation.HistoryState.v1"),
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        for (const auto id : {QStringLiteral("history.redo"), QStringLiteral("history.undo")}) {
            add({
                .id = id,
                .category = QStringLiteral("history"),
                .kind = OperationKind::Command,
                .syncMode = SyncMode::Synchronous,
                .inputContract = QStringLiteral("automation.DocumentCommand.v1"),
                .outputContract = QStringLiteral("automation.MutationResult.v1"),
                .documentPolicy = DocumentPolicy::Write,
                .revisionPolicy = RevisionPolicy::Increment,
                .historyPolicy = HistoryPolicy::UndoRedo,
                .fileAccess = FileAccessPolicy::None,
                .hostAvailability = HostAvailability::Core,
                .safety = SafetyClass::Reversible,
                .exposure = ExposurePolicy::InternalOnly,
                .idempotency = IdempotencyPolicy::DocumentGeneration,
            });
        }
    }

} // namespace Automation
