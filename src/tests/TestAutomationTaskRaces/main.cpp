#include "ControlledRaceSupport.h"
#include "TestRuntime.h"

#include "Automation/DocumentSession.h"

#include <QCoreApplication>
#include <QTextStream>

#include <algorithm>
#include <atomic>
#include <barrier>
#include <thread>

namespace {
    using namespace AutomationTaskRaceTests;

    class Checks final {
    public:
        void scenario(const char *name) {
            ++m_scenarios;
            m_currentScenario = QString::fromUtf8(name);
        }

        bool expect(const bool condition, const char *message, const int line) {
            ++m_assertions;
            if (condition)
                return true;
            ++m_failures;
            QTextStream(stderr) << "FAILED [" << m_currentScenario << ":" << line
                                << "]: " << message << Qt::endl;
            return false;
        }

        [[nodiscard]] bool passed() const {
            return m_failures == 0;
        }

        void printSummary() const {
            QTextStream(stdout) << "TestAutomationTaskRaces: " << m_scenarios << " scenarios, "
                                << m_assertions << " assertions, " << m_failures << " failures"
                                << Qt::endl;
        }

    private:
        QString m_currentScenario;
        int m_scenarios = 0;
        int m_assertions = 0;
        int m_failures = 0;
    };

#define EXPECT(checks, condition, message) (checks).expect((condition), (message), __LINE__)

    [[nodiscard]] bool isTerminal(const Automation::AutomationTaskState state) {
        return state == Automation::AutomationTaskState::Succeeded ||
               state == Automation::AutomationTaskState::Failed ||
               state == Automation::AutomationTaskState::Canceled;
    }

    [[nodiscard]] Automation::MutationResult
        successfulMutation(const Automation::DocumentVersion &base) {
        return {
            .previous = base,
            .current = {base.documentId, base.revision + 1},
            .changed = true,
        };
    }

    [[nodiscard]] bool allObserverStatesAre(const ObserverLog &log,
                                            const Automation::AutomationTaskState expected) {
        return !log.snapshots.isEmpty() &&
               std::all_of(log.snapshots.cbegin(), log.snapshots.cend(),
                           [expected](const auto &snapshot) { return snapshot.state == expected; });
    }

    void testTaskManagerStateBoundaries(Checks &checks) {
        checks.scenario("task manager state boundaries and stable terminal state");

        Automation::AutomationTaskManager tasks;
        const Automation::DocumentVersion base{Automation::DocumentId::create(), 7};
        int cancelCallbackCount = 0;
        const auto task = tasks.createTask(Automation::OperationIds::extract::pitch::start, base,
                                           Automation::ObjectRef{Automation::ObjectKind::Clip, 42},
                                           [&cancelCallbackCount] { ++cancelCallbackCount; });
        EXPECT(checks,
               task.operationId == Automation::OperationIds::extract::pitch::start &&
                   task.state == Automation::AutomationTaskState::Queued && task.cancelable,
               "createTask must expose a queued task under the centralized operation ID");
        EXPECT(checks, !tasks.succeed(task.taskId, successfulMutation(base)),
               "a queued task cannot complete without entering the commit point");
        EXPECT(checks, tasks.markRunning(task.taskId) && !tasks.markRunning(task.taskId),
               "Queued must enter Running exactly once");

        const auto firstCancel = tasks.requestCancel(base.documentId, task.taskId);
        const auto repeatedCancel = tasks.requestCancel(base.documentId, task.taskId);
        EXPECT(checks,
               firstCancel && repeatedCancel &&
                   firstCancel.get().state == Automation::AutomationTaskState::CancelRequested &&
                   repeatedCancel.get().state == Automation::AutomationTaskState::CancelRequested &&
                   cancelCallbackCount == 1,
               "Running cancellation must enter CancelRequested and invoke its callback once");

        const auto commitAfterCancel = tasks.beginCommitting(task.taskId);
        const auto canceled = tasks.get(base.documentId, task.taskId);
        EXPECT(checks,
               commitAfterCancel && !commitAfterCancel.get() && canceled &&
                   canceled.get().state == Automation::AutomationTaskState::Canceled &&
                   !canceled.get().cancelable && isTerminal(canceled.get().state),
               "CancelRequested must beat beginCommitting and end as Canceled");
        EXPECT(checks,
               !tasks.cancel(task.taskId) &&
                   !tasks.fail(task.taskId, Automation::AutomationError{}) &&
                   !tasks.succeed(task.taskId, successfulMutation(base)),
               "terminal tasks must reject every later terminal transition");
        const auto cancelAfterTerminal = tasks.requestCancel(base.documentId, task.taskId);
        EXPECT(checks,
               cancelAfterTerminal && cancelAfterTerminal.get() == canceled.get() &&
                   cancelCallbackCount == 1,
               "cancel-after-terminal must return the one stable terminal snapshot");

        const auto committingTask =
            tasks.createTask(Automation::OperationIds::extract::midi::start, base);
        EXPECT(checks, tasks.markRunning(committingTask.taskId), "second task must enter Running");
        const auto committing = tasks.beginCommitting(committingTask.taskId);
        const auto lateCancel = tasks.requestCancel(base.documentId, committingTask.taskId);
        EXPECT(checks,
               committing && committing.get() && !lateCancel &&
                   lateCancel.getError().code ==
                       Automation::AutomationErrorCode::OperationNotCancelable,
               "Committing is the non-cancelable boundary");
        const auto mutation = successfulMutation(base);
        EXPECT(checks, tasks.succeed(committingTask.taskId, mutation),
               "Committing must allow one successful terminal transition");
        EXPECT(checks,
               !tasks.succeed(committingTask.taskId, mutation) &&
                   !tasks.fail(committingTask.taskId, Automation::AutomationError{}) &&
                   !tasks.cancel(committingTask.taskId),
               "duplicate completion callbacks must not create another terminal transition");
        const auto succeeded = tasks.get(base.documentId, committingTask.taskId);
        EXPECT(checks,
               succeeded && succeeded.get().state == Automation::AutomationTaskState::Succeeded &&
                   succeeded.get().mutation == mutation,
               "the first successful terminal payload must remain authoritative");

        const auto unknownTaskId = Automation::TaskId::create();
        const auto unknownGet = tasks.get(base.documentId, unknownTaskId);
        const auto unknownCancel = tasks.requestCancel(base.documentId, unknownTaskId);
        const auto unknownCommit = tasks.beginCommitting(unknownTaskId);
        EXPECT(checks,
               !unknownGet && !unknownCancel && !unknownCommit &&
                   unknownGet.getError().code == Automation::AutomationErrorCode::NotFound &&
                   unknownCancel.getError().code == Automation::AutomationErrorCode::NotFound &&
                   unknownCommit.getError().code == Automation::AutomationErrorCode::NotFound,
               "unknown TaskId must fail consistently on query, cancel, and commit entry");
    }

    void testApplicationTaskScope(Checks &checks) {
        checks.scenario("application tasks remain independent from document generations");

        Automation::AutomationTaskManager tasks;
        const auto document = Automation::DocumentVersion{Automation::DocumentId::create(), 4};
        int cancelCount = 0;
        const auto canceledTask = tasks.createApplicationTask(
            QStringLiteral("packages.refresh"), [&cancelCount] { ++cancelCount; },
            QStringLiteral("test-client"));
        EXPECT(checks,
               canceledTask.scope == Automation::AutomationTaskScope::Application &&
                   canceledTask.baseDocument.documentId.isNull() &&
                   tasks.listApplication().size() == 1 && tasks.list(document.documentId).isEmpty(),
               "application tasks must expose an explicit scope without a fake document");
        EXPECT(checks,
               !tasks.get(document.documentId, canceledTask.taskId) &&
                   tasks.getApplication(canceledTask.taskId),
               "document and application task lookup must remain scope-safe");
        const auto firstCancel = tasks.requestCancelApplication(canceledTask.taskId);
        const auto repeatedCancel = tasks.requestCancelApplication(canceledTask.taskId);
        const auto canceledCommit = tasks.beginCommitting(canceledTask.taskId);
        EXPECT(checks,
               firstCancel && repeatedCancel && canceledCommit && !canceledCommit.get() &&
                   cancelCount == 1 &&
                   tasks.getApplication(canceledTask.taskId).get().state ==
                       Automation::AutomationTaskState::Canceled,
               "application cancellation must be idempotent and win before commit");

        const auto succeededTask = tasks.createApplicationTask(QStringLiteral("packages.refresh"));
        const auto running = tasks.markRunning(succeededTask.taskId);
        const auto committing = tasks.beginCommitting(succeededTask.taskId);
        EXPECT(checks,
               running && committing && committing.get() &&
                   tasks.succeedApplication(
                       succeededTask.taskId,
                       QJsonObject{
                           {QStringLiteral("added"),   2},
                           {QStringLiteral("removed"), 1}
        }),
               "application tasks must support one successful application result");
        tasks.discardDocumentGeneration(document.documentId);
        const auto succeeded = tasks.getApplication(succeededTask.taskId);
        EXPECT(checks,
               succeeded && succeeded.get().state == Automation::AutomationTaskState::Succeeded &&
                   succeeded.get().applicationResult &&
                   succeeded.get().applicationResult->value(QStringLiteral("added")).toInt() == 2,
               "document generation cleanup must not discard application task results");

        Automation::AutomationTaskManager retainedTasks;
        const auto active = retainedTasks.createApplicationTask(QStringLiteral("packages.refresh"));
        EXPECT(checks, retainedTasks.markRunning(active.taskId),
               "the retained-history fixture must keep one active application task");
        QList<Automation::TaskId> terminalIds;
        for (qsizetype index = 0;
             index <= Automation::AutomationTaskManager::MaximumRetainedApplicationTasks; ++index) {
            const auto terminal =
                retainedTasks.createApplicationTask(QStringLiteral("packages.refresh"));
            terminalIds.append(terminal.taskId);
            const auto enteredCommit = retainedTasks.beginCommitting(terminal.taskId);
            EXPECT(checks,
                   enteredCommit && enteredCommit.get() &&
                       retainedTasks.succeedApplication(
                           terminal.taskId,
                           QJsonObject{
                               {QStringLiteral("sequence"), static_cast<qint64>(index)}
            }),
                   "application history fixture tasks must reach a terminal state");
        }
        EXPECT(checks,
               retainedTasks.listApplication().size() ==
                       Automation::AutomationTaskManager::MaximumRetainedApplicationTasks + 1 &&
                   !retainedTasks.getApplication(terminalIds.first()) &&
                   retainedTasks.getApplication(terminalIds.last()) &&
                   retainedTasks.getApplication(active.taskId),
               "terminal application history must be bounded without evicting active tasks");
    }

    void testDocumentTaskRetention(Checks &checks) {
        checks.scenario("document terminal task history remains bounded per generation");

        Automation::AutomationTaskManager tasks;
        const auto document = Automation::DocumentVersion{Automation::DocumentId::create(), 4};
        const auto otherDocument = Automation::DocumentVersion{Automation::DocumentId::create(), 2};
        const auto active = tasks.createTask(Automation::OperationIds::inference::start, document);
        EXPECT(checks, tasks.markRunning(active.taskId),
               "the document retention fixture must keep one active task");

        const auto other = tasks.createTask(Automation::OperationIds::inference::start, otherDocument);
        const auto otherCommit = tasks.beginCommitting(other.taskId);
        EXPECT(checks,
               otherCommit && otherCommit.get() &&
                   tasks.succeed(other.taskId,
                                 {.previous = otherDocument, .current = otherDocument}),
               "a second document generation must retain its own terminal history");

        QList<Automation::TaskId> terminalIds;
        for (qsizetype index = 0;
             index <= Automation::AutomationTaskManager::MaximumRetainedDocumentTasks; ++index) {
            const auto terminal =
                tasks.createTask(Automation::OperationIds::inference::start, document);
            terminalIds.append(terminal.taskId);
            const auto enteredCommit = tasks.beginCommitting(terminal.taskId);
            EXPECT(checks,
                   enteredCommit && enteredCommit.get() &&
                       tasks.succeed(terminal.taskId,
                                     {.previous = document, .current = document}),
                   "document history fixture tasks must reach a terminal state");
        }
        EXPECT(checks,
               tasks.list(document.documentId).size() ==
                       Automation::AutomationTaskManager::MaximumRetainedDocumentTasks + 1 &&
                   !tasks.get(document.documentId, terminalIds.first()) &&
                   tasks.get(document.documentId, terminalIds.last()) &&
                   tasks.get(document.documentId, active.taskId) &&
                   tasks.get(otherDocument.documentId, other.taskId),
               "terminal document history must be bounded per generation without evicting active "
               "or unrelated tasks");
    }

    struct CancelRaceOutcome {
        bool succeeded = false;
        Automation::AutomationTaskState state = Automation::AutomationTaskState::Queued;
        Automation::AutomationErrorCode error = Automation::AutomationErrorCode::InternalError;
    };

    struct CommitRaceOutcome {
        bool returned = false;
        bool mayCommit = false;
        Automation::AutomationErrorCode error = Automation::AutomationErrorCode::InternalError;
    };

    void testCancelVersusCommitStress(Checks &checks) {
        checks.scenario("cancel versus commit-point barrier stress");

        constexpr int iterations = 256;
        int cancelWins = 0;
        int commitWins = 0;
        for (int iteration = 0; iteration < iterations; ++iteration) {
            Automation::AutomationTaskManager tasks;
            const Automation::DocumentVersion base{Automation::DocumentId::create(),
                                                   static_cast<Automation::Revision>(iteration)};
            std::atomic_int cancelCallbackCount = 0;
            const auto task = tasks.createTask(
                Automation::OperationIds::extract::pitch::start, base, std::nullopt,
                [&cancelCallbackCount] { cancelCallbackCount.fetch_add(1); });
            EXPECT(checks, tasks.markRunning(task.taskId),
                   "stress task must enter Running before the barrier");

            std::barrier<> barrier(3);
            CancelRaceOutcome cancelOutcome;
            CommitRaceOutcome commitOutcome;
            std::jthread cancelThread([&] {
                barrier.arrive_and_wait();
                const auto result = tasks.requestCancel(base.documentId, task.taskId);
                if (result) {
                    cancelOutcome.succeeded = true;
                    cancelOutcome.state = result.get().state;
                } else {
                    cancelOutcome.error = result.getError().code;
                }
            });
            std::jthread commitThread([&] {
                barrier.arrive_and_wait();
                const auto result = tasks.beginCommitting(task.taskId);
                if (result) {
                    commitOutcome.returned = true;
                    commitOutcome.mayCommit = result.get();
                } else {
                    commitOutcome.error = result.getError().code;
                }
            });
            barrier.arrive_and_wait();
            cancelThread.join();
            commitThread.join();

            if (commitOutcome.returned && commitOutcome.mayCommit) {
                ++commitWins;
                EXPECT(checks,
                       !cancelOutcome.succeeded &&
                           cancelOutcome.error ==
                               Automation::AutomationErrorCode::OperationNotCancelable &&
                           cancelCallbackCount.load() == 0,
                       "a commit-point winner must reject the racing cancellation");
                EXPECT(checks, tasks.succeed(task.taskId, successfulMutation(base)),
                       "the commit winner must own the only terminal transition");
            } else {
                ++cancelWins;
                EXPECT(
                    checks,
                    commitOutcome.returned && !commitOutcome.mayCommit && cancelOutcome.succeeded &&
                        cancelOutcome.state == Automation::AutomationTaskState::CancelRequested &&
                        cancelCallbackCount.load() == 1,
                    "a cancel winner must stop commit entry and invoke cancellation once");
                const auto canceledBeforeFinalCheck = tasks.get(base.documentId, task.taskId);
                EXPECT(checks,
                       canceledBeforeFinalCheck && canceledBeforeFinalCheck.get().state ==
                                                       Automation::AutomationTaskState::Canceled,
                       "beginCommitting must acknowledge the winning cancel as Canceled");
            }

            const auto terminal = tasks.get(base.documentId, task.taskId);
            EXPECT(
                checks,
                terminal && isTerminal(terminal.get().state) &&
                    ((commitOutcome.mayCommit &&
                      terminal.get().state == Automation::AutomationTaskState::Succeeded) ||
                     (!commitOutcome.mayCommit &&
                      terminal.get().state == Automation::AutomationTaskState::Canceled)),
                "each cancel/commit race must end in exactly one branch-specific terminal state");
            EXPECT(checks,
                   !tasks.succeed(task.taskId, successfulMutation(base)) &&
                       !tasks.cancel(task.taskId),
                   "the terminal state must be immutable after every stress iteration");
        }
        EXPECT(checks, cancelWins + commitWins == iterations,
               "every stress iteration must select exactly one race winner");
    }

    void testDuplicateCompletionStress(Checks &checks) {
        checks.scenario("duplicate completion callback barrier stress");

        constexpr int iterations = 128;
        for (int iteration = 0; iteration < iterations; ++iteration) {
            Automation::AutomationTaskManager tasks;
            const Automation::DocumentVersion base{Automation::DocumentId::create(), 3};
            const auto task =
                tasks.createTask(Automation::OperationIds::extract::pitch::start, base);
            EXPECT(checks, tasks.markRunning(task.taskId),
                   "duplicate-callback task must enter Running");

            std::barrier<> barrier(3);
            std::atomic_int commitCount = 0;
            std::atomic_int terminalCount = 0;
            const auto complete = [&] {
                barrier.arrive_and_wait();
                const auto committing = tasks.beginCommitting(task.taskId);
                if (!committing || !committing.get())
                    return;
                commitCount.fetch_add(1);
                if (tasks.succeed(task.taskId, successfulMutation(base)))
                    terminalCount.fetch_add(1);
            };
            std::jthread first(complete);
            std::jthread duplicate(complete);
            barrier.arrive_and_wait();
            first.join();
            duplicate.join();

            const auto terminal = tasks.get(base.documentId, task.taskId);
            EXPECT(checks,
                   commitCount.load() == 1 && terminalCount.load() == 1 && terminal &&
                       terminal.get().state == Automation::AutomationTaskState::Succeeded,
                   "racing duplicate callbacks must commit and terminalize at most once");
            EXPECT(checks,
                   !tasks.fail(task.taskId, Automation::AutomationError{}) &&
                       !tasks.cancel(task.taskId),
                   "a duplicate callback cannot replace the successful terminal result");
        }
    }

    void testSessionGenerationContract(Checks &checks) {
        checks.scenario("DocumentSession generation rotation and old TaskId contract");

        Automation::DocumentSession session(nullptr, nullptr);
        Automation::AutomationTaskManager tasks;
        const auto oldVersion = session.version();
        int cancelCallbacks = 0;
        int unsuccessfulCallbacks = 0;
        int terminalCallbacks = 0;
        std::optional<Automation::AutomationTaskSnapshot> removedSnapshot;
        const auto task = tasks.createTask(Automation::OperationIds::extract::pitch::start,
                                           oldVersion, std::nullopt, [&] { ++cancelCallbacks; });
        tasks.setUnsuccessfulCallback(task.taskId, [&](const auto &snapshot) {
            ++unsuccessfulCallbacks;
            removedSnapshot = snapshot;
        });
        tasks.setTerminalCallback(task.taskId, [&](const auto &snapshot) {
            ++terminalCallbacks;
            removedSnapshot = snapshot;
        });
        const auto newVersion = session.replaceGeneration({}, {});
        EXPECT(checks, newVersion.documentId != oldVersion.documentId && newVersion.revision == 0,
               "replaceGeneration must rotate DocumentId and reset revision");
        const auto crossGenerationLookup = tasks.get(newVersion.documentId, task.taskId);
        EXPECT(checks,
               !crossGenerationLookup && crossGenerationLookup.getError().code ==
                                             Automation::AutomationErrorCode::DocumentChanged,
               "an old TaskId cannot be reinterpreted in a replacement generation");
        tasks.discardDocumentGeneration(oldVersion.documentId);
        const auto discardedLookup = tasks.get(oldVersion.documentId, task.taskId);
        EXPECT(checks,
               !discardedLookup &&
                   discardedLookup.getError().code == Automation::AutomationErrorCode::NotFound,
               "discarding a generation must remove its old TaskId records");
        EXPECT(checks,
               cancelCallbacks == 1 && unsuccessfulCallbacks == 1 && terminalCallbacks == 1 &&
                   removedSnapshot &&
                   removedSnapshot->state == Automation::AutomationTaskState::Canceled &&
                   !removedSnapshot->cancelable,
               "discarding a generation must terminalize removed work outside the task store");

        const auto replacementTask = tasks.createTask(QStringLiteral("documents.open"), newVersion,
                                                      std::nullopt, [&] { ++cancelCallbacks; });
        tasks.setUnsuccessfulCallback(replacementTask.taskId,
                                      [&](const auto &) { ++unsuccessfulCallbacks; });
        tasks.setTerminalCallback(replacementTask.taskId,
                                  [&](const auto &) { ++terminalCallbacks; });
        const auto finalVersion = session.replaceGeneration({}, {});
        tasks.replaceDocumentGeneration(newVersion.documentId, finalVersion);
        EXPECT(checks,
               cancelCallbacks == 2 && unsuccessfulCallbacks == 2 && terminalCallbacks == 2 &&
                   tasks.size() == 0,
               "replacing a generation must terminalize every non-preserved task exactly once");
    }

    // Extraction callbacks do not expose a production hook between their validation and commit
    // calls. This capability test composes the same public dispatcher/task-manager protocol and
    // puts a controlled scheduler barrier at that otherwise-unobservable boundary.
    void testControlledDispatcherCommitBoundary(Checks &checks) {
        checks.scenario("dispatcher final revision recheck after validation barrier");
        {
            AutomationTestSupport::TestRuntime fixture;
            auto &runtime = fixture.runtime();
            const auto base = runtime.documentVersion();
            const auto task = runtime.automationTasks().createTask(
                Automation::OperationIds::extract::pitch::start, base);
            EXPECT(checks, runtime.automationTasks().markRunning(task.taskId),
                   "contract task must enter Running");

            Automation::CommandContext validationContext{
                .expected = base,
                .validateOnly = true,
                .source = Automation::InvocationSource::Test,
            };
            const auto validation = runtime.timeline().setTempo(validationContext, 960, 141.0);
            QStringList phaseOrder{QStringLiteral("validated")};
            EXPECT(checks, validation && validation.get().validatedOnly,
                   "the asynchronous payload must validate against its captured base revision");

            ManualScheduler scheduler;
            Automation::AutomationResult<Automation::MutationResult> interveningEdit(
                Automation::AutomationError{});
            scheduler.schedule([&] {
                Automation::CommandContext editContext{
                    .expected = runtime.documentVersion(),
                    .source = Automation::InvocationSource::Test,
                };
                interveningEdit = runtime.timeline().setTempo(editContext, 480, 132.0);
                phaseOrder.append(QStringLiteral("revision advanced"));
            });
            EXPECT(checks, scheduler.runNext() && interveningEdit,
                   "the controlled barrier must advance revision after validation");

            const auto committing = runtime.automationTasks().beginCommitting(task.taskId);
            phaseOrder.append(QStringLiteral("committing"));
            EXPECT(checks, committing && committing.get(),
                   "the task must reach Committing after the controlled barrier");
            const auto lateCancel =
                runtime.tasks().cancelTask({.expected = runtime.documentVersion(),
                                            .source = Automation::InvocationSource::Test},
                                           task.taskId);
            phaseOrder.append(QStringLiteral("late cancel rejected"));
            EXPECT(checks,
                   !lateCancel && lateCancel.getError().code ==
                                      Automation::AutomationErrorCode::OperationNotCancelable,
                   "cancel must remain rejected after the commit point even if revision advanced");

            Automation::CommandContext commitContext{
                .expected = base,
                .source = Automation::InvocationSource::Test,
            };
            const auto staleCommit = runtime.timeline().setTempo(commitContext, 960, 141.0);
            phaseOrder.append(QStringLiteral("commit rechecked"));
            EXPECT(checks,
                   !staleCommit &&
                       staleCommit.getError().code ==
                           Automation::AutomationErrorCode::RevisionConflict &&
                       staleCommit.getError().expectedRevision == base.revision &&
                       staleCommit.getError().actualRevision == base.revision + 1,
                   "the real dispatcher must recheck revision at final commit, after validation");
            auto taskError = staleCommit.getError();
            taskError.taskId = task.taskId;
            EXPECT(checks, runtime.automationTasks().fail(task.taskId, std::move(taskError)),
                   "a rejected final commit must produce one Failed terminal state");
            const auto terminal = runtime.automationTasks().get(base.documentId, task.taskId);
            EXPECT(checks,
                   terminal && terminal.get().state == Automation::AutomationTaskState::Failed &&
                       terminal.get().error &&
                       terminal.get().error->code ==
                           Automation::AutomationErrorCode::RevisionConflict &&
                       runtime.documentVersion().revision == base.revision + 1,
                   "revision rejection must not add a second commit");
            const QStringList expectedPhaseOrder{
                QStringLiteral("validated"), QStringLiteral("revision advanced"),
                QStringLiteral("committing"), QStringLiteral("late cancel rejected"),
                QStringLiteral("commit rechecked")};
            EXPECT(checks, phaseOrder == expectedPhaseOrder,
                   "the controlled phases must prove the final revision recheck order");
        }

        checks.scenario("non-cancelable commit point and one dispatcher commit");
        {
            AutomationTestSupport::TestRuntime fixture;
            auto &runtime = fixture.runtime();
            const auto base = runtime.documentVersion();
            const auto task = runtime.automationTasks().createTask(
                Automation::OperationIds::extract::pitch::start, base);
            EXPECT(checks, runtime.automationTasks().markRunning(task.taskId),
                   "successful contract task must enter Running");
            Automation::CommandContext validationContext{
                .expected = base,
                .validateOnly = true,
                .source = Automation::InvocationSource::Test,
            };
            const auto validation = runtime.timeline().setTempo(validationContext, 960, 142.0);
            const auto committing = runtime.automationTasks().beginCommitting(task.taskId);
            const auto lateCancel = runtime.tasks().cancelTask(
                {.expected = base, .source = Automation::InvocationSource::Test}, task.taskId);
            EXPECT(checks,
                   validation && committing && committing.get() && !lateCancel &&
                       lateCancel.getError().code ==
                           Automation::AutomationErrorCode::OperationNotCancelable,
                   "the successful path must validate, enter Committing, and reject late cancel");

            int commitAttempts = 0;
            ++commitAttempts;
            Automation::CommandContext commitContext{
                .expected = base,
                .source = Automation::InvocationSource::Test,
            };
            const auto committed = runtime.timeline().setTempo(commitContext, 960, 142.0);
            const bool terminalized =
                committed && runtime.automationTasks().succeed(task.taskId, committed.get());
            EXPECT(checks,
                   terminalized && commitAttempts == 1 &&
                       runtime.documentVersion().revision == base.revision + 1,
                   "the real dispatcher mutation must commit exactly once");
            EXPECT(
                checks,
                !runtime.automationTasks().succeed(task.taskId, committed.get()) &&
                    !runtime.automationTasks().fail(task.taskId, Automation::AutomationError{}) &&
                    !runtime.automationTasks().cancel(task.taskId),
                "duplicate completion paths must be rejected after the successful terminal state");
            const auto stableCancel =
                runtime.tasks().cancelTask({.expected = runtime.documentVersion(),
                                            .source = Automation::InvocationSource::Test},
                                           task.taskId);
            EXPECT(checks,
                   stableCancel &&
                       stableCancel.get().state == Automation::AutomationTaskState::Succeeded,
                   "cancel-after-success must return the stable successful snapshot");
        }

        checks.scenario("public task cancellation while document workflow is busy");
        {
            AutomationTestSupport::TestRuntime fixture;
            auto &runtime = fixture.runtime();
            const auto base = runtime.documentVersion();
            int cancelCallbackCount = 0;
            const auto task = runtime.automationTasks().createTask(
                Automation::OperationIds::extract::pitch::start, base, std::nullopt,
                [&cancelCallbackCount] { ++cancelCallbackCount; });
            const bool running = runtime.automationTasks().markRunning(task.taskId);
            const bool busy = runtime.setDocumentBusy(base.documentId, true);
            const auto canceled = runtime.tasks().cancelTask(
                {.expected = base, .source = Automation::InvocationSource::PublicMcp}, task.taskId);
            runtime.setDocumentBusy(base.documentId, false);
            EXPECT(checks,
                   running && busy && canceled &&
                       canceled.get().state == Automation::AutomationTaskState::CancelRequested &&
                       cancelCallbackCount == 1,
                   "workflow busy must not prevent a public client from canceling its task");
        }
    }

    void testCommittingGenerationReplacement(Checks &checks) {
        checks.scenario("generation replacement supersedes a task at the commit point");

        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();
        const auto base = runtime.documentVersion();
        int cancelCallbackCount = 0;
        const auto task = runtime.automationTasks().createTask(
            Automation::OperationIds::extract::pitch::start, base, std::nullopt,
            [&cancelCallbackCount] { ++cancelCallbackCount; });
        const bool running = runtime.automationTasks().markRunning(task.taskId);
        const auto committing = runtime.automationTasks().beginCommitting(task.taskId);
        EXPECT(checks, running && committing && committing.get(),
               "generation replacement scenario must stop at Committing");

        Automation::CommandContext replaceContext{
            .expected = base,
            .source = Automation::InvocationSource::Test,
        };
        const auto replacement = runtime.documents().commitNewDocument(
            replaceContext, RaceFixture::emptyDocumentDraft());
        const auto replacementVersion = runtime.documentVersion();
        EXPECT(checks,
               replacement && replacementVersion.documentId != base.documentId &&
                   replacementVersion.revision == 0 && runtime.automationTasks().size() == 0,
               "commit_new must discard even a Committing record when it replaces the generation");
        EXPECT(checks,
               cancelCallbackCount == 0 &&
                   !runtime.automationTasks().succeed(task.taskId, successfulMutation(base)),
               "generation discard must not request normal cancellation past the commit point, and "
               "a stale completion must be inert");

        const auto oldLookup = runtime.tasks().getTask(base.documentId, task.taskId);
        const auto newLookup = runtime.tasks().getTask(replacementVersion.documentId, task.taskId);
        const auto oldCancel = runtime.tasks().cancelTask(replaceContext, task.taskId);
        const auto newCancel = runtime.tasks().cancelTask(
            {.expected = replacementVersion, .source = Automation::InvocationSource::Test},
            task.taskId);
        EXPECT(checks,
               !oldLookup &&
                   oldLookup.getError().code == Automation::AutomationErrorCode::DocumentChanged &&
                   !oldCancel &&
                   oldCancel.getError().code == Automation::AutomationErrorCode::DocumentChanged &&
                   !newLookup &&
                   newLookup.getError().code == Automation::AutomationErrorCode::NotFound &&
                   !newCancel &&
                   newCancel.getError().code == Automation::AutomationErrorCode::NotFound,
               "old and unknown TaskId errors must stay stable for both query and cancel after "
               "replacement");
    }

    void testQueuedAndRunningCancellation(Checks &checks) {
        checks.scenario("queued cancel before fake scheduler release");
        {
            RaceFixture fixture;
            EXPECT(checks, fixture.isReady(),
                   "race fixture must create typed audio and singing clips");
            ObserverLog observer;
            const auto accepted = fixture.startPitch(observer.observer());
            EXPECT(checks,
                   accepted && fixture.scheduler().pendingCount() == 1 &&
                       fixture.pitchStates().size() == 1,
                   "pitch extraction must remain Queued until the fake scheduler releases it");
            if (!accepted || fixture.pitchStates().isEmpty())
                return;
            const auto state = fixture.pitchStates().last();
            const auto canceled =
                fixture.runtime().tasks().cancelTask(fixture.context(), accepted.get().taskId);
            const auto repeated =
                fixture.runtime().tasks().cancelTask(fixture.context(), accepted.get().taskId);
            EXPECT(checks,
                   canceled && repeated &&
                       canceled.get().state == Automation::AutomationTaskState::CancelRequested &&
                       repeated.get().state == Automation::AutomationTaskState::CancelRequested &&
                       state->cancelCount == 1,
                   "queued cancellation must be idempotent before scheduler release");
            EXPECT(checks, fixture.scheduler().runNext(),
                   "the controlled scheduler must release the queued closure");
            const auto terminal = fixture.runtime().tasks().getTask(
                fixture.runtime().documentVersion().documentId, accepted.get().taskId);
            EXPECT(
                checks,
                terminal && terminal.get().state == Automation::AutomationTaskState::Canceled &&
                    state->startCount == 0 &&
                    allObserverStatesAre(observer, Automation::AutomationTaskState::Canceled),
                "queued cancellation must prevent backend start and publish one terminal branch");
        }

        checks.scenario("running cancel before backend completion");
        {
            RaceFixture fixture;
            EXPECT(checks, fixture.isReady(), "race fixture must initialize");
            ObserverLog observer;
            const auto base = fixture.runtime().documentVersion();
            const auto accepted = fixture.startPitch(observer.observer());
            EXPECT(checks, accepted && fixture.scheduler().runNext(),
                   "fake scheduler must move pitch extraction from Queued to Running");
            if (!accepted || fixture.pitchStates().isEmpty())
                return;
            const auto state = fixture.pitchStates().last();
            const auto running =
                fixture.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(checks,
                   running && running.get().state == Automation::AutomationTaskState::Running &&
                       state->startCount == 1,
                   "released extraction must expose Running before completion");
            const auto cancel =
                fixture.runtime().tasks().cancelTask(fixture.context(), accepted.get().taskId);
            const auto repeatedCancel =
                fixture.runtime().tasks().cancelTask(fixture.context(), accepted.get().taskId);
            state->complete(RaceFixture::successfulPitch());
            state->complete(RaceFixture::successfulPitch());
            const auto terminal =
                fixture.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(checks,
                   cancel && repeatedCancel && terminal &&
                       terminal.get().state == Automation::AutomationTaskState::Canceled &&
                       state->cancelCount == 1 && fixture.runtime().documentVersion() == base &&
                       allObserverStatesAre(observer, Automation::AutomationTaskState::Canceled),
                   "CancelRequested must beat success, stay terminal under duplicate completion, "
                   "and commit zero times");
        }
    }

    void testCompletionPermutations(Checks &checks) {
        checks.scenario("successful completion before cancel and duplicate callback");
        {
            RaceFixture fixture;
            EXPECT(checks, fixture.isReady(), "race fixture must initialize");
            ObserverLog observer;
            const auto base = fixture.runtime().documentVersion();
            const auto accepted = fixture.startPitch(observer.observer());
            EXPECT(checks, accepted && fixture.scheduler().runNext(),
                   "completion scenario must reach Running");
            if (!accepted || fixture.pitchStates().isEmpty())
                return;
            const auto state = fixture.pitchStates().last();
            state->complete(RaceFixture::successfulPitch());
            const auto firstTerminal =
                fixture.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            const auto lateCancel =
                fixture.runtime().tasks().cancelTask(fixture.context(), accepted.get().taskId);
            state->complete(RaceFixture::successfulPitch());
            state->complete(RaceFixture::successfulPitch());
            const auto stableTerminal =
                fixture.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            const auto records = fixture.runtime().automationTasks().list(base.documentId);
            EXPECT(checks,
                   firstTerminal && stableTerminal && lateCancel &&
                       firstTerminal.get().state == Automation::AutomationTaskState::Succeeded &&
                       stableTerminal.get() == firstTerminal.get() &&
                       lateCancel.get().state == Automation::AutomationTaskState::Succeeded &&
                       fixture.runtime().documentVersion().revision == base.revision + 1 &&
                       records.size() == 1 &&
                       allObserverStatesAre(observer, Automation::AutomationTaskState::Succeeded),
                   "success, late cancel, and duplicate callbacks must retain one task and one "
                   "commit");
        }

        checks.scenario("cancel plus revision advance before completion");
        {
            RaceFixture fixture;
            EXPECT(checks, fixture.isReady(), "race fixture must initialize");
            const auto base = fixture.runtime().documentVersion();
            const auto accepted = fixture.startPitch();
            EXPECT(checks, accepted && fixture.scheduler().runNext(),
                   "cancel/revision scenario must reach Running");
            if (!accepted || fixture.pitchStates().isEmpty())
                return;
            const auto state = fixture.pitchStates().last();
            const auto cancel =
                fixture.runtime().tasks().cancelTask(fixture.context(), accepted.get().taskId);
            const auto edit = fixture.runtime().timeline().setTempo(fixture.context(), 480, 133.0);
            state->complete(RaceFixture::successfulPitch());
            const auto terminal =
                fixture.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(checks,
                   cancel && edit && terminal &&
                       terminal.get().state == Automation::AutomationTaskState::Canceled &&
                       !terminal.get().error && state->cancelCount == 1 &&
                       fixture.runtime().documentVersion().revision == base.revision + 1,
                   "a pre-completion cancel must win over the later revision conflict and add no "
                   "commit");
        }
    }

    void testRevisionAndObjectDeletion(Checks &checks) {
        checks.scenario("revision advance before extraction completion");
        {
            RaceFixture fixture;
            EXPECT(checks, fixture.isReady(), "race fixture must initialize");
            ObserverLog observer;
            const auto base = fixture.runtime().documentVersion();
            const auto accepted = fixture.startPitch(observer.observer());
            EXPECT(checks, accepted && fixture.scheduler().runNext(),
                   "revision race must reach Running");
            if (!accepted || fixture.pitchStates().isEmpty())
                return;
            const auto edit = fixture.runtime().timeline().setTempo(fixture.context(), 480, 134.0);
            fixture.pitchStates().last()->complete(RaceFixture::successfulPitch());
            const auto terminal =
                fixture.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(checks,
                   edit && terminal &&
                       terminal.get().state == Automation::AutomationTaskState::Failed &&
                       terminal.get().error &&
                       terminal.get().error->code ==
                           Automation::AutomationErrorCode::RevisionConflict &&
                       terminal.get().error->expectedRevision == base.revision &&
                       terminal.get().error->actualRevision == base.revision + 1 &&
                       fixture.runtime().documentVersion().revision == base.revision + 1 &&
                       allObserverStatesAre(observer, Automation::AutomationTaskState::Failed),
                   "completion must recheck the captured revision and commit zero times after an "
                   "edit");
        }

        checks.scenario("target deletion before extraction completion");
        {
            RaceFixture fixture;
            EXPECT(checks, fixture.isReady(), "race fixture must initialize");
            const auto base = fixture.runtime().documentVersion();
            const auto accepted = fixture.startPitch();
            EXPECT(checks, accepted && fixture.scheduler().runNext(),
                   "object deletion race must reach Running");
            if (!accepted || fixture.pitchStates().isEmpty())
                return;
            const auto removed = fixture.runtime().project().removeClips(fixture.context(),
                                                                         {fixture.singingClipId()});
            fixture.pitchStates().last()->complete(RaceFixture::successfulPitch());
            const auto terminal =
                fixture.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(checks,
                   removed && removed.get().changed && terminal &&
                       terminal.get().state == Automation::AutomationTaskState::Failed &&
                       terminal.get().error &&
                       terminal.get().error->code ==
                           Automation::AutomationErrorCode::RevisionConflict &&
                       !terminal.get().error->object &&
                       fixture.runtime().documentVersion().revision == base.revision + 1,
                   "public deletion must advance revision, so final revision validation precedes "
                   "stale object lookup");
        }
    }

    void testNewAndOpenGenerationReplacement(Checks &checks) {
        checks.scenario("new generation replaces a queued extraction");
        {
            RaceFixture fixture;
            EXPECT(checks, fixture.isReady(), "race fixture must initialize");
            ObserverLog observer;
            const auto accepted = fixture.startPitch(observer.observer());
            EXPECT(checks, accepted && fixture.scheduler().pendingCount() == 1,
                   "new-generation scenario must retain queued work");
            if (!accepted || fixture.pitchStates().isEmpty())
                return;
            const auto oldVersion = fixture.runtime().documentVersion();
            const auto state = fixture.pitchStates().last();
            const auto replacement = fixture.runtime().documents().commitNewDocument(
                fixture.context(), RaceFixture::emptyDocumentDraft());
            const auto newVersion = fixture.runtime().documentVersion();
            EXPECT(checks,
                   replacement && newVersion.documentId != oldVersion.documentId &&
                       newVersion.revision == 0 && state->cancelCount == 1 &&
                       fixture.runtime().automationTasks().size() == 0,
                   "commit_new must rotate generation, cancel queued work, and discard old tasks");
            EXPECT(checks, fixture.scheduler().runNext() && state->startCount == 0,
                   "a stale queued closure must become inert after generation replacement");
            const auto oldLookup =
                fixture.runtime().tasks().getTask(oldVersion.documentId, accepted.get().taskId);
            const auto newLookup =
                fixture.runtime().tasks().getTask(newVersion.documentId, accepted.get().taskId);
            EXPECT(checks,
                   !oldLookup &&
                       oldLookup.getError().code ==
                           Automation::AutomationErrorCode::DocumentChanged &&
                       !newLookup &&
                       newLookup.getError().code == Automation::AutomationErrorCode::NotFound &&
                       observer.snapshots.isEmpty(),
                   "old TaskId must be inaccessible from both old and replacement document "
                   "identities");
        }

        checks.scenario("open generation replaces a running extraction");
        {
            RaceFixture fixture;
            EXPECT(checks, fixture.isReady(), "race fixture must initialize");
            ObserverLog observer;
            const auto accepted = fixture.startPitch(observer.observer());
            EXPECT(checks, accepted && fixture.scheduler().runNext(),
                   "open-generation scenario must reach Running");
            if (!accepted || fixture.pitchStates().isEmpty())
                return;
            const auto oldVersion = fixture.runtime().documentVersion();
            const auto state = fixture.pitchStates().last();
            const auto replacement = fixture.runtime().documents().commitOpenedDocument(
                fixture.context(), RaceFixture::emptyDocumentDraft(),
                QStringLiteral("replacement.dspx"), QStringLiteral("replacement.dspx"), true);
            const auto replacementVersion = fixture.runtime().documentVersion();
            EXPECT(checks,
                   replacement && state->startCount == 1 && state->cancelCount == 1 &&
                       replacementVersion.documentId != oldVersion.documentId &&
                       replacementVersion.revision == 0 &&
                       fixture.runtime().automationTasks().size() == 0,
                   "commit_open must cancel running work and discard its task generation");

            state->complete(RaceFixture::successfulPitch());
            state->complete(RaceFixture::successfulPitch());
            const auto oldLookup =
                fixture.runtime().tasks().getTask(oldVersion.documentId, accepted.get().taskId);
            const auto newLookup = fixture.runtime().tasks().getTask(replacementVersion.documentId,
                                                                     accepted.get().taskId);
            EXPECT(checks,
                   fixture.runtime().documentVersion() == replacementVersion && !oldLookup &&
                       oldLookup.getError().code ==
                           Automation::AutomationErrorCode::DocumentChanged &&
                       !newLookup &&
                       newLookup.getError().code == Automation::AutomationErrorCode::NotFound &&
                       observer.snapshots.isEmpty(),
                   "late and duplicate callbacks must not write into the opened generation");
        }
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    Checks checks;

    testTaskManagerStateBoundaries(checks);
    testApplicationTaskScope(checks);
    testDocumentTaskRetention(checks);
    testCancelVersusCommitStress(checks);
    testDuplicateCompletionStress(checks);
    testSessionGenerationContract(checks);
    testControlledDispatcherCommitBoundary(checks);
    testCommittingGenerationReplacement(checks);
    testQueuedAndRunningCancellation(checks);
    testCompletionPermutations(checks);
    testRevisionAndObjectDeletion(checks);
    testNewAndOpenGenerationReplacement(checks);

    checks.printSummary();
    return checks.passed() ? 0 : 1;
}
