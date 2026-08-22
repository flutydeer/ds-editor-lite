#ifndef COMMANDCOMMITTER_H
#define COMMANDCOMMITTER_H

#include "DocumentSession.h"

#include <memory>
#include <functional>

class ActionSequence;

namespace Automation {

    class CommandCommitter final {
    public:
        AutomationResult<MutationResult>
        commit(DocumentSession &session,
               std::unique_ptr<ActionSequence> actions,
               QList<ObjectRef> affectedObjects = {});
        MutationResult commitStateChange(DocumentSession &session,
                                         bool changed,
                                         const std::function<void()> &apply,
                                         QList<ObjectRef> affectedObjects = {});

        [[nodiscard]] MutationResult preview(const DocumentSession &session,
                                             bool wouldChange,
                                             QList<ObjectRef> affectedObjects = {}) const;
        [[nodiscard]] MutationResult unchanged(const DocumentSession &session) const;

        AutomationResult<MutationResult> undo(DocumentSession &session);
        AutomationResult<MutationResult> redo(DocumentSession &session);
    };

} // namespace Automation

#endif // COMMANDCOMMITTER_H
