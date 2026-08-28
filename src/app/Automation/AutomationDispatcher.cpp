#include "AutomationDispatcher.h"

#include <QCryptographicHash>

namespace Automation {

    AutomationDispatcher::AutomationDispatcher(IDocumentSessionResolver &documentResolver,
                                               SingleWindowContext &windowContext)
        : m_documentResolver(documentResolver), m_windowContext(windowContext) {
    }

    AutomationResult<MutationResult>
        AutomationDispatcher::dispatchDocumentCommand(const OperationId &operationId,
                                                      const CommandContext &context,
                                                      const DocumentCommandHandler &handler) {
        return dispatchDocumentCommandResult<MutationResult>(operationId, context, handler);
    }

    AutomationResult<MutationResult>
        AutomationDispatcher::dispatchDocumentCommandWithoutRevisionCheck(
            const OperationId &operationId, const CommandContext &context,
            const DocumentCommandHandler &handler) {
        return dispatchDocumentCommandResultWithoutRevisionCheck<MutationResult>(operationId,
                                                                                 context, handler);
    }

    AutomationResult<MutationResult> AutomationDispatcher::dispatchIdempotentDocumentCommand(
        const OperationId &operationId, const CommandContext &context,
        const QByteArray &requestFingerprint, const DocumentCommandHandler &handler) {
        return dispatchIdempotentDocumentCommandResult<MutationResult>(operationId, context,
                                                                       requestFingerprint, handler);
    }

    AutomationError AutomationDispatcher::decorateError(AutomationError error,
                                                        const OperationId &operationId) {
        if (error.operationId.isEmpty())
            error.operationId = operationId;
        return error;
    }

    QByteArray AutomationDispatcher::effectiveFingerprint(const CommandContext &context,
                                                          const QByteArray &requestFingerprint) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(context.expected.documentId.toString().toUtf8());
        hash.addData("\0", 1);
        hash.addData(QByteArray::number(context.expected.revision));
        hash.addData("\0", 1);
        hash.addData(requestFingerprint);
        return hash.result();
    }

} // namespace Automation
