#ifndef DOCUMENTSESSIONRESOLVER_H
#define DOCUMENTSESSIONRESOLVER_H

#include "DocumentSession.h"

#include <functional>

namespace Automation {

    class IDocumentSessionResolver {
    public:
        virtual ~IDocumentSessionResolver() = default;

        virtual AutomationResult<std::reference_wrapper<DocumentSession>>
            resolveDocument(const DocumentId &documentId) = 0;
    };

    class SingleDocumentSessionResolver final : public IDocumentSessionResolver {
    public:
        explicit SingleDocumentSessionResolver(DocumentSession &session);

        AutomationResult<std::reference_wrapper<DocumentSession>>
            resolveDocument(const DocumentId &documentId) override;

    private:
        DocumentSession &m_session;
    };

} // namespace Automation

#endif // DOCUMENTSESSIONRESOLVER_H
