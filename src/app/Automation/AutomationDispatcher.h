#ifndef AUTOMATIONDISPATCHER_H
#define AUTOMATIONDISPATCHER_H

#include "DocumentSessionResolver.h"
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
                             SingleWindowContext &windowContext);

        AutomationResult<DocumentVersion>
            validateDocumentCommand(const CommandContext &context);

        template <typename T, typename Handler>
        AutomationResult<T> dispatchApplicationQuery(const OperationId &operationId,
                                                     Handler &&handler) {
            return runSerialized([&]() -> AutomationResult<T> {
                return decorateHandlerResult<T>(std::forward<Handler>(handler)(), operationId);
            });
        }

        template <typename T, typename Handler>
        AutomationResult<T> dispatchApplicationCommand(const OperationId &operationId,
                                                       const ApplicationCommandContext &context,
                                                       Handler &&handler) {
            return runSerialized([&]() -> AutomationResult<T> {
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
                auto resolved = m_documentResolver.resolveDocument(context.documentId);
                if (!resolved)
                    return decorateError(resolved.getError(), operationId);
                auto &session = resolved.get().get();
                if (session.lifecycleState() != DocumentLifecycleState::Active)
                    return decorateError(AutomationError::documentBusy(session.documentId()),
                                         operationId);
                if (context.expectedRevision && session.revision() != *context.expectedRevision) {
                    return decorateError(
                        AutomationError::revisionConflict(session.documentId(),
                                                          *context.expectedRevision,
                                                          session.revision()),
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
                                    const DocumentCommandHandler &handler);

        AutomationResult<MutationResult>
            dispatchDocumentCommandWithoutRevisionCheck(const OperationId &operationId,
                                                        const CommandContext &context,
                                                        const DocumentCommandHandler &handler);

        AutomationResult<MutationResult> dispatchIdempotentDocumentCommand(
            const OperationId &operationId, const CommandContext &context,
            const QByteArray &requestFingerprint, const DocumentCommandHandler &handler);

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
                                                          Handler &&handler) {
            if (!context.idempotencyKey.isEmpty()) {
                return decorateError(
                    AutomationError::invalidArgument(
                        QStringLiteral("idempotency_key"),
                        QStringLiteral("Operation does not support document idempotency")),
                    operationId);
            }
            return dispatchDocumentCommandResultImpl<T>(operationId, context, nullptr, true,
                                                        std::forward<Handler>(handler));
        }

        template <typename T, typename Handler>
        AutomationResult<T> dispatchDocumentCommandResultWithoutRevisionCheck(
            const OperationId &operationId, const CommandContext &context, Handler &&handler) {
            if (!context.idempotencyKey.isEmpty()) {
                return decorateError(
                    AutomationError::invalidArgument(
                        QStringLiteral("idempotency_key"),
                        QStringLiteral("Operation does not support document idempotency")),
                    operationId);
            }
            return dispatchDocumentCommandResultImpl<T>(operationId, context, nullptr, false,
                                                        std::forward<Handler>(handler));
        }

        template <typename T, typename Handler>
        AutomationResult<T> dispatchIdempotentDocumentCommandResult(
            const OperationId &operationId, const CommandContext &context,
            const QByteArray &requestFingerprint, Handler &&handler) {
            return dispatchDocumentCommandResultImpl<T>(operationId, context, &requestFingerprint,
                                                        true, std::forward<Handler>(handler));
        }

    private:
        template <typename T, typename Handler>
        AutomationResult<T> dispatchDocumentCommandResultImpl(const OperationId &operationId,
                                                              const CommandContext &context,
                                                              const QByteArray *requestFingerprint,
                                                              const bool checkRevision,
                                                              Handler &&handler) {
            return runSerialized([&]() -> AutomationResult<T> {
                auto validated = resolveDocumentCommand(context);
                if (!validated)
                    return decorateError(validated.getError(), operationId);
                auto &session = validated.get().get();

                QByteArray fingerprint;
                if (!context.validateOnly && !context.idempotencyKey.isEmpty()) {
                    Q_ASSERT(requestFingerprint);
                    fingerprint = effectiveFingerprint(context, *requestFingerprint);
                    auto replay = session.idempotencyStore().replay<T>(
                        operationId, context.idempotencyKey, fingerprint);
                    if (!replay)
                        return decorateError(replay.getError(), operationId);
                    if (replay.get())
                        return *replay.get();
                }

                if (checkRevision && session.revision() != context.expected.revision) {
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

        AutomationResult<std::reference_wrapper<DocumentSession>>
            resolveDocumentCommand(const CommandContext &context);

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

        static AutomationError decorateError(AutomationError error, const OperationId &operationId);
        static QByteArray effectiveFingerprint(const CommandContext &context,
                                               const QByteArray &requestFingerprint);

        IDocumentSessionResolver &m_documentResolver;
        SingleWindowContext &m_windowContext;
    };

} // namespace Automation

#endif // AUTOMATIONDISPATCHER_H
