#ifndef AUTOMATIONDISPATCHER_H
#define AUTOMATIONDISPATCHER_H

#include "DocumentSessionResolver.h"
#include "OperationCatalog.h"
#include "SingleWindowContext.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QThread>

#include <functional>
#include <optional>
#include <type_traits>

namespace Automation {

    class AutomationDispatcher final {
    public:
        using DocumentCommandHandler =
            std::function<AutomationResult<MutationResult>(DocumentSession &, bool validateOnly)>;

        AutomationDispatcher(IDocumentSessionResolver &documentResolver,
                             SingleWindowContext &windowContext,
                             const OperationCatalog &catalog);

        template <typename T, typename Handler>
        AutomationResult<T> dispatchApplicationQuery(const OperationId &operationId,
                                                     Handler &&handler) {
            return runSerialized([&]() -> AutomationResult<T> {
                const auto descriptor = requireDescriptor(operationId, OperationKind::Query);
                if (!descriptor)
                    return descriptor.getError();
                if (descriptor.get()->documentPolicy != DocumentPolicy::None ||
                    descriptor.get()->hostAvailability != HostAvailability::Core) {
                    return decorateError(
                        AutomationError::invalidArgument(
                            QStringLiteral("operation_id"),
                            QStringLiteral("Operation is not an application query")),
                        operationId);
                }
                return std::forward<Handler>(handler)();
            });
        }

        template <typename T, typename Handler>
        AutomationResult<T> dispatchDocumentQuery(const OperationId &operationId,
                                                  const DocumentId &documentId,
                                                  Handler &&handler) {
            return runSerialized([&]() -> AutomationResult<T> {
                const auto descriptor = requireDescriptor(operationId, OperationKind::Query);
                if (!descriptor)
                    return descriptor.getError();

                auto resolved = m_documentResolver.resolveDocument(documentId);
                if (!resolved)
                    return decorateError(resolved.getError(), operationId);
                if (resolved.get().get().lifecycleState() != DocumentLifecycleState::Active)
                    return decorateError(
                        AutomationError::documentBusy(resolved.get().get().documentId()),
                        operationId);
                return std::forward<Handler>(handler)(resolved.get().get());
            });
        }

        template <typename T, typename Handler>
        AutomationResult<T> dispatchGuiQuery(const OperationId &operationId,
                                             const WindowId &windowId,
                                             Handler &&handler) {
            return runSerialized([&]() -> AutomationResult<T> {
                const auto descriptor = requireDescriptor(operationId, OperationKind::Query);
                if (!descriptor)
                    return descriptor.getError();
                if (descriptor.get()->hostAvailability != HostAvailability::GuiOnly) {
                    return decorateError(
                        AutomationError::invalidArgument(
                            QStringLiteral("operation_id"),
                            QStringLiteral("Operation is not a GUI-only query")),
                        operationId);
                }

                const auto validated = m_windowContext.validateWindow(windowId);
                if (!validated)
                    return decorateError(validated.getError(), operationId);
                return std::forward<Handler>(handler)();
            });
        }

        AutomationResult<MutationResult>
        dispatchDocumentCommand(const OperationId &operationId,
                                const CommandContext &context,
                                const QByteArray &requestFingerprint,
                                const DocumentCommandHandler &handler);

        template <typename T, typename Handler>
        AutomationResult<T> dispatchDocumentCommandResult(
            const OperationId &operationId,
            const CommandContext &context,
            const QByteArray &requestFingerprint,
            Handler &&handler) {
            return runSerialized([&]() -> AutomationResult<T> {
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
                if (session.lifecycleState() != DocumentLifecycleState::Active)
                    return decorateError(AutomationError::documentBusy(session.documentId()),
                                         operationId);

                const auto fingerprint = effectiveFingerprint(context, requestFingerprint);
                if (!context.validateOnly && !context.idempotencyKey.isEmpty()) {
                    if (descriptor.get()->idempotency != IdempotencyPolicy::DocumentGeneration) {
                        return decorateError(
                            AutomationError::invalidArgument(
                                QStringLiteral("idempotency_key"),
                                QStringLiteral("Operation does not support document idempotency")),
                            operationId);
                    }
                    auto replay = session.idempotencyStore().replay<T>(
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

                auto result = std::forward<Handler>(handler)(session, context.validateOnly);
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

    private:
        template <typename Callable>
        auto runSerialized(Callable &&callable) -> std::invoke_result_t<Callable> {
            using Result = std::invoke_result_t<Callable>;
            auto *application = QCoreApplication::instance();
            if (!application || application->thread() == QThread::currentThread())
                return std::forward<Callable>(callable)();

            std::optional<Result> result;
            QMetaObject::invokeMethod(
                application,
                [&result, &callable] { result.emplace(std::forward<Callable>(callable)()); },
                Qt::BlockingQueuedConnection);
            return std::move(*result);
        }

        AutomationResult<const OperationDescriptor *>
        requireDescriptor(const OperationId &operationId, OperationKind expectedKind) const;
        static AutomationError decorateError(AutomationError error,
                                               const OperationId &operationId);
        static QByteArray effectiveFingerprint(const CommandContext &context,
                                               const QByteArray &requestFingerprint);

        IDocumentSessionResolver &m_documentResolver;
        SingleWindowContext &m_windowContext;
        const OperationCatalog &m_catalog;
    };

} // namespace Automation

#endif // AUTOMATIONDISPATCHER_H
