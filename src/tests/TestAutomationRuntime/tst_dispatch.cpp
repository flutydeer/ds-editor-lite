#include "tst_automation_runtime.h"

#include "Automation/AutomationDispatcher.h"
#include "Automation/CoreRuntime.h"

#include <QCoreApplication>
#include <QtTest>
#include <QTextStream>
#include <QTimer>

#include <functional>

namespace {


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

void AutomationRuntimeTests::documentRoutingAndErrorContext() {
    Automation::DocumentSession first(nullptr, nullptr);
    Automation::DocumentSession second(nullptr, nullptr);
    FakeResolver resolver(first, second);
    Automation::SingleWindowContext window;
    Automation::AutomationDispatcher dispatcher(resolver, window);
    QVERIFY2((window.windowId() && !window.windowId()->isNull()),
             "default single-window context must create a real window ID");

    const auto secondQuery = dispatcher.dispatchDocumentQuery<Automation::Revision>(
        QStringLiteral("test.query"), second.documentId(),
        [](Automation::DocumentSession &session) {
            return Automation::AutomationResult<Automation::Revision>(session.revision());
        });
    QVERIFY2((secondQuery && secondQuery.get() == 0),
             "dispatcher must route by explicit document ID");

    const auto unknownDocument = Automation::DocumentId::create();
    const auto missingQuery = dispatcher.dispatchDocumentQuery<Automation::Revision>(
        QStringLiteral("test.query"), unknownDocument, [](Automation::DocumentSession &session) {
            return Automation::AutomationResult<Automation::Revision>(session.revision());
        });
    QVERIFY2((!missingQuery &&
              missingQuery.getError().code == Automation::AutomationErrorCode::DocumentChanged &&
              missingQuery.getError().operationId == QStringLiteral("test.query")),
             "dispatcher must decorate document resolution failures with the operation ID");

    const auto handlerFailure = dispatcher.dispatchApplicationQuery<int>(
        QStringLiteral("test.failure"), []() -> Automation::AutomationResult<int> {
            return Automation::AutomationError::invalidArgument(QStringLiteral("value"),
                                                                QStringLiteral("invalid"));
        });
    QVERIFY2((!handlerFailure &&
              handlerFailure.getError().operationId == QStringLiteral("test.failure") &&
              handlerFailure.getError().fieldPath == QStringLiteral("value")),
             "dispatcher must preserve handler details and add operation context");
}

void AutomationRuntimeTests::validateCommitAndStaleRevision() {
    Automation::DocumentSession first(nullptr, nullptr);
    Automation::DocumentSession second(nullptr, nullptr);
    FakeResolver resolver(first, second);
    Automation::SingleWindowContext window;
    Automation::AutomationDispatcher dispatcher(resolver, window);
    Automation::CommandContext command;
    command.expected = first.version();
    command.validateOnly = true;
    command.source = Automation::InvocationSource::Test;
    const auto preview = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), command,
        [](Automation::DocumentSession &session, const bool validateOnly) {
            return commit(session, validateOnly);
        });
    QVERIFY2((preview && preview.get().validatedOnly && first.revision() == 0),
             "validate-only commands must execute without advancing revision");

    command.validateOnly = false;
    const auto committed = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), command,
        [](Automation::DocumentSession &session, const bool validateOnly) {
            return commit(session, validateOnly);
        });
    QVERIFY2((committed && committed.get().current.revision == 1 && first.revision() == 1),
             "document commands must commit against the expected revision");

    int staleHandlerCalls = 0;
    const auto stale = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), command,
        [&staleHandlerCalls](Automation::DocumentSession &session, const bool validateOnly) {
            ++staleHandlerCalls;
            return commit(session, validateOnly);
        });
    QVERIFY2((!stale &&
              stale.getError().code == Automation::AutomationErrorCode::RevisionConflict &&
              staleHandlerCalls == 0),
             "stale revisions must be rejected before entering the handler");
}

void AutomationRuntimeTests::publicBusyAdmissionAndContinuation_data() {
    QTest::addColumn<int>("source");
    QTest::newRow("mcp") << int(Automation::InvocationSource::PublicMcp);
    QTest::newRow("json-rpc") << int(Automation::InvocationSource::PublicJsonRpc);
}

void AutomationRuntimeTests::publicBusyAdmissionAndContinuation() {
    Automation::DocumentSession first(nullptr, nullptr);
    Automation::DocumentSession second(nullptr, nullptr);
    FakeResolver resolver(first, second);
    Automation::SingleWindowContext window;
    Automation::AutomationDispatcher dispatcher(resolver, window);
    QFETCH(int, source);
    Automation::CommandContext command{.expected = first.version()};
    auto admittedTaskCommand = command;
    admittedTaskCommand.expected = first.version();
    admittedTaskCommand.source = static_cast<Automation::InvocationSource>(source);
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
    publicCommand.source = static_cast<Automation::InvocationSource>(source);
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
    QVERIFY2(
        (admittedTaskBase &&
         admittedTaskCommand.source == Automation::InvocationSource::PublicTaskContinuation &&
         admittedTaskCompletion && !staleTaskCompletion &&
         staleTaskCompletion.getError().code == Automation::AutomationErrorCode::RevisionConflict &&
         queryWhileBusy && queryWhileBusy.get() == first.revision() && !publicValidationBusy &&
         publicValidationBusy.getError().code == Automation::AutomationErrorCode::Busy &&
         !rejectedTaskAdmission &&
         rejectedTaskAdmission.getError().code == Automation::AutomationErrorCode::Busy &&
         rejectedTaskCommand.source == static_cast<Automation::InvocationSource>(source) &&
         !publicBusy && publicBusy.getError().code == Automation::AutomationErrorCode::Busy &&
         !stalePublicBusy &&
         stalePublicBusy.getError().code == Automation::AutomationErrorCode::Busy &&
         publicHandlerCalls == 0 && allowedControl && !staleControl &&
         staleControl.getError().code == Automation::AutomationErrorCode::RevisionConflict &&
         allowedCancellation && trustedPreview && internalPreview),
        "workflow busy must reject new public mutations while allowing admitted tasks, "
        "document "
        "controls, queries, and trusted work");
    first.setBusy(false);
    publicCommand.expected = first.version();
    const auto publicValidation = dispatcher.validateDocumentCommand(publicCommand);
    ++publicCommand.expected.revision;
    const auto stalePublicValidation = dispatcher.validateDocumentCommand(publicCommand);
    QVERIFY2((publicValidation && publicValidation.get() == first.version() &&
              !stalePublicValidation &&
              stalePublicValidation.getError().code ==
                  Automation::AutomationErrorCode::RevisionConflict),
             "public async admission must share the dispatcher revision contract");
}

void AutomationRuntimeTests::closingRejectsMutations_data() {
    QTest::addColumn<int>("source");
    QTest::newRow("gui") << int(Automation::InvocationSource::TrustedGui);
    QTest::newRow("internal") << int(Automation::InvocationSource::InternalAutomation);
    QTest::newRow("mcp") << int(Automation::InvocationSource::PublicMcp);
    QTest::newRow("json-rpc") << int(Automation::InvocationSource::PublicJsonRpc);
    QTest::newRow("continuation") << int(Automation::InvocationSource::PublicTaskContinuation);
    QTest::newRow("test") << int(Automation::InvocationSource::Test);
}

void AutomationRuntimeTests::closingRejectsMutations() {
    using namespace Automation;
    QFETCH(int, source);
    CoreRuntime runtime(nullptr, nullptr);
    auto &dispatcher = runtime.dispatcher();
    const auto operation = QStringLiteral("test.closing");
    CommandContext replayContext{
        .expected = runtime.documentVersion(),
        .idempotencyKey = QStringLiteral("before-closing"),
        .source = static_cast<InvocationSource>(source),
    };
    const auto fingerprint = QByteArrayLiteral("closing-request");
    QVERIFY(dispatcher.dispatchIdempotentDocumentCommand(
        operation, replayContext, fingerprint,
        [](DocumentSession &session, bool) -> AutomationResult<MutationResult> {
            return MutationResult{.previous = session.version(), .current = session.version()};
        }));
    CommandContext admitted{
        .expected = runtime.documentVersion(),
        .source = InvocationSource::PublicMcp,
    };
    QVERIFY(dispatcher.admitDocumentTask(admitted));
    QCOMPARE(admitted.source, InvocationSource::PublicTaskContinuation);
    QVERIFY(runtime.setDocumentBusy(admitted.expected.documentId, true));

    QVERIFY(!runtime.shutdownCommitted());
    runtime.commitShutdown();
    runtime.commitShutdown();
    QVERIFY(runtime.shutdownCommitted());
    // Releasing the workflow lease must never reopen a committed shutdown.
    runtime.setDocumentBusy(admitted.expected.documentId, false);

    int handlerCalls = 0;
    const auto handler = [&](DocumentSession &session, const bool validateOnly) {
        ++handlerCalls;
        return commit(session, validateOnly);
    };
    const auto replay = dispatcher.dispatchIdempotentDocumentCommand(operation, replayContext,
                                                                     fingerprint, handler);
    QVERIFY(!replay);
    QCOMPARE(replay.getError().code, AutomationErrorCode::Busy);
    for (const bool validateOnly : {false, true}) {
        CommandContext context{
            .expected = runtime.documentVersion(),
            .validateOnly = validateOnly,
            .source = static_cast<InvocationSource>(source),
        };
        const auto validated = dispatcher.validateDocumentCommand(context);
        QVERIFY(!validated);
        QCOMPARE(validated.getError().code, AutomationErrorCode::Busy);
        const auto task = dispatcher.admitDocumentTask(context);
        QVERIFY(!task);
        QCOMPARE(task.getError().code, AutomationErrorCode::Busy);
        QCOMPARE(context.source, static_cast<InvocationSource>(source));
        const auto mutation = dispatcher.dispatchDocumentCommand(operation, context, handler);
        QVERIFY(!mutation);
        QCOMPARE(mutation.getError().code, AutomationErrorCode::Busy);
        QCOMPARE(mutation.getError().operationId, operation);
        const auto unchecked =
            dispatcher.dispatchDocumentCommandWithoutRevisionCheck(operation, context, handler);
        QVERIFY(!unchecked);
        QCOMPARE(unchecked.getError().code, AutomationErrorCode::Busy);
        const auto control = dispatcher.dispatchDocumentControlCommand(operation, context, handler);
        QVERIFY(!control);
        QCOMPARE(control.getError().code, AutomationErrorCode::Busy);
        const auto gui = dispatcher.dispatchGuiDocumentCommand<MutationResult>(
            operation,
            {.documentId = context.expected.documentId,
             .expectedRevision = context.expected.revision,
             .windowId = *runtime.windowId(),
             .validateOnly = validateOnly,
             .source = context.source},
            handler);
        QVERIFY(!gui);
        QCOMPARE(gui.getError().code, AutomationErrorCode::Busy);
        // GUI controllers construct their own context but still dispatch through the facade.
        const auto insertTrack = runtime.project().insertTrack(context, 0, {});
        QVERIFY(!insertTrack);
        QCOMPARE(insertTrack.getError().code, AutomationErrorCode::Busy);
        const auto undo = runtime.history().undo(context);
        QVERIFY(!undo);
        QCOMPARE(undo.getError().code, AutomationErrorCode::Busy);
    }
    const auto continuation = dispatcher.dispatchDocumentCommand(operation, admitted, handler);
    QVERIFY(!continuation);
    QCOMPARE(continuation.getError().code, AutomationErrorCode::Busy);
    const auto workflowContext = runtime.documentWorkflowCommitContext(admitted.expected);
    QVERIFY(!workflowContext);
    QCOMPARE(workflowContext.getError().code, AutomationErrorCode::Busy);
    const auto writebackContext = runtime.derivedWritebackContext(admitted.expected, false);
    QVERIFY(!writebackContext);
    QCOMPARE(writebackContext.getError().code, AutomationErrorCode::Busy);
    QCOMPARE(handlerCalls, 0);
    QCOMPARE(runtime.documentVersion(), admitted.expected);

    DocumentSession closingSession(nullptr, nullptr);
    closingSession.commitClosing();
    closingSession.setLifecycleState(DocumentLifecycleState::Replacing);
    QCOMPARE(closingSession.lifecycleState(), DocumentLifecycleState::Closing);
    closingSession.setLifecycleState(DocumentLifecycleState::Active);
    QCOMPARE(closingSession.lifecycleState(), DocumentLifecycleState::Closing);
    closingSession.replaceGeneration(QStringLiteral("late.dspx"), QStringLiteral("late"));
    QCOMPARE(closingSession.lifecycleState(), DocumentLifecycleState::Closing);
}

void AutomationRuntimeTests::generationReplacementInvalidatesRoutes() {
    Automation::DocumentSession first(nullptr, nullptr);
    Automation::DocumentSession second(nullptr, nullptr);
    FakeResolver resolver(first, second);
    Automation::SingleWindowContext window;
    Automation::AutomationDispatcher dispatcher(resolver, window);
    first.setLifecycleState(Automation::DocumentLifecycleState::Replacing);
    const auto busy = dispatcher.dispatchDocumentQuery<Automation::Revision>(
        QStringLiteral("test.query"), first.documentId(), [](Automation::DocumentSession &session) {
            return Automation::AutomationResult<Automation::Revision>(session.revision());
        });
    QVERIFY2((!busy && busy.getError().code == Automation::AutomationErrorCode::Busy),
             "non-active document generations must reject dispatcher access");
    first.setLifecycleState(Automation::DocumentLifecycleState::Active);

    const auto oldDocumentId = first.documentId();
    first.replaceGeneration({}, QStringLiteral("Replacement"));
    const auto oldGeneration = dispatcher.dispatchDocumentQuery<Automation::Revision>(
        QStringLiteral("test.query"), oldDocumentId, [](Automation::DocumentSession &session) {
            return Automation::AutomationResult<Automation::Revision>(session.revision());
        });
    QVERIFY2((!oldGeneration &&
              oldGeneration.getError().code == Automation::AutomationErrorCode::DocumentChanged),
             "generation replacement must clear idempotency state and invalidate the old ID");
}

void AutomationRuntimeTests::headlessCapabilityRouting() {
    Automation::DocumentSession first(nullptr, nullptr);
    Automation::DocumentSession second(nullptr, nullptr);
    FakeResolver resolver(first, second);
    Automation::SingleWindowContext window;
    Automation::AutomationDispatcher dispatcher(resolver, window);
    const auto invalidWindow = window.validateWindow(Automation::WindowId::create());
    QVERIFY2((!invalidWindow && invalidWindow.getError().code ==
                                    Automation::AutomationErrorCode::HostCapabilityUnavailable),
             "single-window context must reject an unrelated window ID");

    Automation::SingleWindowContext headlessWindow(std::nullopt);
    Automation::AutomationDispatcher headlessDispatcher(resolver, headlessWindow);
    QVERIFY2((!headlessWindow.windowId()),
             "headless window context must not create a null or sentinel window ID");
    const Automation::SingleWindowContext nullWindow(Automation::WindowId{});
    QVERIFY2((!nullWindow.windowId()),
             "a null UUID must be canonicalized to an absent window capability");

    int headlessGuiHandlerCalls = 0;
    const auto headlessGuiQuery = headlessDispatcher.dispatchGuiDocumentQuery<int>(
        QStringLiteral("test.gui_query"), Automation::DocumentId::create(),
        Automation::WindowId::create(), [&headlessGuiHandlerCalls](Automation::DocumentSession &) {
            ++headlessGuiHandlerCalls;
            return Automation::AutomationResult<int>(1);
        });
    QVERIFY2((!headlessGuiQuery &&
              headlessGuiQuery.getError().code ==
                  Automation::AutomationErrorCode::HostCapabilityUnavailable &&
              headlessGuiQuery.getError().operationId == QStringLiteral("test.gui_query") &&
              headlessGuiHandlerCalls == 0),
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
    QVERIFY2((headlessApplicationCommand && headlessApplicationHandlerCalls == 1),
             "headless application commands must not depend on window routing");
}

void AutomationRuntimeTests::invocationSourceCapture() {
    using namespace Automation;
    DocumentSession first(nullptr, nullptr);
    DocumentSession second(nullptr, nullptr);
    FakeResolver resolver(first, second);
    SingleWindowContext window;
    AutomationDispatcher dispatcher(resolver, window);
    bool ok = true;
    InvocationSource deferredSource = InvocationSource::TrustedGui;
    InvocationSource sourceAfterReturn = InvocationSource::InternalAutomation;
    for (const auto source : {InvocationSource::PublicMcp, InvocationSource::PublicJsonRpc}) {
        CommandContext context{.expected = first.version(), .source = source};
        const auto admitted = dispatcher.admitDocumentTask(context);
        ok &= QTest::qVerify(
            bool((admitted && context.source == InvocationSource::PublicTaskContinuation)),
            "invocation source", "admitted public tasks must retain their non-interactive source",
            __FILE__, __LINE__);
        const auto result = dispatcher.dispatchDocumentCommand(
            QStringLiteral("test.source"), context,
            [&](DocumentSession &session, const bool validateOnly) {
                const auto captured = dispatcher.currentInvocationSource();
                QTimer::singleShot(0, QCoreApplication::instance(), [&, captured] {
                    deferredSource = captured;
                    sourceAfterReturn = dispatcher.currentInvocationSource();
                });
                const auto nested = dispatcher.dispatchApplicationCommand<int>(
                    QStringLiteral("test.nested"), {.source = InvocationSource::InternalAutomation},
                    [&](bool) -> AutomationResult<int> {
                        ok &= QTest::qVerify(bool((dispatcher.currentInvocationSource() ==
                                                   InvocationSource::InternalAutomation)),
                                             "invocation source",
                                             "nested commands must expose their own source",
                                             __FILE__, __LINE__);
                        return AutomationError::invalidArgument(QStringLiteral("test"),
                                                                QStringLiteral("nested failure"));
                    });
                ok &= QTest::qVerify(
                    bool((!nested && dispatcher.currentInvocationSource() == captured)),
                    "invocation source", "failed nested commands must restore the outer source",
                    __FILE__, __LINE__);
                return commit(session, validateOnly);
            });
        QCoreApplication::processEvents();
        ok &= QTest::qVerify(
            bool((result && deferredSource == InvocationSource::PublicTaskContinuation &&
                  sourceAfterReturn == InvocationSource::TrustedGui)),
            "invocation source",
            "deferred work must retain its captured source after dispatch returns", __FILE__,
            __LINE__);
    }
    const auto gui = dispatcher.dispatchGuiCommand<int>(
        QStringLiteral("test.gui.source"),
        {.windowId = *window.windowId(), .source = InvocationSource::PublicMcp},
        [&](bool) -> AutomationResult<int> {
            return dispatcher.currentInvocationSource() == InvocationSource::PublicMcp;
        });
    const auto guiDocument = dispatcher.dispatchGuiDocumentCommand<int>(
        QStringLiteral("test.gui.document.source"),
        {.documentId = first.documentId(),
         .windowId = *window.windowId(),
         .source = InvocationSource::PublicJsonRpc},
        [&](DocumentSession &, bool) -> AutomationResult<int> {
            return dispatcher.currentInvocationSource() == InvocationSource::PublicJsonRpc;
        });
    ok &= QTest::qVerify(
        bool((gui && gui.get() && guiDocument && guiDocument.get() &&
              dispatcher.currentInvocationSource() == InvocationSource::TrustedGui)),
        "invocation source", "GUI commands must expose and restore the caller source", __FILE__,
        __LINE__);
    QVERIFY(ok);
}
