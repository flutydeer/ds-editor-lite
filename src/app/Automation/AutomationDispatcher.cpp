#include "AutomationDispatcher.h"

#include <QCryptographicHash>

namespace Automation {

    AutomationDispatcher::AutomationDispatcher(IDocumentSessionResolver &documentResolver,
                                               SingleWindowContext &windowContext,
                                               const OperationCatalog &catalog)
        : m_documentResolver(documentResolver), m_windowContext(windowContext), m_catalog(catalog) {
    }

    AutomationResult<MutationResult> AutomationDispatcher::dispatchDocumentCommand(
        const OperationId &operationId, const CommandContext &context,
        const QByteArray &requestFingerprint, const DocumentCommandHandler &handler) {
        return dispatchDocumentCommandResult<MutationResult>(operationId, context,
                                                             requestFingerprint, handler);
    }

    AutomationResult<const OperationDescriptor *>
        AutomationDispatcher::requireDescriptor(const OperationId &operationId,
                                                const OperationKind expectedKind) const {
        const auto descriptor = m_catalog.find(operationId);
        if (!descriptor) {
            AutomationError error;
            error.code = AutomationErrorCode::OperationUnavailable;
            error.operationId = operationId;
            error.message = QStringLiteral("Operation is not registered");
            return error;
        }
        if (descriptor->kind != expectedKind) {
            return decorateError(
                AutomationError::invalidArgument(
                    QStringLiteral("operation_id"),
                    QStringLiteral("Operation kind does not match the dispatch path")),
                operationId);
        }
        return descriptor;
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
