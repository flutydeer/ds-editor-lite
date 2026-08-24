#ifndef HISTORYAUTOMATIONFACADE_H
#define HISTORYAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "CommandCommitter.h"

namespace Automation {

    struct HistoryStateDto {
        DocumentVersion document;
        bool canUndo = false;
        bool canRedo = false;
        bool onSavePoint = true;
        QString undoName;
        QString redoName;
    };

    class HistoryAutomationFacade final {
    public:
        HistoryAutomationFacade(OperationCatalog &catalog, AutomationDispatcher &dispatcher,
                                CommandCommitter &committer);

        AutomationResult<HistoryStateDto> getState(const DocumentId &documentId);
        AutomationResult<MutationResult> undo(const CommandContext &context);
        AutomationResult<MutationResult> redo(const CommandContext &context);

    private:
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        CommandCommitter &m_committer;
    };

} // namespace Automation

#endif // HISTORYAUTOMATIONFACADE_H
