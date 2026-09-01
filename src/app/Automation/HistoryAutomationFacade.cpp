#include "HistoryAutomationFacade.h"
#include "OperationIds.h"

#include <lite/History/HistoryManager.h>

namespace Automation {

    HistoryAutomationFacade::HistoryAutomationFacade(AutomationDispatcher &dispatcher,
                                                     CommandCommitter &committer)
        : m_dispatcher(dispatcher), m_committer(committer) {
    }

    AutomationResult<HistoryStateDto>
        HistoryAutomationFacade::getState(const DocumentId &documentId) {
        return m_dispatcher.dispatchDocumentQuery<HistoryStateDto>(
            OperationIds::history::get_state, documentId, [](DocumentSession &session) {
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

    AutomationResult<MutationResult> HistoryAutomationFacade::undo(const CommandContext &context) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::history::undo, context,
            [this](DocumentSession &session, const bool validateOnly) {
                if (validateOnly && session.history())
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, session.history()->canUndo()));
                return m_committer.undo(session);
            });
    }

    AutomationResult<MutationResult> HistoryAutomationFacade::redo(const CommandContext &context) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::history::redo, context,
            [this](DocumentSession &session, const bool validateOnly) {
                if (validateOnly && session.history())
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, session.history()->canRedo()));
                return m_committer.redo(session);
            });
    }

} // namespace Automation
