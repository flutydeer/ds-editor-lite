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
                             SingleWindowContext &windowContext, const OperationCatalog &catalog);

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
                return decorateHandlerResult<T>(std::forward<Handler>(handler)(), operationId);
            });
        }

        template <typename T, typename Handler>
        AutomationResult<T> dispatchApplicationCommand(const OperationId &operationId,
                                                       const ApplicationCommandContext &context,
                                                       Handler &&handler) {
            return runSerialized([&]() -> AutomationResult<T> {
                const auto descriptor = requireDescriptor(operationId, OperationKind::Command);
                if (!descriptor)
                    return descriptor.getError();
                if (descriptor.get()->documentPolicy != DocumentPolicy::None ||
                    descriptor.get()->hostAvailability != HostAvailability::Core) {
                    return decorateError(
                        AutomationError::invalidArgument(
                            QStringLiteral("operation_id"),
                            QStringLiteral("Operation is not an application command")),
                        operationId);
                }
                auto result = std::forward<Handler>(handler)(context.validateOnly);
                if (!result)
                    return decorateError(result.getError(), operationId);
                return result;
            });
        }

        template <typename T, typename Handler>
        AutomationResult<T> dispatchDocumentQuery(const OperationId &operationId,
                                                  const DocumentId &documentId, Handler &&handler) {
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
                return decorateHandlerResult<T>(
                    std::forward<Handler>(handler)(resolved.get().get()), operationId);
            });
        }

        template <typename T, typename Handler>
        AutomationResult<T> dispatchGuiQuery(const OperationId &operationId,
                                             const WindowId &windowId, Handler &&handler) {
            return runSerialized([&]() -> AutomationResult<T> {
                const auto descriptor = requireDescriptor(operationId, OperationKind::Query);
                if (!descriptor)
                    return descriptor.getError();
                if (descriptor.get()->hostAvailability != HostAvailability::GuiOnly) {
                    return decorateError(AutomationError::invalidArgument(
                                             QStringLiteral("operation_id"),
                                             QStringLiteral("Operation is not a GUI-only query")),
                                         operationId);
                }

                const auto validated = m_windowContext.validateWindow(windowId);
                if (!validated)
                    return decorateError(validated.getError(), operationId);
                return decorateHandlerResult<T>(std::forward<Handler>(handler)(), operationId);
            });
        }

        template <typename T, typename Handler>
        AutomationResult<T> dispatchGuiDocumentQuery(const OperationId &operationId,
                                                     const DocumentId &documentId,
                                                     const WindowId &windowId, Handler &&handler) {
            return runSerialized([&]() -> AutomationResult<T> {
                const auto descriptor = requireDescriptor(operationId, OperationKind::Query);
                if (!descriptor)
                    return descriptor.getError();
                if (descriptor.get()->hostAvailability != HostAvailability::GuiOnly ||
                    descriptor.get()->documentPolicy == DocumentPolicy::None) {
                    return decorateError(
                        AutomationError::invalidArgument(
                            QStringLiteral("operation_id"),
                            QStringLiteral("Operation is not a GUI document query")),
                        operationId);
                }

                auto resolved = m_documentResolver.resolveDocument(documentId);
                if (!resolved)
                    return decorateError(resolved.getError(), operationId);
                auto &session = resolved.get().get();
                if (session.lifecycleState() != DocumentLifecycleState::Active)
                    return decorateError(AutomationError::documentBusy(session.documentId()),
                                         operationId);

                const auto validated = m_windowContext.validateWindow(windowId);
                if (!validated)
                    return decorateError(validated.getError(), operationId);
                return decorateHandlerResult<T>(std::forward<Handler>(handler)(session),
                                                operationId);
            });
        }

        template <typename T, typename Handler>
        AutomationResult<T> dispatchGuiCommand(const OperationId &operationId,
                                               const GuiCommandContext &context,
                                               Handler &&handler) {
            return runSerialized([&]() -> AutomationResult<T> {
                const auto descriptor = requireDescriptor(operationId, OperationKind::Command);
                if (!descriptor)
                    return descriptor.getError();
                if (descriptor.get()->hostAvailability != HostAvailability::GuiOnly ||
                    descriptor.get()->documentPolicy != DocumentPolicy::None) {
                    return decorateError(AutomationError::invalidArgument(
                                             QStringLiteral("operation_id"),
                                             QStringLiteral("Operation is not a GUI-only command")),
                                         operationId);
                }
                const auto validated = m_windowContext.validateWindow(context.windowId);
                if (!validated)
                    return decorateError(validated.getError(), operationId);
                auto result = std::forward<Handler>(handler)(context.validateOnly);
                if (!result)
                    return decorateError(result.getError(), operationId);
                return result;
            });
        }

        template <typename T, typename Handler>
        AutomationResult<T> dispatchGuiDocumentCommand(const OperationId &operationId,
                                                       const GuiDocumentCommandContext &context,
                                                       Handler &&handler) {
            return runSerialized([&]() -> AutomationResult<T> {
                const auto descriptor = requireDescriptor(operationId, OperationKind::Command);
                if (!descriptor)
                    return descriptor.getError();
                if (descriptor.get()->hostAvailability != HostAvailability::GuiOnly ||
                    descriptor.get()->documentPolicy == DocumentPolicy::None) {
                    return decorateError(
                        AutomationError::invalidArgument(
                            QStringLiteral("operation_id"),
                            QStringLiteral("Operation is not a GUI document command")),
                        operationId);
                }

                auto resolved = m_documentResolver.resolveDocument(context.expected.documentId);
                if (!resolved)
                    return decorateError(resolved.getError(), operationId);
                auto &session = resolved.get().get();
                if (session.lifecycleState() != DocumentLifecycleState::Active)
                    return decorateError(AutomationError::documentBusy(session.documentId()),
                                         operationId);
                if (descriptor.get()->revisionPolicy != RevisionPolicy::None &&
                    session.revision() != context.expected.revision) {
                    return decorateError(
                        AutomationError::revisionConflict(
                            session.documentId(), context.expected.revision, session.revision()),
                        operationId);
                }

                const auto validated = m_windowContext.validateWindow(context.windowId);
                if (!validated)
                    return decorateError(validated.getError(), operationId);
                auto result = std::forward<Handler>(handler)(session, context.validateOnly);
                if (!result)
                    return decorateError(result.getError(), operationId);
                return result;
            });
        }

        AutomationResult<MutationResult>
            dispatchDocumentCommand(const OperationId &operationId, const CommandContext &context,
                                    const QByteArray &requestFingerprint,
                                    const DocumentCommandHandler &handler);

        template <typename T>
        bool releaseDocumentIdempotency(const OperationId &operationId,
                                        const CommandContext &context,
                                        const QByteArray &requestFingerprint, const T &result) {
            if (context.validateOnly || context.idempotencyKey.isEmpty())
                return false;
            return runSerialized([&] {
                auto resolved = m_documentResolver.resolveDocument(context.expected.documentId);
                if (!resolved)
                    return false;
                auto &session = resolved.get().get();
                return session.idempotencyStore().release(
                    operationId, context.idempotencyKey,
                    effectiveFingerprint(context, requestFingerprint), result);
            });
        }

        template <typename T, typename Handler>
        AutomationResult<T> dispatchDocumentCommandResult(const OperationId &operationId,
                                                          const CommandContext &context,
                                                          const QByteArray &requestFingerprint,
                                                          Handler &&handler) {
            return runSerialized([&]() -> AutomationResult<T> {
                const auto descriptor = requireDescriptor(operationId, OperationKind::Command);
                if (!descriptor)
                    return descriptor.getError();
                if (descriptor.get()->documentPolicy == DocumentPolicy::None) {
                    return decorateError(AutomationError::invalidArgument(
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
                if (!context.idempotencyKey.isEmpty() &&
                    descriptor.get()->idempotency != IdempotencyPolicy::DocumentGeneration) {
                    return decorateError(
                        AutomationError::invalidArgument(
                            QStringLiteral("idempotency_key"),
                            QStringLiteral("Operation does not support document idempotency")),
                        operationId);
                }
                if (!context.validateOnly && !context.idempotencyKey.isEmpty()) {
                    auto replay = session.idempotencyStore().replay<T>(
                        operationId, context.idempotencyKey, fingerprint);
                    if (!replay)
                        return decorateError(replay.getError(), operationId);
                    if (replay.get())
                        return *replay.get();
                }

                if (descriptor.get()->revisionPolicy != RevisionPolicy::None &&
                    session.revision() != context.expected.revision) {
                    return decorateError(
                        AutomationError::revisionConflict(
                            session.documentId(), context.expected.revision, session.revision()),
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
        template <typename T>
        static AutomationResult<T> decorateHandlerResult(AutomationResult<T> result,
                                                         const OperationId &operationId) {
            if (!result)
                return decorateError(result.getError(), operationId);
            return result;
        }

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
        static AutomationError decorateError(AutomationError error, const OperationId &operationId);
        static QByteArray effectiveFingerprint(const CommandContext &context,
                                               const QByteArray &requestFingerprint);

        IDocumentSessionResolver &m_documentResolver;
        SingleWindowContext &m_windowContext;
        const OperationCatalog &m_catalog;
    };

} // namespace Automation

#endif // AUTOMATIONDISPATCHER_H
