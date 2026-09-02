#ifndef DOCUMENTWORKFLOWREVISIONGUARD_H
#define DOCUMENTWORKFLOWREVISIONGUARD_H

#include "Automation/AutomationTypes.h"

#include <optional>

class DocumentWorkflowRevisionGuard final {
public:
    [[nodiscard]] bool ensureApproved(const Automation::DocumentVersion &current,
                                      const bool documentSaved) {
        if (documentSaved)
            m_approved = current;
        return approves(current);
    }

    void approve(const Automation::DocumentVersion &current) {
        m_approved = current;
    }

    [[nodiscard]] bool approves(const Automation::DocumentVersion &current) const {
        return m_approved && *m_approved == current;
    }

private:
    std::optional<Automation::DocumentVersion> m_approved;
};

#endif // DOCUMENTWORKFLOWREVISIONGUARD_H
