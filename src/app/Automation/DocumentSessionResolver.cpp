#include "DocumentSessionResolver.h"

namespace Automation {

    SingleDocumentSessionResolver::SingleDocumentSessionResolver(DocumentSession &session)
        : m_session(session) {
    }

    AutomationResult<std::reference_wrapper<DocumentSession>>
        SingleDocumentSessionResolver::resolveDocument(const DocumentId &documentId) {
        if (documentId != m_session.documentId())
            return AutomationError::documentChanged(documentId, m_session.documentId());
        return std::ref(m_session);
    }

} // namespace Automation
