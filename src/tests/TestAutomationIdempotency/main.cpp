#include "Automation/AutomationDispatcher.h"
#include "Automation/CoreRuntime.h"
#include "Automation/OperationIds.h"
#include "Automation/ProjectAutomationDtos.h"
#include "TestRuntime.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QMetaObject>
#include <QTextStream>

#include <atomic>
#include <barrier>
#include <optional>
#include <thread>
#include <vector>

namespace {
    using MutationResult = Automation::AutomationResult<Automation::MutationResult>;

    bool expect(const bool condition, const QString &message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    Automation::CommandContext commandContext(const Automation::DocumentVersion &version,
                                              const QString &idempotencyKey = {},
                                              const bool validateOnly = false) {
        return {
            .expected = version,
            .validateOnly = validateOnly,
            .idempotencyKey = idempotencyKey,
            .source = Automation::InvocationSource::Test,
        };
    }

    Automation::CommandContext commandContext(const Automation::CoreRuntime &runtime,
                                              const QString &idempotencyKey = {},
                                              const bool validateOnly = false) {
        return commandContext(runtime.documentVersion(), idempotencyKey, validateOnly);
    }

    Automation::MutationResult successfulMutation(Automation::DocumentSession &session,
                                                  const bool validateOnly) {
        Automation::MutationResult result;
        result.previous = session.version();
        result.changed = true;
        result.validatedOnly = validateOnly;
        result.current = validateOnly ? session.version() : session.advanceRevision();
        return result;
    }

    Automation::AutomationDispatcher::DocumentCommandHandler countedHandler(int &executionCount) {
        return [&executionCount](Automation::DocumentSession &session, const bool validateOnly) {
            ++executionCount;
            return MutationResult(successfulMutation(session, validateOnly));
        };
    }

    bool serialReplayAndExplicitOptIn() {
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();
        const auto context =
            commandContext(runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000001"));
        int executions = 0;
        const auto handler = countedHandler(executions);

        const auto first = runtime.dispatcher().dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::insert, context, QByteArrayLiteral("alpha"), handler);
        const auto replay = runtime.dispatcher().dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::insert, context, QByteArrayLiteral("alpha"), handler);
        const auto changedInput = runtime.dispatcher().dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::insert, context, QByteArrayLiteral("beta"), handler);
        const auto changedOperation = runtime.dispatcher().dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::move, context, QByteArrayLiteral("alpha"), handler);

        int ordinaryExecutions = 0;
        const auto unsupported = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::move, context, countedHandler(ordinaryExecutions));
        const auto ordinary = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::move, commandContext(runtime),
            countedHandler(ordinaryExecutions));

        const auto isConflict = [](const MutationResult &result) {
            return !result &&
                   result.getError().code == Automation::AutomationErrorCode::IdempotencyConflict &&
                   result.getError().fieldPath == QStringLiteral("idempotency_key");
        };
        return expect(first && replay && first.get() == replay.get() && executions == 1,
                      QStringLiteral("opt-in replay must execute once")) &&
               expect(isConflict(changedInput) && isConflict(changedOperation),
                      QStringLiteral("a claimed key must reject changed input or operation")) &&
               expect(!unsupported &&
                          unsupported.getError().code ==
                              Automation::AutomationErrorCode::InvalidArgument &&
                          ordinaryExecutions == 1 && ordinary,
                      QStringLiteral(
                          "ordinary dispatch must reject a key and otherwise bypass the cache"));
    }

    bool completedReplayPrecedesWorkflowBusyAdmission() {
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();
        auto context =
            commandContext(runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000008"));
        context.source = Automation::InvocationSource::PublicMcp;
        int executions = 0;
        const auto handler = countedHandler(executions);
        const auto first = runtime.dispatcher().dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::insert, context, QByteArrayLiteral("alpha"), handler);

        runtime.setDocumentBusy(context.expected.documentId, true);
        const auto replay = runtime.dispatcher().dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::insert, context, QByteArrayLiteral("alpha"), handler);
        const auto conflict = runtime.dispatcher().dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::insert, context, QByteArrayLiteral("beta"), handler);
        auto newContext =
            commandContext(runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000009"));
        newContext.source = Automation::InvocationSource::PublicMcp;
        const auto blocked = runtime.dispatcher().dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::insert, newContext, QByteArrayLiteral("alpha"),
            handler);
        runtime.setDocumentBusy(context.expected.documentId, false);

        return expect(
                   first && replay && replay.get() == first.get() && executions == 1,
                   QStringLiteral("completed replay must remain available while workflow busy")) &&
               expect(!conflict && conflict.getError().code ==
                                       Automation::AutomationErrorCode::IdempotencyConflict,
                      QStringLiteral("claimed key conflicts must precede workflow busy")) &&
               expect(!blocked && blocked.getError().code == Automation::AutomationErrorCode::Busy,
                      QStringLiteral("workflow busy must still reject a new idempotent mutation"));
    }

    bool concurrentReplayExecutesOnce() {
        constexpr int LaneCount = 8;
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();
        const auto context =
            commandContext(runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000002"));
        std::atomic<int> executions = 0;
        Automation::AutomationDispatcher::DocumentCommandHandler handler =
            [&executions](Automation::DocumentSession &session, const bool validateOnly) {
                executions.fetch_add(1, std::memory_order_relaxed);
                return MutationResult(successfulMutation(session, validateOnly));
            };

        std::vector<std::optional<MutationResult>> results(LaneCount);
        std::barrier startGate(LaneCount + 1);
        std::atomic<int> completed = 0;
        QEventLoop eventLoop;
        std::vector<std::thread> workers;
        workers.reserve(LaneCount);
        for (int index = 0; index < LaneCount; ++index) {
            workers.emplace_back([&, index] {
                startGate.arrive_and_wait();
                results[static_cast<size_t>(index)].emplace(
                    runtime.dispatcher().dispatchIdempotentDocumentCommand(
                        Automation::OperationIds::tracks::insert, context,
                        QByteArrayLiteral("concurrent"), handler));
                if (completed.fetch_add(1, std::memory_order_acq_rel) + 1 == LaneCount)
                    QMetaObject::invokeMethod(&eventLoop, "quit", Qt::QueuedConnection);
            });
        }
        startGate.arrive_and_wait();
        eventLoop.exec();
        for (auto &worker : workers)
            worker.join();

        bool sameResult = results.front() && *results.front();
        if (sameResult) {
            const auto expected = results.front()->get();
            for (const auto &result : results)
                sameResult &= result && *result && result->get() == expected;
        }
        return expect(sameResult && executions.load(std::memory_order_relaxed) == 1 &&
                          runtime.documentVersion().revision == 1,
                      QStringLiteral("concurrent opt-in replays must serialize to one execution"));
    }

    bool unsuccessfulAttemptsDoNotClaimKeys() {
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();
        int executions = 0;
        const auto handler = countedHandler(executions);
        const auto key = QStringLiteral("d0d00000-0000-4000-8000-000000000003");

        const auto preview = runtime.dispatcher().dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::insert, commandContext(runtime, key, true),
            QByteArrayLiteral("preview"), handler);
        const auto committed = runtime.dispatcher().dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::insert, commandContext(runtime, key),
            QByteArrayLiteral("commit"), handler);

        const auto failedKey = QStringLiteral("d0d00000-0000-4000-8000-000000000004");
        int attempts = 0;
        Automation::AutomationDispatcher::DocumentCommandHandler failing =
            [&attempts](Automation::DocumentSession &, bool) {
                ++attempts;
                return MutationResult(Automation::AutomationError::invalidArgument(
                    QStringLiteral("request"), QStringLiteral("simulated failure")));
            };
        Automation::AutomationDispatcher::DocumentCommandHandler succeeding =
            [&attempts](Automation::DocumentSession &session, const bool validateOnly) {
                ++attempts;
                return MutationResult(successfulMutation(session, validateOnly));
            };
        const auto failed = runtime.dispatcher().dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::insert, commandContext(runtime, failedKey),
            QByteArrayLiteral("failed"), failing);
        const auto retried = runtime.dispatcher().dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::insert, commandContext(runtime, failedKey),
            QByteArrayLiteral("retry"), succeeding);

        return expect(preview && preview.get().validatedOnly && committed && executions == 2,
                      QStringLiteral("validate-only must not claim its key")) &&
               expect(!failed && retried && attempts == 2,
                      QStringLiteral("a failed handler must not claim its key"));
    }

    class TwoDocumentResolver final : public Automation::IDocumentSessionResolver {
    public:
        TwoDocumentResolver(Automation::DocumentSession &first, Automation::DocumentSession &second)
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

    bool documentsAndGenerationsHaveIndependentKeySpaces() {
        Automation::DocumentSession first(nullptr, nullptr);
        Automation::DocumentSession second(nullptr, nullptr);
        TwoDocumentResolver resolver(first, second);
        Automation::SingleWindowContext window;
        Automation::AutomationDispatcher dispatcher(resolver, window);
        const auto key = QStringLiteral("d0d00000-0000-4000-8000-000000000005");
        int executions = 0;
        const auto handler = countedHandler(executions);

        const auto firstResult = dispatcher.dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::insert, commandContext(first.version(), key),
            QByteArrayLiteral("shared"), handler);
        const auto secondResult = dispatcher.dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::insert, commandContext(second.version(), key),
            QByteArrayLiteral("shared"), handler);
        first.replaceGeneration({}, QStringLiteral("Replacement"));
        const auto reused = dispatcher.dispatchIdempotentDocumentCommand(
            Automation::OperationIds::tracks::insert, commandContext(first.version(), key),
            QByteArrayLiteral("shared"), handler);

        return expect(firstResult && secondResult && reused && executions == 3 &&
                          first.revision() == 1 && second.revision() == 1,
                      QStringLiteral("document and generation boundaries must isolate keys"));
    }

    bool facadeWiringUsesOptInOnly() {
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();
        Automation::TrackDraftDto draft;
        draft.clientRef = QStringLiteral("idempotent-track");
        draft.name = QStringLiteral("Track A");
        draft.gain = 1.0;
        const auto context =
            commandContext(runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000006"));
        const auto first = runtime.project().insertTrack(context, 0, draft);
        const auto replay = runtime.project().insertTrack(context, 0, draft);
        auto changedDraft = draft;
        changedDraft.name = QStringLiteral("Track B");
        const auto conflict = runtime.project().insertTrack(context, 0, changedDraft);
        if (!first || first.get().affectedObjects.isEmpty())
            return expect(false, QStringLiteral("track insertion fixture must succeed"));

        const Automation::TrackId trackId(first.get().affectedObjects.first().value);
        const auto keyedRename = runtime.project().renameTrack(
            commandContext(runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000007")),
            trackId, QStringLiteral("Renamed"));
        const auto rename = runtime.project().renameTrack(commandContext(runtime), trackId,
                                                          QStringLiteral("Renamed"));

        return expect(replay && replay.get() == first.get() && !conflict &&
                          conflict.getError().code ==
                              Automation::AutomationErrorCode::IdempotencyConflict,
                      QStringLiteral("the retained track creator must opt in to replay")) &&
               expect(!keyedRename &&
                          keyedRename.getError().code ==
                              Automation::AutomationErrorCode::InvalidArgument &&
                          rename && rename.get().changed,
                      QStringLiteral(
                          "ordinary edits must reject keys and remain callable without one"));
    }

    bool retentionIsBounded() {
        Automation::IdempotencyStore store;
        const auto operationId = Automation::OperationIds::tracks::insert;
        for (qsizetype index = 0; index <= Automation::IdempotencyStore::MaximumRetainedKeys;
             ++index) {
            const auto key = QString::number(index);
            const auto stored = store.store(operationId, key, QByteArrayLiteral("request"), index);
            if (!stored)
                return expect(false, QStringLiteral("bounded cache fixture must store entries"));
        }

        const auto oldest = store.replay<qsizetype>(operationId, QStringLiteral("0"),
                                                    QByteArrayLiteral("request"));
        const auto newestKey =
            QString::number(Automation::IdempotencyStore::MaximumRetainedKeys);
        const auto newest =
            store.replay<qsizetype>(operationId, newestKey, QByteArrayLiteral("request"));
        return expect(store.size() == Automation::IdempotencyStore::MaximumRetainedKeys && oldest &&
                          !oldest.get() && newest && newest.get() &&
                          *newest.get() == Automation::IdempotencyStore::MaximumRetainedKeys,
                      QStringLiteral("idempotency retention must evict the oldest completed key"));
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    bool ok = true;
    ok &= serialReplayAndExplicitOptIn();
    ok &= completedReplayPrecedesWorkflowBusyAdmission();
    ok &= concurrentReplayExecutesOnce();
    ok &= unsuccessfulAttemptsDoNotClaimKeys();
    ok &= documentsAndGenerationsHaveIndependentKeySpaces();
    ok &= facadeWiringUsesOptInOnly();
    ok &= retentionIsBounded();
    return ok ? 0 : 1;
}
