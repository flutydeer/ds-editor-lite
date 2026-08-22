#include "CommandCommitter.h"

#include <lite/History/ActionSequence.h>
#include <lite/History/HistoryManager.h>

namespace Automation {

    AutomationResult<MutationResult>
    CommandCommitter::commit(DocumentSession &session,
                             std::unique_ptr<ActionSequence> actions,
                             QList<ObjectRef> affectedObjects) {
        if (!actions) {
            return AutomationError::invalidArgument(QStringLiteral("actions"),
                                                    QStringLiteral("Action sequence is required"));
        }

        MutationResult result;
        result.previous = session.version();
        result.current = result.previous;
        result.affectedObjects = std::move(affectedObjects);
        if (actions->count() == 0)
            return result;

        auto *history = session.history();
        if (!history) {
            AutomationError error;
            error.code = AutomationErrorCode::InternalError;
            error.message = QStringLiteral("Document session has no HistoryManager");
            return error;
        }

        actions->execute();
        history->record(actions.release());
        result.current = session.advanceRevision();
        result.changed = true;
        return result;
    }

    MutationResult CommandCommitter::commitStateChange(DocumentSession &session,
                                                       const bool changed,
                                                       const std::function<void()> &apply,
                                                       QList<ObjectRef> affectedObjects) {
        MutationResult result;
        result.previous = session.version();
        result.current = result.previous;
        result.affectedObjects = std::move(affectedObjects);
        if (!changed)
            return result;
        apply();
        result.current = session.advanceRevision();
        result.changed = true;
        return result;
    }

    MutationResult CommandCommitter::preview(const DocumentSession &session,
                                             const bool wouldChange,
                                             QList<ObjectRef> affectedObjects) const {
        MutationResult result;
        result.previous = session.version();
        result.current = result.previous;
        result.affectedObjects = std::move(affectedObjects);
        result.changed = wouldChange;
        result.validatedOnly = true;
        if (wouldChange)
            ++result.current.revision;
        return result;
    }

    MutationResult CommandCommitter::unchanged(const DocumentSession &session) const {
        MutationResult result;
        result.previous = session.version();
        result.current = result.previous;
        return result;
    }

    AutomationResult<MutationResult> CommandCommitter::undo(DocumentSession &session) {
        auto *history = session.history();
        if (!history) {
            AutomationError error;
            error.code = AutomationErrorCode::InternalError;
            error.message = QStringLiteral("Document session has no HistoryManager");
            return error;
        }

        MutationResult result;
        result.previous = session.version();
        result.current = result.previous;
        if (!history->canUndo())
            return result;

        history->undo();
        result.current = session.advanceRevision();
        result.changed = true;
        return result;
    }

    AutomationResult<MutationResult> CommandCommitter::redo(DocumentSession &session) {
        auto *history = session.history();
        if (!history) {
            AutomationError error;
            error.code = AutomationErrorCode::InternalError;
            error.message = QStringLiteral("Document session has no HistoryManager");
            return error;
        }

        MutationResult result;
        result.previous = session.version();
        result.current = result.previous;
        if (!history->canRedo())
            return result;

        history->redo();
        result.current = session.advanceRevision();
        result.changed = true;
        return result;
    }

} // namespace Automation
