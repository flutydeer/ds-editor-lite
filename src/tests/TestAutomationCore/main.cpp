#include "Automation/AutomationDispatcher.h"

#include <QCoreApplication>
#include <QTextStream>

#include <functional>

namespace {
    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    class FakeResolver final : public Automation::IDocumentSessionResolver {
    public:
        FakeResolver(Automation::DocumentSession &first, Automation::DocumentSession &second)
            : m_first(first), m_second(second) {
        }

        Automation::AutomationResult<std::reference_wrapper<Automation::DocumentSession>>
            resolveDocument(const Automation::DocumentId &documentId) override {
            if (documentId == m_first.documentId())
                return std::ref(m_first);
            if (documentId == m_second.documentId())
                return std::ref(m_second);
            return Automation::AutomationError::documentChanged(documentId, m_first.documentId());
        }

    private:
        Automation::DocumentSession &m_first;
        Automation::DocumentSession &m_second;
    };

    Automation::AutomationResult<Automation::MutationResult>
        commit(Automation::DocumentSession &session, const bool validateOnly) {
        Automation::MutationResult result;
        result.previous = session.version();
        result.current = validateOnly ? session.version() : session.advanceRevision();
        result.changed = true;
        result.validatedOnly = validateOnly;
        return result;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    bool ok = true;

    Automation::DocumentSession first(nullptr, nullptr);
    Automation::DocumentSession second(nullptr, nullptr);
    FakeResolver resolver(first, second);
    Automation::SingleWindowContext window;
    Automation::AutomationDispatcher dispatcher(resolver, window);
    ok &= expect(window.windowId() && !window.windowId()->isNull(),
                 "default single-window context must create a real window ID");

    const auto secondQuery = dispatcher.dispatchDocumentQuery<Automation::Revision>(
        QStringLiteral("test.query"), second.documentId(),
        [](Automation::DocumentSession &session) {
            return Automation::AutomationResult<Automation::Revision>(session.revision());
        });
    ok &= expect(secondQuery && secondQuery.get() == 0,
                 "dispatcher must route by explicit document ID");

    const auto unknownDocument = Automation::DocumentId::create();
    const auto missingQuery = dispatcher.dispatchDocumentQuery<Automation::Revision>(
        QStringLiteral("test.query"), unknownDocument, [](Automation::DocumentSession &session) {
            return Automation::AutomationResult<Automation::Revision>(session.revision());
        });
    ok &= expect(!missingQuery &&
                     missingQuery.getError().code ==
                         Automation::AutomationErrorCode::DocumentChanged &&
                     missingQuery.getError().operationId == QStringLiteral("test.query"),
                 "dispatcher must decorate document resolution failures with the operation ID");

    const auto handlerFailure = dispatcher.dispatchApplicationQuery<int>(
        QStringLiteral("test.failure"), []() -> Automation::AutomationResult<int> {
            return Automation::AutomationError::invalidArgument(QStringLiteral("value"),
                                                                QStringLiteral("invalid"));
        });
    ok &= expect(!handlerFailure &&
                     handlerFailure.getError().operationId == QStringLiteral("test.failure") &&
                     handlerFailure.getError().fieldPath == QStringLiteral("value"),
                 "dispatcher must preserve handler details and add operation context");

    Automation::CommandContext command;
    command.expected = first.version();
    command.validateOnly = true;
    command.source = Automation::InvocationSource::Test;
    const auto preview = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), command,
        [](Automation::DocumentSession &session, const bool validateOnly) {
            return commit(session, validateOnly);
        });
    ok &= expect(preview && preview.get().validatedOnly && first.revision() == 0,
                 "validate-only commands must execute without advancing revision");

    command.validateOnly = false;
    const auto committed = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), command,
        [](Automation::DocumentSession &session, const bool validateOnly) {
            return commit(session, validateOnly);
        });
    ok &= expect(committed && committed.get().current.revision == 1 && first.revision() == 1,
                 "document commands must commit against the expected revision");

    int staleHandlerCalls = 0;
    const auto stale = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), command,
        [&staleHandlerCalls](Automation::DocumentSession &session, const bool validateOnly) {
            ++staleHandlerCalls;
            return commit(session, validateOnly);
        });
    ok &= expect(!stale &&
                     stale.getError().code == Automation::AutomationErrorCode::RevisionConflict &&
                     staleHandlerCalls == 0,
                 "stale revisions must be rejected before entering the handler");

    auto admittedTaskCommand = command;
    admittedTaskCommand.expected = first.version();
    admittedTaskCommand.source = Automation::InvocationSource::PublicMcp;
    const auto admittedTaskBase = dispatcher.admitDocumentTask(admittedTaskCommand);
    first.setBusy(true);
    const auto admittedTaskCompletion = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.task.complete"), admittedTaskCommand,
        [](Automation::DocumentSession &session, const bool validateOnly) {
            return commit(session, validateOnly);
        });
    const auto staleTaskCompletion = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.task.complete"), admittedTaskCommand,
        [](Automation::DocumentSession &session, const bool validateOnly) {
            return commit(session, validateOnly);
        });
    const auto queryWhileBusy = dispatcher.dispatchDocumentQuery<Automation::Revision>(
        QStringLiteral("test.query"), first.documentId(), [](Automation::DocumentSession &session) {
            return Automation::AutomationResult<Automation::Revision>(session.revision());
        });
    auto publicCommand = command;
    publicCommand.expected = first.version();
    publicCommand.source = Automation::InvocationSource::PublicMcp;
    int publicHandlerCalls = 0;
    const auto publicValidationBusy = dispatcher.validateDocumentCommand(publicCommand);
    auto rejectedTaskCommand = publicCommand;
    const auto rejectedTaskAdmission = dispatcher.admitDocumentTask(rejectedTaskCommand);
    const auto publicBusy = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), publicCommand,
        [&publicHandlerCalls](Automation::DocumentSession &session, const bool validateOnly) {
            ++publicHandlerCalls;
            return commit(session, validateOnly);
        });
    ++publicCommand.expected.revision;
    const auto stalePublicBusy = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), publicCommand,
        [&publicHandlerCalls](Automation::DocumentSession &session, const bool validateOnly) {
            ++publicHandlerCalls;
            return commit(session, validateOnly);
        });
    auto publicControl = publicCommand;
    publicControl.expected = first.version();
    publicControl.validateOnly = true;
    const auto allowedControl = dispatcher.dispatchDocumentControlCommand(
        QStringLiteral("test.control"), publicControl,
        [](Automation::DocumentSession &session, const bool validateOnly) {
            return commit(session, validateOnly);
        });
    ++publicControl.expected.revision;
    const auto staleControl = dispatcher.dispatchDocumentControlCommand(
        QStringLiteral("test.control"), publicControl,
        [](Automation::DocumentSession &session, const bool validateOnly) {
            return commit(session, validateOnly);
        });
    const auto allowedCancellation =
        dispatcher.dispatchDocumentControlCommandResultWithoutRevisionCheck<Automation::Revision>(
            QStringLiteral("test.cancel"), publicControl,
            [](Automation::DocumentSession &session, const bool) {
                return Automation::AutomationResult<Automation::Revision>(session.revision());
            });
    auto trustedCommand = publicCommand;
    trustedCommand.expected = first.version();
    trustedCommand.validateOnly = true;
    trustedCommand.source = Automation::InvocationSource::TrustedGui;
    const auto trustedPreview = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), trustedCommand,
        [](Automation::DocumentSession &session, const bool validateOnly) {
            return commit(session, validateOnly);
        });
    auto internalCommand = trustedCommand;
    internalCommand.source = Automation::InvocationSource::InternalAutomation;
    const auto internalPreview = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), internalCommand,
        [](Automation::DocumentSession &session, const bool validateOnly) {
            return commit(session, validateOnly);
        });
    ok &= expect(
        admittedTaskBase &&
            admittedTaskCommand.source == Automation::InvocationSource::PublicMcpContinuation &&
            admittedTaskCompletion && !staleTaskCompletion &&
            staleTaskCompletion.getError().code ==
                Automation::AutomationErrorCode::RevisionConflict &&
            queryWhileBusy && queryWhileBusy.get() == first.revision() && !publicValidationBusy &&
            publicValidationBusy.getError().code == Automation::AutomationErrorCode::Busy &&
            !rejectedTaskAdmission &&
            rejectedTaskAdmission.getError().code == Automation::AutomationErrorCode::Busy &&
            rejectedTaskCommand.source == Automation::InvocationSource::PublicMcp &&
            !publicBusy && publicBusy.getError().code == Automation::AutomationErrorCode::Busy &&
            !stalePublicBusy &&
            stalePublicBusy.getError().code == Automation::AutomationErrorCode::Busy &&
            publicHandlerCalls == 0 && allowedControl && !staleControl &&
            staleControl.getError().code == Automation::AutomationErrorCode::RevisionConflict &&
            allowedCancellation && trustedPreview && internalPreview,
        "workflow busy must reject new public mutations while allowing admitted tasks, document "
        "controls, queries, and trusted work");
    first.setBusy(false);
    publicCommand.expected = first.version();
    const auto publicValidation = dispatcher.validateDocumentCommand(publicCommand);
    ++publicCommand.expected.revision;
    const auto stalePublicValidation = dispatcher.validateDocumentCommand(publicCommand);
    ok &= expect(publicValidation && publicValidation.get() == first.version() &&
                     !stalePublicValidation &&
                     stalePublicValidation.getError().code ==
                         Automation::AutomationErrorCode::RevisionConflict,
                 "public async admission must share the dispatcher revision contract");

    auto unsupportedKey = command;
    unsupportedKey.expected = first.version();
    unsupportedKey.idempotencyKey = QStringLiteral("unsupported-key");
    const auto rejectedKey = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), unsupportedKey,
        [](Automation::DocumentSession &session, const bool validateOnly) {
            return commit(session, validateOnly);
        });
    ok &= expect(!rejectedKey &&
                     rejectedKey.getError().code ==
                         Automation::AutomationErrorCode::InvalidArgument &&
                     rejectedKey.getError().fieldPath == QStringLiteral("idempotency_key"),
                 "ordinary dispatcher commands must reject unsupported idempotency keys");

    auto keyed = command;
    keyed.expected = first.version();
    keyed.idempotencyKey = QStringLiteral("replay-key");
    int executions = 0;
    const auto idempotentHandler = [&executions](Automation::DocumentSession &session,
                                                 const bool validateOnly) {
        ++executions;
        return commit(session, validateOnly);
    };
    const auto firstResult = dispatcher.dispatchIdempotentDocumentCommand(
        QStringLiteral("test.idempotent"), keyed, QByteArrayLiteral("payload"), idempotentHandler);
    const auto replayed = dispatcher.dispatchIdempotentDocumentCommand(
        QStringLiteral("test.idempotent"), keyed, QByteArrayLiteral("payload"), idempotentHandler);
    const auto conflict = dispatcher.dispatchIdempotentDocumentCommand(
        QStringLiteral("test.idempotent"), keyed, QByteArrayLiteral("different"),
        idempotentHandler);
    ok &=
        expect(firstResult && replayed && replayed.get() == firstResult.get() && executions == 1 &&
                   !conflict &&
                   conflict.getError().code == Automation::AutomationErrorCode::IdempotencyConflict,
               "idempotent dispatcher commands must replay matches and reject conflicts");

    first.setLifecycleState(Automation::DocumentLifecycleState::Replacing);
    const auto busy = dispatcher.dispatchDocumentQuery<Automation::Revision>(
        QStringLiteral("test.query"), first.documentId(), [](Automation::DocumentSession &session) {
            return Automation::AutomationResult<Automation::Revision>(session.revision());
        });
    ok &= expect(!busy && busy.getError().code == Automation::AutomationErrorCode::Busy,
                 "non-active document generations must reject dispatcher access");
    first.setLifecycleState(Automation::DocumentLifecycleState::Active);

    const auto oldDocumentId = first.documentId();
    first.replaceGeneration({}, QStringLiteral("Replacement"));
    const auto oldGeneration = dispatcher.dispatchDocumentQuery<Automation::Revision>(
        QStringLiteral("test.query"), oldDocumentId, [](Automation::DocumentSession &session) {
            return Automation::AutomationResult<Automation::Revision>(session.revision());
        });
    ok &= expect(first.idempotencyStore().size() == 0 && !oldGeneration &&
                     oldGeneration.getError().code ==
                         Automation::AutomationErrorCode::DocumentChanged,
                 "generation replacement must clear idempotency state and invalidate the old ID");

    const auto invalidWindow = window.validateWindow(Automation::WindowId::create());
    ok &= expect(!invalidWindow && invalidWindow.getError().code ==
                                       Automation::AutomationErrorCode::HostCapabilityUnavailable,
                 "single-window context must reject an unrelated window ID");

    Automation::SingleWindowContext headlessWindow(std::nullopt);
    Automation::AutomationDispatcher headlessDispatcher(resolver, headlessWindow);
    ok &= expect(!headlessWindow.windowId(),
                 "headless window context must not create a null or sentinel window ID");
    const Automation::SingleWindowContext nullWindow(Automation::WindowId{});
    ok &= expect(!nullWindow.windowId(),
                 "a null UUID must be canonicalized to an absent window capability");

    int headlessGuiHandlerCalls = 0;
    const auto headlessGuiQuery = headlessDispatcher.dispatchGuiDocumentQuery<int>(
        QStringLiteral("test.gui_query"), Automation::DocumentId::create(),
        Automation::WindowId::create(), [&headlessGuiHandlerCalls](Automation::DocumentSession &) {
            ++headlessGuiHandlerCalls;
            return Automation::AutomationResult<int>(1);
        });
    ok &= expect(!headlessGuiQuery &&
                     headlessGuiQuery.getError().code ==
                         Automation::AutomationErrorCode::HostCapabilityUnavailable &&
                     headlessGuiQuery.getError().operationId == QStringLiteral("test.gui_query") &&
                     headlessGuiHandlerCalls == 0,
                 "headless GUI routes must reject host capability before document routing");

    int headlessApplicationHandlerCalls = 0;
    const auto headlessApplicationCommand =
        headlessDispatcher.dispatchApplicationCommand<Automation::ApplicationMutationResult>(
            QStringLiteral("test.application_command"),
            {.source = Automation::InvocationSource::Test},
            [&headlessApplicationHandlerCalls](const bool validateOnly) {
                ++headlessApplicationHandlerCalls;
                return Automation::AutomationResult<Automation::ApplicationMutationResult>({
                    .changed = true,
                    .validatedOnly = validateOnly,
                });
            });
    ok &= expect(headlessApplicationCommand && headlessApplicationHandlerCalls == 1,
                 "headless application commands must not depend on window routing");

    return ok ? 0 : 1;
}
