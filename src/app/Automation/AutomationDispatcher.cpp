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
        return runSerialized([&]() -> AutomationResult<MutationResult> {
            const auto descriptor = requireDescriptor(operationId, OperationKind::Command);
            if (!descriptor)
                return descriptor.getError();
            if (descriptor.get()->documentPolicy == DocumentPolicy::None) {
                return decorateError(
                    AutomationError::invalidArgument(
                        QStringLiteral("operation_id"),
                        QStringLiteral("Operation is not a document command")),
                    operationId);
            }

            auto resolved = m_documentResolver.resolveDocument(context.expected.documentId);
            if (!resolved)
                return decorateError(resolved.getError(), operationId);
            auto &session = resolved.get().get();

            const auto fingerprint = effectiveFingerprint(context, requestFingerprint);
            if (!context.validateOnly && !context.idempotencyKey.isEmpty()) {
                if (descriptor.get()->idempotency != IdempotencyPolicy::DocumentGeneration) {
                    return decorateError(
                        AutomationError::invalidArgument(
                            QStringLiteral("idempotency_key"),
                            QStringLiteral("Operation does not support document idempotency")),
                        operationId);
                }
                auto replay = session.idempotencyStore().replay<MutationResult>(
                    operationId, context.idempotencyKey, fingerprint);
                if (!replay)
                    return decorateError(replay.getError(), operationId);
                if (replay.get())
                    return *replay.get();
            }

            if (session.revision() != context.expected.revision) {
                return decorateError(
                    AutomationError::revisionConflict(session.documentId(),
                                                      context.expected.revision,
                                                      session.revision()),
                    operationId);
            }

            auto result = handler(session, context.validateOnly);
            if (!result)
                return decorateError(result.getError(), operationId);

            if (!context.validateOnly && !context.idempotencyKey.isEmpty()) {
                auto stored = session.idempotencyStore().store(
                    operationId, context.idempotencyKey, fingerprint, result.get());
                if (!stored)
                    return decorateError(stored.getError(), operationId);
            }
            return result;
        });
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
