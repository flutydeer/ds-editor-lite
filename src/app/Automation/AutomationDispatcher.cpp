#include "AutomationDispatcher.h"

#include <QCryptographicHash>

namespace Automation {

    AutomationDispatcher::AutomationDispatcher(IDocumentSessionResolver &documentResolver,
                                               SingleWindowContext &windowContext)
        : m_documentResolver(documentResolver), m_windowContext(windowContext) {
    }

    bool AutomationDispatcher::requiresPublicBusyAdmission(const InvocationSource source) {
        return source == InvocationSource::PublicMcp ||
               source == InvocationSource::PublicJsonRpc;
    }

    AutomationResult<DocumentVersion>
        AutomationDispatcher::validateDocumentCommand(const CommandContext &context) {
        return runSerialized([&]() -> AutomationResult<DocumentVersion> {
            auto validated = resolveDocumentCommand(context);
            if (!validated)
                return validated.getError();
            const auto &session = validated.get().get();
            if (requiresPublicBusyAdmission(context.source) && session.isBusy())
                return AutomationError::documentBusy(session.documentId());
            if (session.revision() != context.expected.revision) {
                return AutomationError::revisionConflict(
                    session.documentId(), context.expected.revision, session.revision());
            }
            return session.version();
        });
    }

    AutomationResult<DocumentVersion>
        AutomationDispatcher::admitDocumentTask(CommandContext &context) {
        auto validated = validateDocumentCommand(context);
        if (validated && requiresPublicBusyAdmission(context.source))
            context.source = InvocationSource::PublicTaskContinuation;
        return validated;
    }

    AutomationResult<MutationResult>
        AutomationDispatcher::dispatchDocumentCommand(const OperationId &operationId,
                                                      const CommandContext &context,
                                                      const DocumentCommandHandler &handler) {
        return dispatchDocumentCommandResult<MutationResult>(operationId, context, handler);
    }

    AutomationResult<MutationResult> AutomationDispatcher::dispatchDocumentControlCommand(
        const OperationId &operationId, const CommandContext &context,
        const DocumentCommandHandler &handler) {
        if (!context.idempotencyKey.isEmpty()) {
            return decorateError(
                AutomationError::invalidArgument(
                    QStringLiteral("idempotency_key"),
                    QStringLiteral("Operation does not support document idempotency")),
                operationId);
        }
        return dispatchDocumentCommandResultImpl<MutationResult>(operationId, context, nullptr,
                                                                 true, false, handler);
    }

    AutomationResult<MutationResult>
        AutomationDispatcher::dispatchDocumentCommandWithoutRevisionCheck(
            const OperationId &operationId, const CommandContext &context,
            const DocumentCommandHandler &handler) {
        return dispatchDocumentCommandResultWithoutRevisionCheck<MutationResult>(operationId,
                                                                                 context, handler);
    }

    AutomationResult<std::reference_wrapper<DocumentSession>>
        AutomationDispatcher::resolveDocumentCommand(const CommandContext &context) {
        auto resolved = m_documentResolver.resolveDocument(context.expected.documentId);
        if (!resolved)
            return resolved.getError();
        auto &session = resolved.get().get();
        if (session.lifecycleState() != DocumentLifecycleState::Active)
            return AutomationError::documentBusy(session.documentId());
        return std::ref(session);
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
