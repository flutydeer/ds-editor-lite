#include "AsyncFileDomainSupport.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <functional>
#include <iterator>
#include <optional>

namespace {
    using namespace AutomationAsyncFileTests;

    class Matrix final {
    public:
        template <typename Function>
        void run(const Automation::OperationId &operationId, const char *scenarioId,
                 Function &&function) {
            run(operationId, QString::fromLatin1(scenarioId), std::forward<Function>(function));
        }

        template <typename Function>
        void run(const Automation::OperationId &operationId, const QString &scenarioId,
                 Function &&function) {
            m_current = scenarioId;
            m_currentOperation = operationId;
            ++m_scenarios;
            ++m_counts[operationId];
            if (m_scenarioIds.contains(scenarioId)) {
                ++m_failures;
                QTextStream(stderr)
                    << "FAILED [coverage]: duplicate scenario ID " << scenarioId << Qt::endl;
            }
            m_scenarioIds.insert(scenarioId);
            std::forward<Function>(function)();
        }

        void expect(const bool condition, const QString &message, const int line) {
            ++m_assertions;
            if (condition)
                return;
            ++m_failures;
            QTextStream(stderr) << "FAILED [" << m_current << ':' << line << "]: " << message
                                << Qt::endl;
        }

        void requireAtLeast(const Automation::OperationId &operationId, const int count) {
            if (m_counts.value(operationId) >= count)
                return;
            ++m_failures;
            QTextStream(stderr) << "FAILED [coverage]: " << operationId << " has "
                                << m_counts.value(operationId) << " scenarios; expected at least "
                                << count << Qt::endl;
        }

        void requireBetween(const Automation::OperationId &operationId, const int minimum,
                            const int maximum) {
            const auto count = m_counts.value(operationId);
            if (count >= minimum && count <= maximum)
                return;
            ++m_failures;
            QTextStream(stderr) << "FAILED [coverage-range]: " << operationId << " has " << count
                                << " scenarios; expected " << minimum << ".." << maximum
                                << Qt::endl;
        }

        void cover(const char *dimension) {
            const auto name = QString::fromLatin1(dimension);
            ++m_dimensions[name];
            ++m_operationDimensions[m_currentOperation][name];
        }

        void requireDimension(const char *dimension, const int count) {
            const auto name = QString::fromLatin1(dimension);
            if (m_dimensions.value(name) >= count)
                return;
            ++m_failures;
            QTextStream(stderr) << "FAILED [dimension]: " << name << " has "
                                << m_dimensions.value(name) << " scenarios; expected at least "
                                << count << Qt::endl;
        }

        void requireAsyncDimension(const Automation::OperationId &operationId,
                                   const char *dimension, const int count = 1) {
            const auto name = QString::fromLatin1(dimension);
            if (m_operationDimensions.value(operationId).value(name) >= count)
                return;
            ++m_failures;
            QTextStream(stderr) << "FAILED [async-dimension]: " << operationId << '/' << name
                                << " has " << m_operationDimensions.value(operationId).value(name)
                                << " scenarios; expected at least " << count << Qt::endl;
        }

        void requireOperationsExactly(QList<Automation::OperationId> expected) {
            std::sort(expected.begin(), expected.end());
            QStringList duplicates;
            for (qsizetype index = 1; index < expected.size(); ++index) {
                if (expected.at(index) == expected.at(index - 1) &&
                    (duplicates.isEmpty() || duplicates.last() != expected.at(index))) {
                    duplicates.append(expected.at(index));
                }
            }
            expected.erase(std::unique(expected.begin(), expected.end()), expected.end());

            auto actual = m_counts.keys();
            std::sort(actual.begin(), actual.end());
            QStringList missing;
            QStringList extra;
            std::set_difference(expected.cbegin(), expected.cend(), actual.cbegin(), actual.cend(),
                                std::back_inserter(missing));
            std::set_difference(actual.cbegin(), actual.cend(), expected.cbegin(), expected.cend(),
                                std::back_inserter(extra));
            if (duplicates.isEmpty() && missing.isEmpty() && extra.isEmpty())
                return;
            ++m_failures;
            QTextStream(stderr) << "FAILED [operation-set]: expected=" << expected.size()
                                << " actual=" << actual.size()
                                << " duplicate_expected=" << duplicates.join(QLatin1Char(','))
                                << " missing=" << missing.join(QLatin1Char(','))
                                << " extra=" << extra.join(QLatin1Char(',')) << Qt::endl;
        }

        [[nodiscard]] int finish() const {
            QTextStream output(stdout);
            output << "TestAutomationAsyncDimensions: " << m_scenarios << " scenarios, "
                   << m_assertions << " assertions, " << m_failures << " failures" << Qt::endl;
            auto operations = m_counts.keys();
            std::sort(operations.begin(), operations.end());
            int minimum = 0;
            int maximum = 0;
            if (!operations.isEmpty()) {
                minimum = m_counts.value(operations.first());
                maximum = minimum;
                for (const auto &operationId : operations) {
                    minimum = std::min(minimum, m_counts.value(operationId));
                    maximum = std::max(maximum, m_counts.value(operationId));
                }
            }
            output << "Operation coverage: " << operations.size() << " operations, min=" << minimum
                   << ", max=" << maximum << Qt::endl;
            for (const auto &operationId : operations)
                output << "  " << operationId << ": " << m_counts.value(operationId) << Qt::endl;
            auto dimensions = m_dimensions.keys();
            std::sort(dimensions.begin(), dimensions.end());
            for (const auto &dimension : dimensions)
                output << "  [" << dimension << "]: " << m_dimensions.value(dimension) << Qt::endl;
            auto dimensionOperations = m_operationDimensions.keys();
            std::sort(dimensionOperations.begin(), dimensionOperations.end());
            for (const auto &operationId : dimensionOperations) {
                output << "  [state " << operationId << ']';
                for (const auto &dimension : dimensions) {
                    output << ' ' << dimension << '='
                           << m_operationDimensions.value(operationId).value(dimension);
                }
                output << Qt::endl;
            }
            return m_failures == 0 ? 0 : 1;
        }

    private:
        QString m_current;
        Automation::OperationId m_currentOperation;
        QHash<Automation::OperationId, int> m_counts;
        QHash<QString, int> m_dimensions;
        QHash<Automation::OperationId, QHash<QString, int>> m_operationDimensions;
        QSet<QString> m_scenarioIds;
        int m_scenarios = 0;
        int m_assertions = 0;
        int m_failures = 0;
    };

#define EXPECT(matrix, condition, message) (matrix).expect((condition), (message), __LINE__)

    [[nodiscard]] Automation::CommandContext keyedContext(const RuntimeHarness &harness,
                                                          const QString &key,
                                                          const bool validateOnly = false) {
        auto context = harness.context(validateOnly);
        context.idempotencyKey = key;
        return context;
    }

    [[nodiscard]] bool sameHistory(const Automation::HistoryStateDto &left,
                                   const Automation::HistoryStateDto &right) {
        return left.document == right.document && left.canUndo == right.canUndo &&
               left.canRedo == right.canRedo && left.onSavePoint == right.onSavePoint &&
               left.undoName == right.undoName && left.redoName == right.redoName;
    }

    [[nodiscard]] bool sameHistoryStack(const Automation::HistoryStateDto &left,
                                        const Automation::HistoryStateDto &right) {
        return left.canUndo == right.canUndo && left.canRedo == right.canRedo &&
               left.onSavePoint == right.onSavePoint && left.undoName == right.undoName &&
               left.redoName == right.redoName;
    }

    [[nodiscard]] Automation::MidiExtractionBackendResult successfulMidi(const int key = 64) {
        return {
            .state = Automation::ExtractionBackendState::Succeeded,
            .notes = {{.keyIndex = key, .localStart = 12, .length = 240}},
        };
    }

    [[nodiscard]] Automation::PitchExtractionBackendResult
        successfulPitch(const double value = 61.5) {
        return {
            .state = Automation::ExtractionBackendState::Succeeded,
            .segments = {{.globalStartTick = 24, .values = {value, value + 0.25}}},
        };
    }

    void testMidiExtraction(Matrix &matrix) {
        const auto operationId = Automation::OperationIds::extract::midi::start;

        matrix.run(operationId, "AFD-EXT-MIDI-001-VALIDATE-SNAPSHOT", [&] {
            RuntimeHarness harness;
            EXPECT(matrix, harness.isReady(), QStringLiteral("fixture must initialize"));
            const auto base = harness.runtime().documentVersion();
            const auto taskCount = harness.runtime().automationTasks().size();
            const auto result = harness.runtime().extractions().startMidi(harness.context(true),
                                                                          harness.audioClipId());
            EXPECT(matrix,
                   result && result.get().validatedOnly && result.get().taskId.isNull() &&
                       result.get().document == base &&
                       harness.runtime().automationTasks().size() == taskCount &&
                       harness.extractionScheduler.pendingCount() == 0 &&
                       harness.midiPrepareCount == 1 && harness.midiStates.size() == 1 &&
                       harness.midiStates.first()->startCount == 0 &&
                       harness.midiStates.first()->input.audioClipId == harness.audioClipId() &&
                       harness.midiStates.first()->input.audioPath == QStringLiteral("source.wav"),
                   QStringLiteral("validate-only must capture immutable input without a task"));
        });

        matrix.run(operationId, "AFD-EXT-MIDI-002-IDEMPOTENT-TASK-ID", [&] {
            matrix.cover("Queued");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto context =
                keyedContext(harness, QStringLiteral("afda0000-0000-4000-8000-000000000001"));
            const auto first =
                harness.runtime().extractions().startMidi(context, harness.audioClipId());
            const auto replay =
                harness.runtime().extractions().startMidi(context, harness.audioClipId());
            EXPECT(matrix,
                   first && replay && replay.get() == first.get() &&
                       harness.midiPrepareCount == 1 &&
                       harness.extractionScheduler.pendingCount() == 1 &&
                       harness.runtime().automationTasks().size() == 1,
                   QStringLiteral("an accepted replay must keep one TaskId and backend job"));
            if (!first)
                return;
            harness.extractionScheduler.runNext();
            harness.midiStates.first()->complete(successfulMidi());
            const auto terminalReplay =
                harness.runtime().extractions().startMidi(context, harness.audioClipId());
            EXPECT(matrix,
                   terminalReplay && terminalReplay.get() == first.get() &&
                       harness.midiPrepareCount == 1,
                   QStringLiteral("terminal replay must retain the accepted TaskId"));
        });

        matrix.run(operationId, "AFD-EXT-MIDI-003-QUEUED-CANCEL", [&] {
            matrix.cover("Queued");
            matrix.cover("CancelRequested");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto tracksBefore = harness.model().tracks().size();
            int finishedCount = 0;
            int destroyCountAtFinished = -1;
            Automation::ExtractionObserver observer;
            observer.finished = [&](const Automation::AutomationTaskSnapshot &) {
                ++finishedCount;
                if (!harness.midiStates.isEmpty())
                    destroyCountAtFinished = harness.midiStates.last()->destroyCount;
            };
            const auto accepted = harness.runtime().extractions().startMidi(
                harness.context(), harness.audioClipId(), std::move(observer));
            EXPECT(matrix, bool(accepted), QStringLiteral("task must be accepted"));
            if (!accepted)
                return;
            const auto firstCancel =
                harness.runtime().tasks().cancelTask(harness.context(), accepted.get().taskId);
            const auto repeatedCancel =
                harness.runtime().tasks().cancelTask(harness.context(), accepted.get().taskId);
            const auto released = harness.extractionScheduler.runNext();
            const auto terminal =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(
                matrix,
                firstCancel && repeatedCancel && released && terminal &&
                    firstCancel.get().state == Automation::AutomationTaskState::CancelRequested &&
                    repeatedCancel.get().state ==
                        Automation::AutomationTaskState::CancelRequested &&
                    terminal.get().state == Automation::AutomationTaskState::Canceled &&
                    harness.midiStates.first()->cancelCount == 1 &&
                    harness.midiStates.first()->startCount == 0 && finishedCount == 1 &&
                    destroyCountAtFinished == 0 && harness.midiStates.first()->destroyCount == 1 &&
                    harness.runtime().documentVersion() == base &&
                    harness.model().tracks().size() == tracksBefore,
                QStringLiteral("queued cancel must be idempotent and prevent backend start"));
        });

        matrix.run(operationId, "AFD-EXT-MIDI-004-RUNNING-CANCEL-DUPLICATE", [&] {
            matrix.cover("Running");
            matrix.cover("CancelRequested");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto tracksBefore = harness.model().tracks().size();
            int finishedCount = 0;
            int destroyCountAtFinished = -1;
            Automation::ExtractionObserver observer;
            observer.finished = [&](const Automation::AutomationTaskSnapshot &) {
                ++finishedCount;
                if (!harness.midiStates.isEmpty())
                    destroyCountAtFinished = harness.midiStates.last()->destroyCount;
            };
            const auto accepted = harness.runtime().extractions().startMidi(
                harness.context(), harness.audioClipId(), std::move(observer));
            EXPECT(matrix, accepted && harness.extractionScheduler.runNext(),
                   QStringLiteral("task must reach Running"));
            if (!accepted || harness.midiStates.isEmpty())
                return;
            const auto state = harness.midiStates.last();
            const auto running =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            const auto canceled =
                harness.runtime().tasks().cancelTask(harness.context(), accepted.get().taskId);
            const auto repeated =
                harness.runtime().tasks().cancelTask(harness.context(), accepted.get().taskId);
            state->complete(successfulMidi());
            state->complete(successfulMidi(67));
            const auto terminal =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(
                matrix,
                running && running.get().state == Automation::AutomationTaskState::Running &&
                    canceled && repeated && terminal &&
                    terminal.get().state == Automation::AutomationTaskState::Canceled &&
                    state->cancelCount == 1 && state->destroyCount == 1 && finishedCount == 1 &&
                    destroyCountAtFinished == 0 && harness.runtime().documentVersion() == base &&
                    harness.model().tracks().size() == tracksBefore,
                QStringLiteral("running cancel must beat duplicate callbacks and release its job"));
        });

        matrix.run(operationId, "AFD-EXT-MIDI-005-SUCCESS-ONCE", [&] {
            matrix.cover("Running");
            matrix.cover("Committing");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto tracksBefore = harness.model().tracks().size();
            int finishedCount = 0;
            int destroyCountAtFinished = -1;
            Automation::ExtractionObserver observer;
            observer.finished = [&](const Automation::AutomationTaskSnapshot &) {
                ++finishedCount;
                if (!harness.midiStates.isEmpty())
                    destroyCountAtFinished = harness.midiStates.last()->destroyCount;
            };
            const auto accepted = harness.runtime().extractions().startMidi(
                harness.context(), harness.audioClipId(), std::move(observer));
            EXPECT(matrix, accepted && harness.extractionScheduler.runNext(),
                   QStringLiteral("task must reach Running"));
            if (!accepted || harness.midiStates.isEmpty())
                return;
            const auto state = harness.midiStates.last();
            state->complete(successfulMidi());
            const auto firstTerminal =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            state->complete(successfulMidi(69));
            const auto stableTerminal =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            const auto lateCancel =
                harness.runtime().tasks().cancelTask(harness.context(), accepted.get().taskId);
            EXPECT(matrix,
                   firstTerminal && stableTerminal && lateCancel &&
                       firstTerminal.get().state == Automation::AutomationTaskState::Succeeded &&
                       stableTerminal.get() == firstTerminal.get() &&
                       lateCancel.get() == firstTerminal.get() && finishedCount == 1 &&
                       destroyCountAtFinished == 0 && state->destroyCount == 1 &&
                       harness.runtime().documentVersion().revision == base.revision + 1 &&
                       harness.model().tracks().size() == tracksBefore + 1,
                   QStringLiteral(
                       "success and duplicate completion must commit once and release the job"));
        });

        matrix.run(operationId, "AFD-EXT-MIDI-006-BACKEND-FAILURE-RETRY", [&] {
            matrix.cover("Running");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto key = QStringLiteral("afda0000-0000-4000-8000-000000000002");
            const auto context = keyedContext(harness, key);
            const auto first =
                harness.runtime().extractions().startMidi(context, harness.audioClipId());
            EXPECT(matrix, first && harness.extractionScheduler.runNext(),
                   QStringLiteral("failing task must run"));
            if (!first || harness.midiStates.isEmpty())
                return;
            const auto state = harness.midiStates.last();
            state->complete({
                .state = Automation::ExtractionBackendState::Failed,
                .errorCode = Automation::AutomationErrorCode::InferenceError,
                .errorMessage = QStringLiteral("controlled MIDI failure"),
            });
            const auto failed = harness.runtime().tasks().getTask(
                harness.runtime().documentVersion().documentId, first.get().taskId);
            const auto retried =
                harness.runtime().extractions().startMidi(context, harness.audioClipId());
            EXPECT(matrix,
                   failed && failed.get().state == Automation::AutomationTaskState::Failed &&
                       failed.get().error &&
                       failed.get().error->code ==
                           Automation::AutomationErrorCode::InferenceError &&
                       state->destroyCount == 1 && retried &&
                       retried.get().taskId != first.get().taskId &&
                       harness.extractionScheduler.pendingCount() == 1,
                   QStringLiteral("backend failure must release both its job and idempotency key"));
        });

        matrix.run(operationId, "AFD-EXT-MIDI-007-REVISION-WINS", [&] {
            matrix.cover("Running");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto tracksBefore = harness.model().tracks().size();
            const auto accepted =
                harness.runtime().extractions().startMidi(harness.context(), harness.audioClipId());
            EXPECT(matrix, accepted && harness.extractionScheduler.runNext(),
                   QStringLiteral("task must reach Running"));
            if (!accepted || harness.midiStates.isEmpty())
                return;
            const auto edit = harness.runtime().timeline().setTempo(harness.context(), 960, 137.0);
            harness.midiStates.last()->complete(successfulMidi());
            const auto terminal =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(matrix,
                   edit && terminal &&
                       terminal.get().state == Automation::AutomationTaskState::Failed &&
                       terminal.get().error &&
                       terminal.get().error->code ==
                           Automation::AutomationErrorCode::RevisionConflict &&
                       harness.model().tracks().size() == tracksBefore &&
                       harness.runtime().documentVersion().revision == base.revision + 1,
                   QStringLiteral("revision conflict must precede async track insertion"));
        });

        matrix.run(operationId, "AFD-EXT-MIDI-008-SOURCE-DELETION", [&] {
            matrix.cover("Running");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto accepted =
                harness.runtime().extractions().startMidi(harness.context(), harness.audioClipId());
            EXPECT(matrix, accepted && harness.extractionScheduler.runNext(),
                   QStringLiteral("task must reach Running"));
            if (!accepted || harness.midiStates.isEmpty())
                return;
            const auto removed =
                harness.runtime().project().removeClips(harness.context(), {harness.audioClipId()});
            harness.midiStates.last()->complete(successfulMidi());
            const auto terminal =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(matrix,
                   removed && terminal &&
                       terminal.get().state == Automation::AutomationTaskState::Failed &&
                       terminal.get().error &&
                       terminal.get().error->code ==
                           Automation::AutomationErrorCode::RevisionConflict &&
                       !terminal.get().error->object,
                   QStringLiteral("public deletion must surface revision conflict before objects"));
        });

        matrix.run(operationId, "AFD-EXT-MIDI-009-GENERATION-LATE-CALLBACK", [&] {
            matrix.cover("Running");
            matrix.cover("CancelRequested");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto oldVersion = harness.runtime().documentVersion();
            const auto accepted =
                harness.runtime().extractions().startMidi(harness.context(), harness.audioClipId());
            EXPECT(matrix, accepted && harness.extractionScheduler.runNext(),
                   QStringLiteral("task must reach Running"));
            if (!accepted || harness.midiStates.isEmpty())
                return;
            const auto state = harness.midiStates.last();
            const auto replacement = harness.runtime().documents().commitOpenedDocument(
                harness.context(), RuntimeHarness::emptyDocument(),
                QStringLiteral("replacement-midi.dspx"), QStringLiteral("replacement-midi.dspx"),
                true);
            const auto newVersion = harness.runtime().documentVersion();
            state->complete(successfulMidi());
            state->complete(successfulMidi(71));
            const auto oldLookup =
                harness.runtime().tasks().getTask(oldVersion.documentId, accepted.get().taskId);
            const auto newLookup =
                harness.runtime().tasks().getTask(newVersion.documentId, accepted.get().taskId);
            EXPECT(matrix,
                   replacement && newVersion.documentId != oldVersion.documentId &&
                       newVersion.revision == 0 && state->cancelCount == 1 &&
                       harness.runtime().automationTasks().size() == 0 && !oldLookup &&
                       oldLookup.getError().code ==
                           Automation::AutomationErrorCode::DocumentChanged &&
                       !newLookup &&
                       newLookup.getError().code == Automation::AutomationErrorCode::NotFound,
                   QStringLiteral("replacement must make late duplicate callbacks inert"));
        });
    }

    void testPitchExtraction(Matrix &matrix) {
        const auto operationId = Automation::OperationIds::extract::pitch::start;

        matrix.run(operationId, "AFD-EXT-PITCH-001-VALIDATE-SNAPSHOT", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto taskCount = harness.runtime().automationTasks().size();
            const auto result = harness.runtime().extractions().startPitch(
                harness.context(true), harness.audioClipId(), harness.singingClipId());
            EXPECT(matrix,
                   result && result.get().validatedOnly && result.get().taskId.isNull() &&
                       result.get().document == base &&
                       harness.runtime().automationTasks().size() == taskCount &&
                       harness.extractionScheduler.pendingCount() == 0 &&
                       harness.pitchPrepareCount == 1 && harness.pitchStates.size() == 1 &&
                       harness.pitchStates.first()->startCount == 0 &&
                       harness.pitchStates.first()->input.audioClipId == harness.audioClipId() &&
                       harness.pitchStates.first()->input.singingClipId ==
                           harness.singingClipId() &&
                       harness.pitchStates.first()->input.audioPath == QStringLiteral("source.wav"),
                   QStringLiteral("pitch validation must capture input without allocating a task"));
        });

        matrix.run(operationId, "AFD-EXT-PITCH-002-IDEMPOTENT-TASK-ID", [&] {
            matrix.cover("Queued");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto context =
                keyedContext(harness, QStringLiteral("afda2000-0000-4000-8000-000000000001"));
            const auto first = harness.runtime().extractions().startPitch(
                context, harness.audioClipId(), harness.singingClipId());
            const auto replay = harness.runtime().extractions().startPitch(
                context, harness.audioClipId(), harness.singingClipId());
            EXPECT(matrix,
                   first && replay && replay.get() == first.get() &&
                       harness.pitchPrepareCount == 1 &&
                       harness.extractionScheduler.pendingCount() == 1 &&
                       harness.runtime().automationTasks().size() == 1,
                   QStringLiteral("pitch replay must retain one TaskId and prepared job"));
            if (!first)
                return;
            harness.extractionScheduler.runNext();
            harness.pitchStates.first()->complete(successfulPitch());
            const auto terminalReplay = harness.runtime().extractions().startPitch(
                context, harness.audioClipId(), harness.singingClipId());
            EXPECT(matrix,
                   terminalReplay && terminalReplay.get() == first.get() &&
                       harness.pitchPrepareCount == 1,
                   QStringLiteral("terminal pitch replay must retain the accepted TaskId"));
        });

        matrix.run(operationId, "AFD-EXT-PITCH-003-QUEUED-CANCEL", [&] {
            matrix.cover("Queued");
            matrix.cover("CancelRequested");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            int finishedCount = 0;
            int destroyCountAtFinished = -1;
            Automation::ExtractionObserver observer;
            observer.finished = [&](const Automation::AutomationTaskSnapshot &) {
                ++finishedCount;
                if (!harness.pitchStates.isEmpty())
                    destroyCountAtFinished = harness.pitchStates.last()->destroyCount;
            };
            const auto accepted = harness.runtime().extractions().startPitch(
                harness.context(), harness.audioClipId(), harness.singingClipId(),
                std::move(observer));
            EXPECT(matrix, bool(accepted), QStringLiteral("pitch task must be accepted"));
            if (!accepted)
                return;
            const auto firstCancel =
                harness.runtime().tasks().cancelTask(harness.context(), accepted.get().taskId);
            const auto repeatedCancel =
                harness.runtime().tasks().cancelTask(harness.context(), accepted.get().taskId);
            const auto released = harness.extractionScheduler.runNext();
            const auto terminal =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(matrix,
                   firstCancel && repeatedCancel && released && terminal &&
                       terminal.get().state == Automation::AutomationTaskState::Canceled &&
                       harness.pitchStates.first()->cancelCount == 1 &&
                       harness.pitchStates.first()->startCount == 0 && finishedCount == 1 &&
                       destroyCountAtFinished == 0 &&
                       harness.pitchStates.first()->destroyCount == 1 &&
                       harness.runtime().documentVersion() == base,
                   QStringLiteral("queued pitch cancel must be idempotent and prevent start"));
        });

        matrix.run(operationId, "AFD-EXT-PITCH-004-RUNNING-CANCEL-DUPLICATE", [&] {
            matrix.cover("Running");
            matrix.cover("CancelRequested");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            int finishedCount = 0;
            int destroyCountAtFinished = -1;
            Automation::ExtractionObserver observer;
            observer.finished = [&](const Automation::AutomationTaskSnapshot &) {
                ++finishedCount;
                if (!harness.pitchStates.isEmpty())
                    destroyCountAtFinished = harness.pitchStates.last()->destroyCount;
            };
            const auto accepted = harness.runtime().extractions().startPitch(
                harness.context(), harness.audioClipId(), harness.singingClipId(),
                std::move(observer));
            EXPECT(matrix, accepted && harness.extractionScheduler.runNext(),
                   QStringLiteral("pitch task must reach Running"));
            if (!accepted || harness.pitchStates.isEmpty())
                return;
            const auto state = harness.pitchStates.last();
            const auto canceled =
                harness.runtime().tasks().cancelTask(harness.context(), accepted.get().taskId);
            const auto repeated =
                harness.runtime().tasks().cancelTask(harness.context(), accepted.get().taskId);
            state->complete(successfulPitch());
            state->complete(successfulPitch(67.0));
            const auto terminal =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(matrix,
                   canceled && repeated && terminal &&
                       terminal.get().state == Automation::AutomationTaskState::Canceled &&
                       state->cancelCount == 1 && state->destroyCount == 1 && finishedCount == 1 &&
                       destroyCountAtFinished == 0 && harness.runtime().documentVersion() == base,
                   QStringLiteral("running pitch cancel must beat duplicates and release its job"));
        });

        matrix.run(operationId, "AFD-EXT-PITCH-005-SUCCESS-ONCE", [&] {
            matrix.cover("Running");
            matrix.cover("Committing");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            int finishedCount = 0;
            int destroyCountAtFinished = -1;
            Automation::ExtractionObserver observer;
            observer.finished = [&](const Automation::AutomationTaskSnapshot &) {
                ++finishedCount;
                if (!harness.pitchStates.isEmpty())
                    destroyCountAtFinished = harness.pitchStates.last()->destroyCount;
            };
            const auto accepted = harness.runtime().extractions().startPitch(
                harness.context(), harness.audioClipId(), harness.singingClipId(),
                std::move(observer));
            EXPECT(matrix, accepted && harness.extractionScheduler.runNext(),
                   QStringLiteral("pitch task must reach Running"));
            if (!accepted || harness.pitchStates.isEmpty())
                return;
            const auto state = harness.pitchStates.last();
            state->complete(successfulPitch());
            const auto firstTerminal =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            state->complete(successfulPitch(69.0));
            const auto stableTerminal =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            const auto lateCancel =
                harness.runtime().tasks().cancelTask(harness.context(), accepted.get().taskId);
            EXPECT(matrix,
                   firstTerminal && stableTerminal && lateCancel &&
                       firstTerminal.get().state == Automation::AutomationTaskState::Succeeded &&
                       stableTerminal.get() == firstTerminal.get() &&
                       lateCancel.get() == firstTerminal.get() && finishedCount == 1 &&
                       destroyCountAtFinished == 0 && state->destroyCount == 1 &&
                       harness.runtime().documentVersion().revision == base.revision + 1,
                   QStringLiteral("pitch completion and duplicate callbacks must commit once and "
                                  "release the job"));
        });

        matrix.run(operationId, "AFD-EXT-PITCH-006-BACKEND-FAILURE-RETRY", [&] {
            matrix.cover("Running");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto context =
                keyedContext(harness, QStringLiteral("afda2000-0000-4000-8000-000000000002"));
            const auto first = harness.runtime().extractions().startPitch(
                context, harness.audioClipId(), harness.singingClipId());
            EXPECT(matrix, first && harness.extractionScheduler.runNext(),
                   QStringLiteral("failing pitch task must run"));
            if (!first || harness.pitchStates.isEmpty())
                return;
            const auto state = harness.pitchStates.last();
            state->complete({
                .state = Automation::ExtractionBackendState::Failed,
                .errorCode = Automation::AutomationErrorCode::InferenceError,
                .errorMessage = QStringLiteral("controlled pitch failure"),
            });
            const auto failed = harness.runtime().tasks().getTask(
                harness.runtime().documentVersion().documentId, first.get().taskId);
            const auto retried = harness.runtime().extractions().startPitch(
                context, harness.audioClipId(), harness.singingClipId());
            EXPECT(matrix,
                   failed && failed.get().state == Automation::AutomationTaskState::Failed &&
                       failed.get().error &&
                       failed.get().error->code ==
                           Automation::AutomationErrorCode::InferenceError &&
                       state->destroyCount == 1 && retried &&
                       retried.get().taskId != first.get().taskId &&
                       harness.extractionScheduler.pendingCount() == 1,
                   QStringLiteral("pitch failure must release both its job and idempotency key"));
        });

        matrix.run(operationId, "AFD-EXT-PITCH-007-REVISION-WINS", [&] {
            matrix.cover("Running");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto accepted = harness.runtime().extractions().startPitch(
                harness.context(), harness.audioClipId(), harness.singingClipId());
            EXPECT(matrix, accepted && harness.extractionScheduler.runNext(),
                   QStringLiteral("pitch task must reach Running"));
            if (!accepted || harness.pitchStates.isEmpty())
                return;
            const auto edit = harness.runtime().timeline().setTempo(harness.context(), 960, 139.0);
            harness.pitchStates.last()->complete(successfulPitch());
            const auto terminal =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(matrix,
                   edit && terminal &&
                       terminal.get().state == Automation::AutomationTaskState::Failed &&
                       terminal.get().error &&
                       terminal.get().error->code ==
                           Automation::AutomationErrorCode::RevisionConflict &&
                       harness.runtime().documentVersion().revision == base.revision + 1,
                   QStringLiteral("revision conflict must precede pitch parameter commit"));
        });

        matrix.run(operationId, "AFD-EXT-PITCH-008-TARGET-DELETION", [&] {
            matrix.cover("Running");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto accepted = harness.runtime().extractions().startPitch(
                harness.context(), harness.audioClipId(), harness.singingClipId());
            EXPECT(matrix, accepted && harness.extractionScheduler.runNext(),
                   QStringLiteral("pitch task must reach Running"));
            if (!accepted || harness.pitchStates.isEmpty())
                return;
            const auto removed = harness.runtime().project().removeClips(harness.context(),
                                                                         {harness.singingClipId()});
            harness.pitchStates.last()->complete(successfulPitch());
            const auto terminal =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(matrix,
                   removed && terminal &&
                       terminal.get().state == Automation::AutomationTaskState::Failed &&
                       terminal.get().error &&
                       terminal.get().error->code ==
                           Automation::AutomationErrorCode::RevisionConflict &&
                       !terminal.get().error->object,
                   QStringLiteral(
                       "target deletion must surface revision conflict before object lookup"));
        });

        matrix.run(operationId, "AFD-EXT-PITCH-009-GENERATION-LATE-CALLBACK", [&] {
            matrix.cover("Running");
            matrix.cover("CancelRequested");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto oldVersion = harness.runtime().documentVersion();
            const auto accepted = harness.runtime().extractions().startPitch(
                harness.context(), harness.audioClipId(), harness.singingClipId());
            EXPECT(matrix, accepted && harness.extractionScheduler.runNext(),
                   QStringLiteral("pitch task must reach Running"));
            if (!accepted || harness.pitchStates.isEmpty())
                return;
            const auto state = harness.pitchStates.last();
            const auto replacement = harness.runtime().documents().commitOpenedDocument(
                harness.context(), RuntimeHarness::emptyDocument(),
                QStringLiteral("replacement-pitch.dspx"), QStringLiteral("replacement-pitch.dspx"),
                true);
            const auto newVersion = harness.runtime().documentVersion();
            state->complete(successfulPitch());
            state->complete(successfulPitch(71.0));
            const auto oldLookup =
                harness.runtime().tasks().getTask(oldVersion.documentId, accepted.get().taskId);
            const auto newLookup =
                harness.runtime().tasks().getTask(newVersion.documentId, accepted.get().taskId);
            EXPECT(
                matrix,
                replacement && newVersion.documentId != oldVersion.documentId &&
                    state->cancelCount == 1 && harness.runtime().automationTasks().size() == 0 &&
                    isError(oldLookup, Automation::AutomationErrorCode::DocumentChanged,
                            Automation::OperationIds::tasks::get) &&
                    isError(newLookup, Automation::AutomationErrorCode::NotFound,
                            Automation::OperationIds::tasks::get),
                QStringLiteral("generation replacement must make duplicate pitch callbacks inert"));
        });
    }

    [[nodiscard]] Automation::AudioExportConfigDto audioConfig(const RuntimeHarness &harness,
                                                               const QString &fileName) {
        return {
            .fileName = fileName,
            .fileDirectory = harness.temporaryDirectoryPath(),
        };
    }

    void testAudioExport(Matrix &matrix) {
        const auto operationId = Automation::OperationIds::exports::audio::start;

        matrix.run(operationId, "AFD-EXP-AUDIO-001-VALIDATE-NO-TASK", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto tasksBefore = harness.runtime().automationTasks().size();
            const auto result = harness.runtime().audioExports().start(
                harness.context(true), audioConfig(harness, QStringLiteral("validate.wav")), {});
            EXPECT(matrix,
                   result && result.get().validatedOnly && result.get().taskId.isNull() &&
                       result.get().document == base &&
                       harness.runtime().automationTasks().size() == tasksBefore &&
                       harness.audioScheduler.pendingCount() == 0 &&
                       harness.audioExportState()->createCount == 1 &&
                       harness.audioExportState()->executeCount == 0,
                   QStringLiteral("validation may preview but must not allocate or execute"));
        });

        matrix.run(operationId, "AFD-EXP-AUDIO-002-IDEMPOTENT-TASK-ID", [&] {
            matrix.cover("Queued");
            matrix.cover("Running");
            matrix.cover("Committing");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto context =
                keyedContext(harness, QStringLiteral("afda0000-0000-4000-8000-000000000003"));
            const auto config = audioConfig(harness, QStringLiteral("idempotent.wav"));
            const auto first = harness.runtime().audioExports().start(context, config, {});
            const auto replay = harness.runtime().audioExports().start(context, config, {});
            EXPECT(matrix,
                   first && replay && replay.get() == first.get() &&
                       harness.audioScheduler.pendingCount() == 1 &&
                       harness.runtime().automationTasks().size() == 1 &&
                       harness.audioExportState()->createCount == 1,
                   QStringLiteral("queued replay must retain one TaskId and one preview job"));
            if (!first)
                return;
            harness.audioScheduler.runNext();
            const auto terminalReplay = harness.runtime().audioExports().start(context, config, {});
            const auto terminal = harness.runtime().tasks().getTask(
                harness.runtime().documentVersion().documentId, first.get().taskId);
            EXPECT(matrix,
                   terminal && terminal.get().state == Automation::AutomationTaskState::Succeeded &&
                       terminalReplay && terminalReplay.get() == first.get() &&
                       harness.audioExportState()->createCount == 1 &&
                       harness.audioExportState()->executeCount == 1,
                   QStringLiteral(
                       "terminal replay must keep the TaskId and reuse the authorized export snapshot"));
        });

        matrix.run(operationId, "AFD-EXP-AUDIO-003-QUEUED-CANCEL", [&] {
            matrix.cover("Queued");
            matrix.cover("CancelRequested");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto accepted = harness.runtime().audioExports().start(
                harness.context(), audioConfig(harness, QStringLiteral("queued.wav")), {});
            EXPECT(matrix, bool(accepted), QStringLiteral("task must be accepted"));
            if (!accepted)
                return;
            const auto canceled =
                harness.runtime().tasks().cancelTask(harness.context(), accepted.get().taskId);
            const auto repeated =
                harness.runtime().tasks().cancelTask(harness.context(), accepted.get().taskId);
            const auto released = harness.audioScheduler.runNext();
            const auto terminal =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(matrix,
                   canceled && repeated && released && terminal &&
                       canceled.get().state == Automation::AutomationTaskState::CancelRequested &&
                       repeated.get().state == Automation::AutomationTaskState::CancelRequested &&
                       terminal.get().state == Automation::AutomationTaskState::Canceled &&
                       harness.audioExportState()->executeCount == 0 &&
                       harness.audioExportState()->cleanupCount == 1 &&
                       harness.runtime().documentVersion() == base,
                   QStringLiteral(
                       "queued cancellation must prevent execution and clean its snapshot"));
        });

        matrix.run(operationId, "AFD-EXP-AUDIO-004-BACKEND-CANCELED", [&] {
            matrix.cover("Running");
            matrix.cover("terminal");
            RuntimeHarness harness;
            harness.audioExportState()->backendState =
                Automation::AudioExportBackendState::Canceled;
            const auto base = harness.runtime().documentVersion();
            const auto accepted = harness.runtime().audioExports().start(
                harness.context(), audioConfig(harness, QStringLiteral("backend-cancel.wav")), {});
            EXPECT(matrix, accepted && harness.audioScheduler.runNext(),
                   QStringLiteral("task must execute"));
            if (!accepted)
                return;
            const auto terminal =
                harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId);
            EXPECT(matrix,
                   terminal && terminal.get().state == Automation::AutomationTaskState::Canceled &&
                       harness.audioExportState()->executeCount == 1 &&
                       harness.audioExportState()->cleanupCount == 1 &&
                       harness.runtime().documentVersion() == base,
                   QStringLiteral("backend cancellation must clean its snapshot exactly once"));
        });

        matrix.run(operationId, "AFD-EXP-AUDIO-005-FAILURE-RETRY", [&] {
            matrix.cover("Running");
            matrix.cover("terminal");
            RuntimeHarness harness;
            harness.audioExportState()->backendState = Automation::AudioExportBackendState::Failed;
            const auto context =
                keyedContext(harness, QStringLiteral("afda0000-0000-4000-8000-000000000004"));
            const auto config = audioConfig(harness, QStringLiteral("failure.wav"));
            const auto first = harness.runtime().audioExports().start(context, config, {});
            EXPECT(matrix, first && harness.audioScheduler.runNext(),
                   QStringLiteral("failing task must execute"));
            if (!first)
                return;
            const auto failed = harness.runtime().tasks().getTask(
                harness.runtime().documentVersion().documentId, first.get().taskId);
            harness.audioExportState()->backendState =
                Automation::AudioExportBackendState::Succeeded;
            const auto retried = harness.runtime().audioExports().start(context, config, {});
            EXPECT(matrix,
                   failed && failed.get().state == Automation::AutomationTaskState::Failed &&
                       failed.get().error &&
                       failed.get().error->code == Automation::AutomationErrorCode::IoError &&
                       retried && retried.get().taskId != first.get().taskId &&
                       harness.audioExportState()->cleanupCount == 1 &&
                       harness.audioScheduler.pendingCount() == 1,
                   QStringLiteral(
                       "I/O failure must be queryable, clean its snapshot, and release the key"));
        });

        matrix.run(operationId, "AFD-EXP-AUDIO-006-SNAPSHOT-SURVIVES-REVISION", [&] {
            matrix.cover("Queued");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto accepted = harness.runtime().audioExports().start(
                harness.context(), audioConfig(harness, QStringLiteral("stale.wav")), {});
            const auto edit = harness.runtime().timeline().setTempo(harness.context(), 1440, 139.0);
            const auto released = harness.audioScheduler.runNext();
            const auto terminal =
                accepted ? harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId)
                         : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                               Automation::AutomationError{});
            EXPECT(matrix,
                   accepted && edit && released && terminal &&
                       terminal.get().state == Automation::AutomationTaskState::Succeeded &&
                       !terminal.get().error && harness.audioExportState()->createCount == 1 &&
                       harness.audioExportState()->executeCount == 1 &&
                       harness.audioExportState()->cleanupCount == 0 &&
                       harness.runtime().documentVersion().revision == base.revision + 1,
                   QStringLiteral(
                       "an authorized export snapshot must remain independent of later edits"));
        });

        matrix.run(operationId, "AFD-EXP-AUDIO-007-GENERATION-BEFORE-RUN", [&] {
            matrix.cover("Queued");
            matrix.cover("Running");
            matrix.cover("CancelRequested");
            matrix.cover("terminal");
            {
                RuntimeHarness harness;
                const auto oldVersion = harness.runtime().documentVersion();
                const auto accepted = harness.runtime().audioExports().start(
                    harness.context(), audioConfig(harness, QStringLiteral("generation.wav")), {});
                const auto replacement = harness.runtime().documents().commitNewDocument(
                    harness.context(), RuntimeHarness::emptyDocument());
                const auto newVersion = harness.runtime().documentVersion();
                const auto released = harness.audioScheduler.runNext();
                const auto oldLookup =
                    accepted ? harness.runtime().tasks().getTask(oldVersion.documentId,
                                                                 accepted.get().taskId)
                             : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                                   Automation::AutomationError{});
                const auto newLookup =
                    accepted ? harness.runtime().tasks().getTask(newVersion.documentId,
                                                                 accepted.get().taskId)
                             : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                                   Automation::AutomationError{});
                EXPECT(matrix,
                       accepted && replacement && released &&
                           newVersion.documentId != oldVersion.documentId &&
                           newVersion.revision == 0 &&
                           harness.runtime().automationTasks().size() == 0 &&
                           harness.audioExportState()->executeCount == 0 && !oldLookup &&
                           oldLookup.getError().code ==
                               Automation::AutomationErrorCode::DocumentChanged &&
                           !newLookup &&
                           newLookup.getError().code == Automation::AutomationErrorCode::NotFound,
                       QStringLiteral("generation replacement must make queued work inert"));
            }

            {
                RuntimeHarness harness;
                const auto oldVersion = harness.runtime().documentVersion();
                std::optional<Automation::AutomationResult<Automation::MutationResult>> replacement;
                harness.audioExportState()->backendState =
                    Automation::AudioExportBackendState::Canceled;
                harness.audioExportState()->executeHook = [&] {
                    replacement.emplace(harness.runtime().documents().commitNewDocument(
                        harness.context(), RuntimeHarness::emptyDocument()));
                };
                const auto accepted = harness.runtime().audioExports().start(
                    harness.context(),
                    audioConfig(harness, QStringLiteral("generation-running.wav")), {});
                const auto released = harness.audioScheduler.runNext();
                const auto newVersion = harness.runtime().documentVersion();
                const auto oldLookup =
                    accepted ? harness.runtime().tasks().getTask(oldVersion.documentId,
                                                                 accepted.get().taskId)
                             : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                                   Automation::AutomationError{});
                const auto newLookup =
                    accepted ? harness.runtime().tasks().getTask(newVersion.documentId,
                                                                 accepted.get().taskId)
                             : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                                   Automation::AutomationError{});
                EXPECT(matrix,
                       accepted && released && replacement && *replacement &&
                           newVersion.documentId != oldVersion.documentId &&
                           harness.runtime().automationTasks().size() == 0,
                       QStringLiteral("generation replacement must discard the running task"));
                EXPECT(matrix, harness.audioExportState()->executeCount == 1,
                       QStringLiteral("generation replacement must let the running export observe "
                                      "its cancellation"));
                EXPECT(
                    matrix, harness.audioExportState()->cancelCount == 1,
                    QStringLiteral("generation replacement must cancel the backend exactly once"));
                EXPECT(
                    matrix, harness.audioExportState()->cleanupCount == 1,
                    QStringLiteral("generation replacement must clean the backend exactly once"));
                EXPECT(matrix,
                       !oldLookup &&
                           oldLookup.getError().code ==
                               Automation::AutomationErrorCode::DocumentChanged &&
                           !newLookup &&
                           newLookup.getError().code == Automation::AutomationErrorCode::NotFound,
                       QStringLiteral("replacement generations must not expose the old TaskId"));
            }
        });

        matrix.run(operationId, "AFD-EXP-AUDIO-008-SUCCESS-NO-DOCUMENT-MUTATION", [&] {
            matrix.cover("Running");
            matrix.cover("Committing");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto historyBefore = harness.runtime().history().getState(base.documentId);
            const auto accepted = harness.runtime().audioExports().start(
                harness.context(), audioConfig(harness, QStringLiteral("success.wav")), {});
            const auto released = harness.audioScheduler.runNext();
            const auto terminal =
                accepted ? harness.runtime().tasks().getTask(base.documentId, accepted.get().taskId)
                         : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                               Automation::AutomationError{});
            const auto historyAfter = harness.runtime().history().getState(base.documentId);
            EXPECT(matrix,
                   accepted && released && terminal &&
                       terminal.get().state == Automation::AutomationTaskState::Succeeded &&
                       terminal.get().mutation && terminal.get().mutation->previous == base &&
                       terminal.get().mutation->current == base &&
                       terminal.get().progress.value == 75 &&
                       harness.runtime().documentVersion() == base && historyBefore &&
                       historyAfter && sameHistory(historyBefore.get(), historyAfter.get()),
                   QStringLiteral("successful export must not change revision or History"));
        });

        matrix.run(operationId, "AFD-EXP-AUDIO-009-COMMIT-POINT-CANCEL", [&] {
            matrix.cover("Running");
            matrix.cover("Committing");
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto task = harness.runtime().automationTasks().createTask(operationId, base);
            const auto running = harness.runtime().automationTasks().markRunning(task.taskId);
            const auto committing =
                harness.runtime().automationTasks().beginCommitting(task.taskId);
            const auto cancel =
                harness.runtime().tasks().cancelTask(harness.context(), task.taskId);
            Automation::MutationResult mutation{.previous = base, .current = base};
            const auto succeeded =
                harness.runtime().automationTasks().succeed(task.taskId, mutation);
            const auto lateCancel =
                harness.runtime().tasks().cancelTask(harness.context(), task.taskId);
            EXPECT(
                matrix,
                running && committing && committing.get() && !cancel &&
                    cancel.getError().code ==
                        Automation::AutomationErrorCode::OperationNotCancelable &&
                    succeeded && lateCancel &&
                    lateCancel.get().state == Automation::AutomationTaskState::Succeeded,
                QStringLiteral("Committing must reject cancel and preserve one terminal result"));
        });
    }

    void testOperationQueries(Matrix &matrix) {
        const auto getId = Automation::OperationIds::tasks::get;
        const auto listId = Automation::OperationIds::tasks::list;
        const auto cancelId = Automation::OperationIds::tasks::cancel;

        matrix.run(getId, "AFD-OPS-GET-001-QUEUED", [&] {
            matrix.cover("Queued");
            RuntimeHarness harness;
            const auto task = harness.runtime().automationTasks().createTask(
                Automation::OperationIds::exports::audio::start,
                harness.runtime().documentVersion());
            const auto result = harness.runtime().tasks().getTask(
                harness.runtime().documentVersion().documentId, task.taskId);
            EXPECT(matrix, result && result.get() == task,
                   QStringLiteral("get must return a queued value snapshot"));
        });
        matrix.run(getId, "AFD-OPS-GET-002-RUNNING", [&] {
            matrix.cover("Running");
            RuntimeHarness harness;
            const auto task = harness.runtime().automationTasks().createTask(
                Automation::OperationIds::extract::midi::start,
                harness.runtime().documentVersion());
            harness.runtime().automationTasks().markRunning(task.taskId);
            const auto result = harness.runtime().tasks().getTask(
                harness.runtime().documentVersion().documentId, task.taskId);
            EXPECT(matrix, result && result.get().state == Automation::AutomationTaskState::Running,
                   QStringLiteral("get must expose Running"));
        });
        matrix.run(getId, "AFD-OPS-GET-003-CANCEL-REQUESTED", [&] {
            matrix.cover("CancelRequested");
            RuntimeHarness harness;
            const auto task = harness.runtime().automationTasks().createTask(
                Automation::OperationIds::extract::midi::start,
                harness.runtime().documentVersion());
            harness.runtime().automationTasks().markRunning(task.taskId);
            harness.runtime().tasks().cancelTask(harness.context(), task.taskId);
            const auto result = harness.runtime().tasks().getTask(
                harness.runtime().documentVersion().documentId, task.taskId);
            EXPECT(matrix,
                   result && result.get().state == Automation::AutomationTaskState::CancelRequested,
                   QStringLiteral("get must expose CancelRequested"));
        });
        matrix.run(getId, "AFD-OPS-GET-004-COMMITTING", [&] {
            matrix.cover("Committing");
            RuntimeHarness harness;
            const auto task = harness.runtime().automationTasks().createTask(
                Automation::OperationIds::exports::audio::start,
                harness.runtime().documentVersion());
            harness.runtime().automationTasks().markRunning(task.taskId);
            harness.runtime().automationTasks().beginCommitting(task.taskId);
            const auto result = harness.runtime().tasks().getTask(
                harness.runtime().documentVersion().documentId, task.taskId);
            EXPECT(matrix,
                   result && result.get().state == Automation::AutomationTaskState::Committing &&
                       !result.get().cancelable,
                   QStringLiteral("get must expose the non-cancelable commit point"));
        });
        matrix.run(getId, "AFD-OPS-GET-005-TERMINAL-STABLE", [&] {
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto task = harness.runtime().automationTasks().createTask(
                Automation::OperationIds::exports::audio::start, base);
            harness.runtime().automationTasks().markRunning(task.taskId);
            harness.runtime().automationTasks().beginCommitting(task.taskId);
            Automation::MutationResult mutation{.previous = base, .current = base};
            harness.runtime().automationTasks().succeed(task.taskId, mutation);
            const auto first = harness.runtime().tasks().getTask(base.documentId, task.taskId);
            harness.runtime().automationTasks().fail(task.taskId, Automation::AutomationError{});
            const auto second = harness.runtime().tasks().getTask(base.documentId, task.taskId);
            EXPECT(matrix, first && second && second.get() == first.get(),
                   QStringLiteral("get must retain the first terminal payload"));
        });
        matrix.run(getId, "AFD-OPS-GET-006-UNKNOWN-AND-DOCUMENT", [&] {
            RuntimeHarness harness;
            const auto unknown = harness.runtime().tasks().getTask(
                harness.runtime().documentVersion().documentId, Automation::TaskId::create());
            const auto wrongDocument = harness.runtime().tasks().getTask(
                Automation::DocumentId::create(), Automation::TaskId::create());
            EXPECT(
                matrix,
                isError(unknown, Automation::AutomationErrorCode::NotFound, getId) &&
                    isError(wrongDocument, Automation::AutomationErrorCode::DocumentChanged, getId),
                QStringLiteral("get must distinguish unknown TaskId from wrong generation"));
        });

        matrix.run(listId, "AFD-OPS-LIST-001-EMPTY", [&] {
            RuntimeHarness harness;
            const auto result =
                harness.runtime().tasks().listTasks(harness.runtime().documentVersion().documentId);
            EXPECT(matrix, result && result.get().isEmpty(),
                   QStringLiteral("list must return an empty value collection"));
        });
        matrix.run(listId, "AFD-OPS-LIST-002-ORDERED-SNAPSHOTS", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto first = harness.runtime().automationTasks().createTask(
                Automation::OperationIds::extract::midi::start, base);
            const auto second = harness.runtime().automationTasks().createTask(
                Automation::OperationIds::exports::audio::start, base);
            harness.runtime().automationTasks().markRunning(second.taskId);
            const auto result = harness.runtime().tasks().listTasks(base.documentId);
            EXPECT(matrix,
                   result && result.get().size() == 2 &&
                       std::any_of(result.get().cbegin(), result.get().cend(),
                                   [&](const auto &task) {
                                       return task.taskId == first.taskId &&
                                              task.state == Automation::AutomationTaskState::Queued;
                                   }) &&
                       std::any_of(result.get().cbegin(), result.get().cend(),
                                   [&](const auto &task) {
                                       return task.taskId == second.taskId &&
                                              task.state ==
                                                  Automation::AutomationTaskState::Running;
                                   }),
                   QStringLiteral("list must return detached snapshots for every state"));
        });
        matrix.run(listId, "AFD-OPS-LIST-003-TERMINALS", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto failed = harness.runtime().automationTasks().createTask(
                Automation::OperationIds::extract::midi::start, base);
            harness.runtime().automationTasks().markRunning(failed.taskId);
            Automation::AutomationError error;
            error.code = Automation::AutomationErrorCode::InferenceError;
            harness.runtime().automationTasks().fail(failed.taskId, error);
            const auto canceled = harness.runtime().automationTasks().createTask(
                Automation::OperationIds::exports::audio::start, base);
            harness.runtime().automationTasks().cancel(canceled.taskId);
            const auto result = harness.runtime().tasks().listTasks(base.documentId);
            EXPECT(matrix, result && result.get().size() == 2,
                   QStringLiteral("list must retain failed and canceled terminal records"));
        });
        matrix.run(listId, "AFD-OPS-LIST-004-WRONG-DOCUMENT", [&] {
            RuntimeHarness harness;
            const auto result =
                harness.runtime().tasks().listTasks(Automation::DocumentId::create());
            EXPECT(matrix,
                   isError(result, Automation::AutomationErrorCode::DocumentChanged, listId),
                   QStringLiteral("list must be generation scoped"));
        });
        matrix.run(listId, "AFD-OPS-LIST-005-OLD-GENERATION", [&] {
            RuntimeHarness harness;
            const auto oldVersion = harness.runtime().documentVersion();
            harness.runtime().automationTasks().createTask(
                Automation::OperationIds::extract::midi::start, oldVersion);
            const auto replacement = harness.runtime().documents().commitNewDocument(
                harness.context(), RuntimeHarness::emptyDocument());
            const auto oldList = harness.runtime().tasks().listTasks(oldVersion.documentId);
            const auto newList =
                harness.runtime().tasks().listTasks(harness.runtime().documentVersion().documentId);
            EXPECT(matrix,
                   replacement &&
                       isError(oldList, Automation::AutomationErrorCode::DocumentChanged, listId) &&
                       newList && newList.get().isEmpty(),
                   QStringLiteral("list must not leak records across generation replacement"));
        });
        matrix.run(listId, "AFD-OPS-LIST-006-NO-DOCUMENT-MUTATION", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto historyBefore = harness.runtime().history().getState(base.documentId);
            harness.runtime().tasks().listTasks(base.documentId);
            const auto historyAfter = harness.runtime().history().getState(base.documentId);
            EXPECT(matrix,
                   harness.runtime().documentVersion() == base && historyBefore && historyAfter &&
                       sameHistory(historyBefore.get(), historyAfter.get()),
                   QStringLiteral("list must not mutate document or History"));
        });

        matrix.run(cancelId, "AFD-OPS-CANCEL-001-VALIDATE", [&] {
            matrix.cover("Queued");
            matrix.cover("CancelRequested");
            RuntimeHarness harness;
            int callbackCount = 0;
            const auto task = harness.runtime().automationTasks().createTask(
                Automation::OperationIds::extract::midi::start, harness.runtime().documentVersion(),
                std::nullopt, [&callbackCount] { ++callbackCount; });
            const auto result =
                harness.runtime().tasks().cancelTask(harness.context(true), task.taskId);
            const auto actual = harness.runtime().tasks().getTask(
                harness.runtime().documentVersion().documentId, task.taskId);
            EXPECT(matrix,
                   result && result.get().validatedOnly &&
                       result.get().state == Automation::AutomationTaskState::CancelRequested &&
                       actual && actual.get().state == Automation::AutomationTaskState::Queued &&
                       callbackCount == 0,
                   QStringLiteral("cancel validation must predict without requesting cancel"));
        });
        matrix.run(cancelId, "AFD-OPS-CANCEL-002-QUEUED-REPEAT", [&] {
            matrix.cover("Queued");
            matrix.cover("CancelRequested");
            RuntimeHarness harness;
            int callbackCount = 0;
            const auto task = harness.runtime().automationTasks().createTask(
                Automation::OperationIds::extract::midi::start, harness.runtime().documentVersion(),
                std::nullopt, [&callbackCount] { ++callbackCount; });
            const auto first = harness.runtime().tasks().cancelTask(harness.context(), task.taskId);
            const auto repeated =
                harness.runtime().tasks().cancelTask(harness.context(), task.taskId);
            EXPECT(matrix,
                   first && repeated &&
                       first.get().state == Automation::AutomationTaskState::CancelRequested &&
                       repeated.get() == first.get() && callbackCount == 1,
                   QStringLiteral("queued cancellation must invoke its callback once"));
        });
        matrix.run(cancelId, "AFD-OPS-CANCEL-003-RUNNING", [&] {
            matrix.cover("Running");
            matrix.cover("CancelRequested");
            RuntimeHarness harness;
            const auto task = harness.runtime().automationTasks().createTask(
                Automation::OperationIds::extract::midi::start,
                harness.runtime().documentVersion());
            harness.runtime().automationTasks().markRunning(task.taskId);
            const auto result =
                harness.runtime().tasks().cancelTask(harness.context(), task.taskId);
            EXPECT(matrix,
                   result && result.get().state == Automation::AutomationTaskState::CancelRequested,
                   QStringLiteral("running cancellation must enter CancelRequested"));
        });
        matrix.run(cancelId, "AFD-OPS-CANCEL-004-COMMITTING", [&] {
            matrix.cover("Committing");
            RuntimeHarness harness;
            const auto task = harness.runtime().automationTasks().createTask(
                Automation::OperationIds::exports::audio::start,
                harness.runtime().documentVersion());
            harness.runtime().automationTasks().markRunning(task.taskId);
            harness.runtime().automationTasks().beginCommitting(task.taskId);
            const auto result =
                harness.runtime().tasks().cancelTask(harness.context(), task.taskId);
            EXPECT(
                matrix,
                isError(result, Automation::AutomationErrorCode::OperationNotCancelable, cancelId),
                QStringLiteral("Committing must be the non-cancelable boundary"));
        });
        matrix.run(cancelId, "AFD-OPS-CANCEL-005-TERMINAL", [&] {
            matrix.cover("terminal");
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto task = harness.runtime().automationTasks().createTask(
                Automation::OperationIds::exports::audio::start, base);
            harness.runtime().automationTasks().markRunning(task.taskId);
            harness.runtime().automationTasks().beginCommitting(task.taskId);
            Automation::MutationResult mutation{.previous = base, .current = base};
            harness.runtime().automationTasks().succeed(task.taskId, mutation);
            const auto result =
                harness.runtime().tasks().cancelTask(harness.context(), task.taskId);
            EXPECT(matrix,
                   result && result.get().state == Automation::AutomationTaskState::Succeeded,
                   QStringLiteral("terminal cancellation must return the stable snapshot"));
        });
        matrix.run(cancelId, "AFD-OPS-CANCEL-006-ERROR-PRIORITY", [&] {
            RuntimeHarness harness;
            auto wrongDocument = harness.context();
            wrongDocument.expected.documentId = Automation::DocumentId::create();
            ++wrongDocument.expected.revision;
            auto staleRevision = harness.context();
            ++staleRevision.expected.revision;
            const auto unknownTask = Automation::TaskId::create();
            const auto documentError =
                harness.runtime().tasks().cancelTask(wrongDocument, unknownTask);
            const auto revisionError =
                harness.runtime().tasks().cancelTask(staleRevision, unknownTask);
            const auto taskError =
                harness.runtime().tasks().cancelTask(harness.context(), unknownTask);
            EXPECT(matrix,
                   isError(documentError, Automation::AutomationErrorCode::DocumentChanged,
                           cancelId) &&
                       isError(revisionError, Automation::AutomationErrorCode::NotFound, cancelId) &&
                       isError(taskError, Automation::AutomationErrorCode::NotFound, cancelId),
                   QStringLiteral(
                       "cancel must route by document but ignore revision before resolving TaskId"));
        });
    }

    struct InferenceDimensionCase {
        Automation::InferenceMutationKind kind;
        Automation::OperationId operationId;
        bool advancesRevision = false;
    };

    [[nodiscard]] QList<InferenceDimensionCase> inferenceDimensionCases() {
        using Kind = Automation::InferenceMutationKind;
        return {
            {Kind::ApplyPronunciations,   Automation::OperationIds::inference::apply_pronunciations,
             true                                                                                         },
            {Kind::ApplyPhonemeNames,     Automation::OperationIds::inference::apply_phoneme_names,
             true                                                                                         },
            {Kind::ApplyDuration,         Automation::OperationIds::inference::apply_duration,       true },
            {Kind::ApplyPitch,            Automation::OperationIds::inference::apply_pitch,          true },
            {Kind::ApplyVariance,         Automation::OperationIds::inference::apply_variance,       true },
            {Kind::ApplyAcoustic,         Automation::OperationIds::inference::apply_acoustic,       false},
            {Kind::ResetStage,            Automation::OperationIds::inference::reset_stage,          true },
            {Kind::InvalidateClip,        Automation::OperationIds::inference::invalidate_clip,      false},
            {Kind::ResegmentClip,         Automation::OperationIds::inference::resegment_clip,       false},
            {Kind::RefreshSpeakerMix,     Automation::OperationIds::inference::refresh_speaker_mix,
             true                                                                                         },
            {Kind::RefreshParamInput,     Automation::OperationIds::inference::refresh_param_input,
             false                                                                                        },
            {Kind::RebuildOriginalParams,
             Automation::OperationIds::inference::rebuild_original_params,                           true },
        };
    }

    [[nodiscard]] Automation::InferenceMutationRequest
        inferenceRequest(const RuntimeHarness &harness, const InferenceDimensionCase &testCase,
                         const int discriminator) {
        Automation::InferenceMutationRequest request;
        request.kind = testCase.kind;
        request.clipId = harness.singingClipId();
        request.pieceId = Automation::PieceId(710000 + discriminator);
        request.pieceIds = {request.pieceId};
        request.noteIds = {harness.noteId()};
        request.parameterName = ParamInfo::Pitch;
        request.pitchSmoothKernelSize = 1;
        request.acousticPath = QStringLiteral("dimension-acoustic-%1.wav").arg(discriminator);
        return request;
    }

    [[nodiscard]] QString inferenceScenario(const int index, const int scenario,
                                            const QString &suffix) {
        return QStringLiteral("AFD-INF-%1-%2-%3")
            .arg(index, 2, 10, QLatin1Char('0'))
            .arg(scenario, 3, 10, QLatin1Char('0'))
            .arg(suffix);
    }

    void testInferenceDimensions(Matrix &matrix) {
        const auto cases = inferenceDimensionCases();
        for (int index = 0; index < cases.size(); ++index) {
            const auto testCase = cases.at(index);
            const auto discriminator = (index + 1) * 100;

            matrix.run(
                testCase.operationId,
                inferenceScenario(index + 1, 1, QStringLiteral("VALIDATE-SNAPSHOT")), [&] {
                    RuntimeHarness harness;
                    harness.inferenceChanged = true;
                    harness.inferenceAdvancesRevision = testCase.advancesRevision;
                    const auto request = inferenceRequest(harness, testCase, discriminator + 1);
                    const auto base = harness.runtime().documentVersion();
                    const auto historyBefore =
                        harness.runtime().history().getState(base.documentId);
                    const auto *descriptor = harness.runtime().catalog().find(testCase.operationId);
                    const auto result =
                        harness.runtime().inference().applyMutation(harness.context(true), request);
                    const auto historyAfter = harness.runtime().history().getState(base.documentId);
                    EXPECT(
                        matrix,
                        descriptor &&
                            descriptor->revisionPolicy ==
                                (testCase.advancesRevision ? Automation::RevisionPolicy::Increment
                                                           : Automation::RevisionPolicy::Check) &&
                            descriptor->historyPolicy == Automation::HistoryPolicy::None &&
                            Automation::InferenceAutomationFacade::operationId(testCase.kind) ==
                                testCase.operationId &&
                            result && result.get().mutation.validatedOnly &&
                            result.get().mutation.changed &&
                            result.get().mutation.previous == base &&
                            result.get().mutation.current.revision ==
                                base.revision + (testCase.advancesRevision ? 1 : 0) &&
                            harness.runtime().documentVersion() == base &&
                            harness.inferencePrepareCount == 1 &&
                            harness.inferenceApplyCount == 0 && historyBefore && historyAfter &&
                            sameHistory(historyBefore.get(), historyAfter.get()),
                        QStringLiteral(
                            "validate-only must predict this inference handler without mutation"));
                });

            matrix.run(
                testCase.operationId,
                inferenceScenario(index + 1, 2, QStringLiteral("COMMIT-POLICY")), [&] {
                    RuntimeHarness harness;
                    harness.inferenceChanged = true;
                    harness.inferenceAdvancesRevision = testCase.advancesRevision;
                    const auto request = inferenceRequest(harness, testCase, discriminator + 2);
                    const auto base = harness.runtime().documentVersion();
                    const auto historyBefore =
                        harness.runtime().history().getState(base.documentId);
                    const auto result =
                        harness.runtime().inference().applyMutation(harness.context(), request);
                    const auto historyAfter = harness.runtime().history().getState(base.documentId);
                    EXPECT(
                        matrix,
                        result && result.get().mutation.changed &&
                            result.get().mutation.current.revision ==
                                base.revision + (testCase.advancesRevision ? 1 : 0) &&
                            harness.inferencePrepareCount == 1 &&
                            harness.inferenceApplyCount == 1 &&
                            harness.lastPreparedInferenceKind == testCase.kind &&
                            harness.lastAppliedInferenceKind == testCase.kind &&
                            result.get().sideEffects.changedPieces ==
                                QList<Automation::PieceId>{request.pieceId} &&
                            historyBefore && historyAfter &&
                            sameHistoryStack(historyBefore.get(), historyAfter.get()),
                        QStringLiteral(
                            "real inference writeback must apply once under its revision policy"));
                });

            matrix.run(testCase.operationId,
                       inferenceScenario(index + 1, 3, QStringLiteral("LEGAL-NO-OP")), [&] {
                           RuntimeHarness harness;
                           harness.inferenceChanged = false;
                           harness.inferenceAdvancesRevision = testCase.advancesRevision;
                           const auto request =
                               inferenceRequest(harness, testCase, discriminator + 3);
                           const auto base = harness.runtime().documentVersion();
                           const auto result = harness.runtime().inference().applyMutation(
                               harness.context(), request);
                           EXPECT(matrix,
                                  result && !result.get().mutation.changed &&
                                      result.get().mutation.current == base &&
                                      harness.runtime().documentVersion() == base &&
                                      harness.inferencePrepareCount == 1 &&
                                      harness.inferenceApplyCount == 0,
                                  QStringLiteral("legal no-op must not apply or advance revision"));
                       });

            matrix.run(
                testCase.operationId,
                inferenceScenario(index + 1, 4, QStringLiteral("ERROR-PRIORITY")), [&] {
                    RuntimeHarness harness;
                    const auto request = inferenceRequest(harness, testCase, discriminator + 4);
                    auto wrongDocument = harness.context();
                    wrongDocument.expected.documentId = Automation::DocumentId::create();
                    ++wrongDocument.expected.revision;
                    auto staleRevision = harness.context();
                    ++staleRevision.expected.revision;
                    const auto documentError =
                        harness.runtime().inference().applyMutation(wrongDocument, request);
                    const auto revisionError =
                        harness.runtime().inference().applyMutation(staleRevision, request);
                    EXPECT(matrix,
                           isError(documentError, Automation::AutomationErrorCode::DocumentChanged,
                                   testCase.operationId) &&
                               isError(revisionError,
                                       Automation::AutomationErrorCode::RevisionConflict,
                                       testCase.operationId) &&
                               harness.inferencePrepareCount == 0,
                           QStringLiteral(
                               "document and revision gates must precede inference preparation"));
                });

            matrix.run(
                testCase.operationId,
                inferenceScenario(index + 1, 5, QStringLiteral("HOST-FAILURE-ROLLBACK")), [&] {
                    RuntimeHarness harness;
                    Automation::AutomationError backendError;
                    backendError.code = Automation::AutomationErrorCode::InferenceError;
                    backendError.message = QStringLiteral("dimension inference failure");
                    harness.inferenceError = backendError;
                    const auto request = inferenceRequest(harness, testCase, discriminator + 5);
                    const auto base = harness.runtime().documentVersion();
                    const auto historyBefore =
                        harness.runtime().history().getState(base.documentId);
                    const auto result =
                        harness.runtime().inference().applyMutation(harness.context(), request);
                    const auto historyAfter = harness.runtime().history().getState(base.documentId);
                    EXPECT(matrix,
                           isError(result, Automation::AutomationErrorCode::InferenceError,
                                   testCase.operationId) &&
                               harness.inferencePrepareCount == 1 &&
                               harness.inferenceApplyCount == 0 &&
                               harness.runtime().documentVersion() == base && historyBefore &&
                               historyAfter && sameHistory(historyBefore.get(), historyAfter.get()),
                           QStringLiteral(
                               "host failure must preserve complete document and history state"));
                });

            matrix.run(
                testCase.operationId,
                inferenceScenario(index + 1, 6, QStringLiteral("GENERATION-FENCE")), [&] {
                    RuntimeHarness harness;
                    const auto request = inferenceRequest(harness, testCase, discriminator + 6);
                    const auto staleContext = harness.context();
                    const auto replacement = harness.runtime().documents().commitNewDocument(
                        harness.context(), RuntimeHarness::emptyDocument());
                    const auto prepareBefore = harness.inferencePrepareCount;
                    const auto result =
                        harness.runtime().inference().applyMutation(staleContext, request);
                    EXPECT(matrix,
                           replacement &&
                               isError(result, Automation::AutomationErrorCode::DocumentChanged,
                                       testCase.operationId) &&
                               harness.inferencePrepareCount == prepareBefore,
                           QStringLiteral(
                               "late inference callback must not cross document generations"));
                });
        }
    }

    enum class AudioMutationKind {
        ApplyDecodeCache,
        ApplyResolvedPath,
        ConfirmPath,
        Relocate,
        SetHash,
        SetPathStatus,
    };

    struct AudioDimensionCase {
        AudioMutationKind kind;
        Automation::OperationId operationId;
        bool advancesRevision = false;
        bool recordsHistory = false;
        bool snapshotGuarded = false;
    };

    [[nodiscard]] QList<AudioDimensionCase> audioDimensionCases() {
        using Kind = AudioMutationKind;
        return {
            {Kind::ApplyDecodeCache,  Automation::OperationIds::audio_clips::apply_decode_cache,
             false,                                                                                      false, true },
            {Kind::ApplyResolvedPath, Automation::OperationIds::audio_clips::apply_resolved_path,
             false,                                                                                      false, true },
            {Kind::ConfirmPath,       Automation::OperationIds::audio_clips::confirm_path,        true,  false,
             false                                                                                                   },
            {Kind::Relocate,          Automation::OperationIds::audio_clips::relocate,            true,  true,  false},
            {Kind::SetHash,           Automation::OperationIds::audio_clips::set_hash,            false, false, true },
            {Kind::SetPathStatus,     Automation::OperationIds::audio_clips::set_path_status,     false,
             false,                                                                                             true },
        };
    }

    struct AudioMutationPlan {
        AudioMutationKind kind = AudioMutationKind::ApplyDecodeCache;
        Automation::AudioAssetSnapshotDto expectedAsset;
        AudioInfoModel audioInfo;
        QString path;
        AudioPathInfo pathInfo;
        QJsonObject formatData;
        QString hash;
        bool fixtureReady = true;
    };

    [[nodiscard]] AudioClip *fixtureAudio(RuntimeHarness &harness) {
        return dynamic_cast<AudioClip *>(
            harness.model().findClipById(harness.audioClipId().value()));
    }

    [[nodiscard]] AudioMutationPlan makeAudioPlan(RuntimeHarness &harness,
                                                  const AudioDimensionCase &testCase,
                                                  const int discriminator) {
        AudioMutationPlan plan;
        plan.kind = testCase.kind;
        auto *audio = fixtureAudio(harness);
        if (!audio) {
            plan.fixtureReady = false;
            return plan;
        }
        plan.expectedAsset = Automation::audioAssetSnapshotDto(*audio);
        plan.audioInfo.sampleRate = 32000 + discriminator;
        plan.audioInfo.channels = 2;
        plan.audioInfo.frames = 64000 + discriminator;
        plan.audioInfo.peakCache.append({-discriminator, discriminator});
        plan.pathInfo.relativeDir = QStringLiteral("dimension-media-%1").arg(discriminator);
        plan.pathInfo.sha512 = QStringLiteral("dimension-relocate-hash-%1").arg(discriminator);
        plan.formatData.insert(QStringLiteral("codec"),
                               QStringLiteral("dimension-pcm-%1").arg(discriminator));
        plan.hash = QStringLiteral("dimension-cache-hash-%1").arg(discriminator);
        if (testCase.kind == AudioMutationKind::ApplyResolvedPath) {
            plan.path = harness.temporaryPath(
                QStringLiteral("dimension-resolved-%1.wav").arg(discriminator));
            QFile file(plan.path);
            const auto payload = QByteArrayLiteral("dimension-audio");
            plan.fixtureReady = file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                                file.write(payload) == payload.size();
            file.close();
        } else {
            plan.path = QStringLiteral("dimension-relocated-%1.wav").arg(discriminator);
        }
        return plan;
    }

    [[nodiscard]] Automation::AutomationResult<Automation::MutationResult>
        invokeAudioMutation(RuntimeHarness &harness, const AudioMutationPlan &plan,
                            const Automation::CommandContext &context,
                            const Automation::ClipId clipId) {
        switch (plan.kind) {
            case AudioMutationKind::ApplyDecodeCache:
                return harness.runtime().project().applyAudioDecodeCache(
                    context, clipId, plan.expectedAsset, plan.audioInfo);
            case AudioMutationKind::ApplyResolvedPath:
                return harness.runtime().project().applyResolvedAudioPath(
                    context, clipId, plan.expectedAsset, plan.path, AudioClip::PathStatus::Normal);
            case AudioMutationKind::ConfirmPath:
                return harness.runtime().project().confirmAudioClipPath(context, clipId);
            case AudioMutationKind::Relocate:
                return harness.runtime().project().relocateAudioClip(
                    context, clipId, plan.path, plan.pathInfo, plan.formatData);
            case AudioMutationKind::SetHash:
                return harness.runtime().project().setAudioClipHash(context, clipId,
                                                                    plan.expectedAsset, plan.hash);
            case AudioMutationKind::SetPathStatus:
                return harness.runtime().project().setAudioClipPathStatus(
                    context, clipId, plan.expectedAsset, AudioClip::PathStatus::Unconfirmed);
        }
        Q_UNREACHABLE_RETURN(Automation::AutomationError::invalidArgument(
            QStringLiteral("kind"), QStringLiteral("Unknown audio mutation kind")));
    }

    [[nodiscard]] bool audioPlanApplied(const AudioMutationPlan &plan, const AudioClip &audio) {
        switch (plan.kind) {
            case AudioMutationKind::ApplyDecodeCache:
                return audio.audioInfo().sampleRate == plan.audioInfo.sampleRate &&
                       audio.pathStatus() == AudioClip::PathStatus::Normal;
            case AudioMutationKind::ApplyResolvedPath:
                return audio.path() == plan.path &&
                       audio.pathStatus() == AudioClip::PathStatus::Normal;
            case AudioMutationKind::ConfirmPath:
                return audio.pathStatus() == AudioClip::PathStatus::Normal;
            case AudioMutationKind::Relocate:
                return audio.path() == plan.path &&
                       audio.pathInfo().relativeDir == plan.pathInfo.relativeDir &&
                       audio.pathInfo().sha512 == plan.pathInfo.sha512 &&
                       audio.pathStatus() == AudioClip::PathStatus::Normal;
            case AudioMutationKind::SetHash:
                return audio.pathInfo().sha512 == plan.hash;
            case AudioMutationKind::SetPathStatus:
                return audio.pathStatus() == AudioClip::PathStatus::Unconfirmed;
        }
        return false;
    }

    [[nodiscard]] QString audioScenario(const int index, const int scenario,
                                        const QString &suffix) {
        return QStringLiteral("AFD-AUDIO-%1-%2-%3")
            .arg(index, 2, 10, QLatin1Char('0'))
            .arg(scenario, 3, 10, QLatin1Char('0'))
            .arg(suffix);
    }

    void testAudioClipDimensions(Matrix &matrix) {
        const auto cases = audioDimensionCases();
        for (int index = 0; index < cases.size(); ++index) {
            const auto testCase = cases.at(index);
            const auto discriminator = (index + 1) * 100;

            matrix.run(
                testCase.operationId,
                audioScenario(index + 1, 1, QStringLiteral("VALIDATE-SNAPSHOT")), [&] {
                    RuntimeHarness harness;
                    auto *audio = fixtureAudio(harness);
                    auto plan = makeAudioPlan(harness, testCase, discriminator + 1);
                    const auto base = harness.runtime().documentVersion();
                    const auto historyBefore =
                        harness.runtime().history().getState(base.documentId);
                    const auto sourceBefore = audio ? audio->sourceGeneration() : -1;
                    const auto pathBefore = audio ? audio->path() : QString();
                    const auto statusBefore =
                        audio ? audio->pathStatus() : AudioClip::PathStatus::Missing;
                    const auto *descriptor = harness.runtime().catalog().find(testCase.operationId);
                    const auto result = invokeAudioMutation(harness, plan, harness.context(true),
                                                            harness.audioClipId());
                    const auto historyAfter = harness.runtime().history().getState(base.documentId);
                    EXPECT(matrix,
                           audio && plan.fixtureReady && descriptor &&
                               descriptor->revisionPolicy ==
                                   (testCase.advancesRevision
                                        ? Automation::RevisionPolicy::Increment
                                        : Automation::RevisionPolicy::None) &&
                               descriptor->historyPolicy ==
                                   (testCase.recordsHistory ? Automation::HistoryPolicy::Record
                                                            : Automation::HistoryPolicy::None) &&
                               result && result.get().validatedOnly && result.get().changed &&
                               result.get().previous == base &&
                               result.get().current.revision ==
                                   base.revision + (testCase.advancesRevision ? 1 : 0) &&
                               harness.runtime().documentVersion() == base &&
                               audio->sourceGeneration() == sourceBefore &&
                               audio->path() == pathBefore && audio->pathStatus() == statusBefore &&
                               historyBefore && historyAfter &&
                               sameHistory(historyBefore.get(), historyAfter.get()),
                           QStringLiteral(
                               "audio validate-only must predict without touching asset state"));
                });

            matrix.run(
                testCase.operationId, audioScenario(index + 1, 2, QStringLiteral("COMMIT-POLICY")),
                [&] {
                    RuntimeHarness harness;
                    auto *audio = fixtureAudio(harness);
                    auto plan = makeAudioPlan(harness, testCase, discriminator + 2);
                    const auto base = harness.runtime().documentVersion();
                    const auto historyBefore =
                        harness.runtime().history().getState(base.documentId);
                    const auto result = invokeAudioMutation(harness, plan, harness.context(),
                                                            harness.audioClipId());
                    const auto historyAfter = harness.runtime().history().getState(base.documentId);
                    EXPECT(
                        matrix,
                        audio && plan.fixtureReady && result && result.get().changed &&
                            audioPlanApplied(plan, *audio) &&
                            harness.runtime().documentVersion().revision ==
                                base.revision + (testCase.advancesRevision ? 1 : 0) &&
                            (!testCase.recordsHistory && historyBefore && historyAfter
                                 ? sameHistoryStack(historyBefore.get(), historyAfter.get())
                                 : true),
                        QStringLiteral("audio handler must obey its revision and History policy"));
                });

            matrix.run(
                testCase.operationId, audioScenario(index + 1, 3, QStringLiteral("LEGAL-NO-OP")),
                [&] {
                    RuntimeHarness harness;
                    auto *audio = fixtureAudio(harness);
                    auto plan = makeAudioPlan(harness, testCase, discriminator + 3);
                    const auto first = invokeAudioMutation(harness, plan, harness.context(),
                                                           harness.audioClipId());
                    if (audio)
                        plan.expectedAsset = Automation::audioAssetSnapshotDto(*audio);
                    const auto base = harness.runtime().documentVersion();
                    const auto historyBefore =
                        harness.runtime().history().getState(base.documentId);
                    const auto noOp = invokeAudioMutation(harness, plan, harness.context(),
                                                          harness.audioClipId());
                    const auto historyAfter = harness.runtime().history().getState(base.documentId);
                    EXPECT(matrix,
                           audio && plan.fixtureReady && first && noOp && !noOp.get().changed &&
                               audioPlanApplied(plan, *audio) &&
                               harness.runtime().documentVersion() == base && historyBefore &&
                               historyAfter && sameHistory(historyBefore.get(), historyAfter.get()),
                           QStringLiteral("reapplying identical audio state must be a true no-op"));
                });

            matrix.run(
                testCase.operationId, audioScenario(index + 1, 4, QStringLiteral("ERROR-PRIORITY")),
                [&] {
                    RuntimeHarness harness;
                    const auto plan = makeAudioPlan(harness, testCase, discriminator + 4);
                    auto wrongDocument = harness.context();
                    wrongDocument.expected.documentId = Automation::DocumentId::create();
                    ++wrongDocument.expected.revision;
                    auto staleRevision = harness.context();
                    ++staleRevision.expected.revision;
                    const auto invalidClip = Automation::ClipId(990000 + index);
                    const auto documentError =
                        invokeAudioMutation(harness, plan, wrongDocument, invalidClip);
                    const auto revisionError =
                        invokeAudioMutation(harness, plan, staleRevision, invalidClip);
                    EXPECT(matrix,
                           plan.fixtureReady &&
                               isError(documentError,
                                       Automation::AutomationErrorCode::DocumentChanged,
                                       testCase.operationId) &&
                               isError(revisionError,
                                       testCase.advancesRevision
                                           ? Automation::AutomationErrorCode::RevisionConflict
                                           : Automation::AutomationErrorCode::NotFound,
                                       testCase.operationId),
                           QStringLiteral(
                               "audio handler must apply its declared revision policy before object lookup"));
                });

            matrix.run(testCase.operationId,
                       audioScenario(index + 1, 5, QStringLiteral("WRONG-OBJECT-TYPE")), [&] {
                           RuntimeHarness harness;
                           const auto plan = makeAudioPlan(harness, testCase, discriminator + 5);
                           const auto result = invokeAudioMutation(harness, plan, harness.context(),
                                                                   harness.singingClipId());
                           EXPECT(matrix,
                                  plan.fixtureReady &&
                                      isError(result,
                                              Automation::AutomationErrorCode::WrongObjectType,
                                              testCase.operationId),
                                  QStringLiteral("audio-only handler must reject a singing clip"));
                       });

            matrix.run(
                testCase.operationId,
                audioScenario(index + 1, 6, QStringLiteral("GENERATION-FENCE")), [&] {
                    RuntimeHarness harness;
                    const auto plan = makeAudioPlan(harness, testCase, discriminator + 6);
                    const auto staleContext = harness.context();
                    const auto replacement = harness.runtime().documents().commitNewDocument(
                        harness.context(), RuntimeHarness::emptyDocument());
                    const auto result =
                        invokeAudioMutation(harness, plan, staleContext, harness.audioClipId());
                    EXPECT(
                        matrix,
                        plan.fixtureReady && replacement &&
                            isError(result, Automation::AutomationErrorCode::DocumentChanged,
                                    testCase.operationId),
                        QStringLiteral("late audio writeback must not cross document generations"));
                });

            matrix.run(
                testCase.operationId,
                audioScenario(index + 1, 7, QStringLiteral("SNAPSHOT-OR-INPUT-GUARD")), [&] {
                    RuntimeHarness harness;
                    auto *audio = fixtureAudio(harness);
                    auto plan = makeAudioPlan(harness, testCase, discriminator + 7);
                    const auto base = harness.runtime().documentVersion();
                    const auto historyBefore =
                        harness.runtime().history().getState(base.documentId);
                    Automation::AutomationResult<Automation::MutationResult> result(
                        Automation::AutomationError{});
                    if (testCase.snapshotGuarded) {
                        plan.expectedAsset.path = QStringLiteral("stale-dimension.wav");
                        result = invokeAudioMutation(harness, plan, harness.context(),
                                                     harness.audioClipId());
                    } else if (testCase.kind == AudioMutationKind::Relocate) {
                        plan.path.clear();
                        result = invokeAudioMutation(harness, plan, harness.context(),
                                                     harness.audioClipId());
                    } else {
                        result = invokeAudioMutation(harness, plan, harness.context(),
                                                     Automation::ClipId(999991));
                    }
                    const auto historyAfter = harness.runtime().history().getState(base.documentId);
                    EXPECT(matrix,
                           audio && plan.fixtureReady &&
                               isError(result,
                                       testCase.kind == AudioMutationKind::ConfirmPath
                                           ? Automation::AutomationErrorCode::NotFound
                                           : Automation::AutomationErrorCode::InvalidArgument,
                                       testCase.operationId) &&
                               harness.runtime().documentVersion() == base && historyBefore &&
                               historyAfter && sameHistory(historyBefore.get(), historyAfter.get()),
                           QStringLiteral(
                               "stale asset or invalid input must fail without partial writeback"));
                });

            matrix.run(
                testCase.operationId,
                audioScenario(index + 1, 8, QStringLiteral("IDEMPOTENT-REPLAY")), [&] {
                    RuntimeHarness harness;
                    auto *audio = fixtureAudio(harness);
                    const auto plan = makeAudioPlan(harness, testCase, discriminator + 8);
                    const auto base = harness.runtime().documentVersion();
                    const auto context =
                        keyedContext(harness, QStringLiteral("afda1000-0000-4000-8000-%1")
                                                  .arg(index + 1, 12, 10, QLatin1Char('0')));
                    const auto first =
                        invokeAudioMutation(harness, plan, context, harness.audioClipId());
                    const auto replay =
                        invokeAudioMutation(harness, plan, context, harness.audioClipId());
                    EXPECT(matrix,
                           audio && plan.fixtureReady && first && replay &&
                               replay.get() == first.get() && audioPlanApplied(plan, *audio) &&
                               harness.runtime().documentVersion().revision ==
                                   base.revision + (testCase.advancesRevision ? 1 : 0),
                           QStringLiteral("same idempotency key must replay one audio mutation"));
                });
        }
    }

    [[nodiscard]] Automation::TaskId finishAudioExport(RuntimeHarness &harness,
                                                       const QString &fileName) {
        const auto accepted = harness.runtime().audioExports().start(
            harness.context(), audioConfig(harness, fileName), {});
        if (!accepted)
            return {};
        if (!harness.audioScheduler.runNext())
            return {};
        return accepted.get().taskId;
    }

    void testFormatsDimensions(Matrix &matrix) {
        const auto operationId = Automation::OperationIds::formats::list;

        matrix.run(operationId, "AFD-FMT-001-TYPED-SNAPSHOT", [&] {
            RuntimeHarness harness;
            const auto *descriptor = harness.runtime().catalog().find(operationId);
            const auto result = harness.runtime().files().listFormats();
            EXPECT(matrix,
                   descriptor && descriptor->kind == Automation::OperationKind::Query &&
                       descriptor->documentPolicy == Automation::DocumentPolicy::None && result &&
                       result.get() == harness.formats && result.get().size() == 2,
                   QStringLiteral("formats must expose the provider's typed value snapshot"));
        });

        matrix.run(operationId, "AFD-FMT-002-VALUE-ISOLATION", [&] {
            RuntimeHarness harness;
            const auto result = harness.runtime().files().listFormats();
            harness.formats.clear();
            EXPECT(matrix, result && result.get().size() == 2 && harness.formats.isEmpty(),
                   QStringLiteral("returned formats must not alias mutable provider storage"));
        });

        matrix.run(operationId, "AFD-FMT-003-EMPTY-PROVIDER", [&] {
            RuntimeHarness harness;
            harness.formats.clear();
            const auto result = harness.runtime().files().listFormats();
            EXPECT(matrix, result && result.get().isEmpty(),
                   QStringLiteral("an available provider may legally return no formats"));
        });

        matrix.run(operationId, "AFD-FMT-004-HOST-UNAVAILABLE", [&] {
            RuntimeHarness harness({.fileServices = false});
            const auto result = harness.runtime().files().listFormats();
            EXPECT(matrix,
                   isError(result, Automation::AutomationErrorCode::ModuleNotReady, operationId),
                   QStringLiteral("missing format provider must be explicit"));
        });

        matrix.run(operationId, "AFD-FMT-005-REPEAT-NO-MUTATION", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto historyBefore = harness.runtime().history().getState(base.documentId);
            const auto tasksBefore = harness.runtime().automationTasks().size();
            const auto first = harness.runtime().files().listFormats();
            const auto second = harness.runtime().files().listFormats();
            const auto historyAfter = harness.runtime().history().getState(base.documentId);
            EXPECT(matrix,
                   first && second && first.get() == second.get() &&
                       harness.runtime().documentVersion() == base &&
                       harness.runtime().automationTasks().size() == tasksBefore && historyBefore &&
                       historyAfter && sameHistory(historyBefore.get(), historyAfter.get()),
                   QStringLiteral("repeated format queries must be side-effect free"));
        });

        matrix.run(operationId, "AFD-FMT-006-DISPATCH-ERROR-STAMP", [&] {
            RuntimeHarness harness;
            Automation::AutomationError providerError;
            providerError.code = Automation::AutomationErrorCode::ModuleNotReady;
            providerError.message = QStringLiteral("dimension format provider failure");
            const auto result = harness.runtime()
                                    .dispatcher()
                                    .dispatchApplicationQuery<QList<Automation::ProjectFormatDto>>(
                                        operationId, [providerError] {
                                            return Automation::AutomationResult<
                                                QList<Automation::ProjectFormatDto>>(providerError);
                                        });
            EXPECT(matrix,
                   isError(result, Automation::AutomationErrorCode::ModuleNotReady, operationId),
                   QStringLiteral("dispatcher must stamp provider errors with formats.list"));
        });
    }

    void testMidiExportDimensions(Matrix &matrix) {
        const auto operationId = Automation::OperationIds::exports::midi::start;

        matrix.run(operationId, "AFD-EXP-MIDI-001-VALIDATE-NO-IO", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto historyBefore = harness.runtime().history().getState(base.documentId);
            const auto path = harness.temporaryPath(QStringLiteral("dimension-validate.mid"));
            const auto result =
                harness.runtime().files().exportMidi(harness.context(true), path, false);
            const auto historyAfter = harness.runtime().history().getState(base.documentId);
            EXPECT(matrix,
                   result && result.get().validatedOnly && !result.get().wroteFile &&
                       result.get().path == QDir::cleanPath(path) && harness.midiExportCount == 0 &&
                       harness.runtime().documentVersion() == base && historyBefore &&
                       historyAfter && sameHistory(historyBefore.get(), historyAfter.get()),
                   QStringLiteral("MIDI validate-only must normalize without writing"));
        });

        matrix.run(operationId, "AFD-EXP-MIDI-002-SUCCESS-NO-DOCUMENT-MUTATION", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto historyBefore = harness.runtime().history().getState(base.documentId);
            const auto path = harness.temporaryPath(QStringLiteral("dimension-success.midi"));
            const auto result =
                harness.runtime().files().exportMidi(harness.context(), path, false);
            const auto historyAfter = harness.runtime().history().getState(base.documentId);
            EXPECT(matrix,
                   result && result.get().wroteFile && !result.get().validatedOnly &&
                       result.get().path == QDir::cleanPath(path) && harness.midiExportCount == 1 &&
                       harness.lastMidiExportPath == QDir::cleanPath(path) &&
                       harness.runtime().documentVersion() == base && historyBefore &&
                       historyAfter && sameHistory(historyBefore.get(), historyAfter.get()),
                   QStringLiteral("successful MIDI export must not change document or History"));
        });

        matrix.run(operationId, "AFD-EXP-MIDI-003-IO-FAILURE-ROLLBACK", [&] {
            RuntimeHarness harness;
            harness.midiExportSucceeds = false;
            const auto base = harness.runtime().documentVersion();
            const auto snapshotBefore = harness.runtime().documents().getDocument(base.documentId);
            const auto historyBefore = harness.runtime().history().getState(base.documentId);
            const auto result = harness.runtime().files().exportMidi(
                harness.context(), harness.temporaryPath(QStringLiteral("dimension-failed.mid")),
                false);
            const auto snapshotAfter = harness.runtime().documents().getDocument(base.documentId);
            const auto historyAfter = harness.runtime().history().getState(base.documentId);
            EXPECT(matrix,
                   isError(result, Automation::AutomationErrorCode::IoError, operationId) &&
                       harness.midiExportCount == 1 && snapshotBefore && snapshotAfter &&
                       snapshotAfter.get().document == snapshotBefore.get().document &&
                       snapshotAfter.get().path == snapshotBefore.get().path && historyBefore &&
                       historyAfter && sameHistory(historyBefore.get(), historyAfter.get()),
                   QStringLiteral("MIDI I/O failure must leave the full session unchanged"));
        });

        matrix.run(operationId, "AFD-EXP-MIDI-004-PATH-VALIDATION-ORDER", [&] {
            RuntimeHarness harness;
            const auto empty = harness.runtime().files().exportMidi(harness.context(), {}, false);
            const auto relative = harness.runtime().files().exportMidi(
                harness.context(), QStringLiteral("dimension-relative.mid"), false);
            const auto format = harness.runtime().files().exportMidi(
                harness.context(), harness.temporaryPath(QStringLiteral("dimension.wav")), false);
            const auto missingDirectory = harness.runtime().files().exportMidi(
                harness.context(),
                QDir(harness.temporaryDirectoryPath())
                    .filePath(QStringLiteral("missing/subdirectory/dimension.mid")),
                false);
            EXPECT(matrix,
                   isError(empty, Automation::AutomationErrorCode::PathRequired, operationId) &&
                       isError(relative, Automation::AutomationErrorCode::InvalidArgument,
                               operationId) &&
                       isError(format, Automation::AutomationErrorCode::FormatUnsupported,
                               operationId) &&
                       isError(missingDirectory, Automation::AutomationErrorCode::FileNotFound,
                               operationId) &&
                       harness.midiExportCount == 0,
                   QStringLiteral("MIDI path validation must fail before invoking the writer"));
        });

        matrix.run(operationId, "AFD-EXP-MIDI-005-OVERWRITE-POLICY", [&] {
            RuntimeHarness harness;
            const auto path = harness.temporaryPath(QStringLiteral("dimension-existing.mid"));
            QFile file(path);
            const auto created = file.open(QIODevice::WriteOnly | QIODevice::Truncate);
            file.close();
            const auto denied =
                harness.runtime().files().exportMidi(harness.context(), path, false);
            const auto allowed =
                harness.runtime().files().exportMidi(harness.context(), path, true);
            EXPECT(matrix,
                   created &&
                       isError(denied, Automation::AutomationErrorCode::OverwriteDenied,
                               operationId) &&
                       allowed && allowed.get().wroteFile && harness.midiExportCount == 1,
                   QStringLiteral("overwrite policy must gate the writer deterministically"));
        });

        matrix.run(operationId, "AFD-EXP-MIDI-006-HOST-UNAVAILABLE", [&] {
            RuntimeHarness harness({.fileServices = false});
            const auto result = harness.runtime().files().exportMidi(
                harness.context(), harness.temporaryPath(QStringLiteral("dimension-host.mid")),
                false);
            EXPECT(matrix,
                   isError(result, Automation::AutomationErrorCode::ModuleNotReady, operationId),
                   QStringLiteral("missing MIDI writer must be explicit"));
        });

        matrix.run(operationId, "AFD-EXP-MIDI-007-ERROR-PRIORITY", [&] {
            RuntimeHarness harness({.fileServices = false});
            auto wrongDocument = harness.context();
            wrongDocument.expected.documentId = Automation::DocumentId::create();
            ++wrongDocument.expected.revision;
            auto staleRevision = harness.context();
            ++staleRevision.expected.revision;
            const auto documentError =
                harness.runtime().files().exportMidi(wrongDocument, {}, false);
            const auto revisionError =
                harness.runtime().files().exportMidi(staleRevision, {}, false);
            const auto inputError =
                harness.runtime().files().exportMidi(harness.context(), {}, false);
            EXPECT(
                matrix,
                isError(documentError, Automation::AutomationErrorCode::DocumentChanged,
                        operationId) &&
                    isError(revisionError, Automation::AutomationErrorCode::RevisionConflict,
                            operationId) &&
                    isError(inputError, Automation::AutomationErrorCode::PathRequired, operationId),
                QStringLiteral("MIDI errors must order document, revision, input, then host"));
        });

        matrix.run(operationId, "AFD-EXP-MIDI-008-IDEMPOTENT-WRITE", [&] {
            RuntimeHarness harness;
            const auto context =
                keyedContext(harness, QStringLiteral("afda3000-0000-4000-8000-000000000001"));
            const auto path = harness.temporaryPath(QStringLiteral("dimension-idempotent.mid"));
            const auto first = harness.runtime().files().exportMidi(context, path, false);
            const auto replay = harness.runtime().files().exportMidi(context, path, false);
            EXPECT(matrix,
                   first && replay && replay.get() == first.get() && harness.midiExportCount == 1,
                   QStringLiteral("MIDI idempotency replay must write exactly once"));
        });
    }

    void testAudioPreviewDimensions(Matrix &matrix) {
        const auto operationId = Automation::OperationIds::exports::audio::preview;

        matrix.run(operationId, "AFD-EXP-PREVIEW-001-TYPED-SNAPSHOT", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto tasksBefore = harness.runtime().automationTasks().size();
            const auto config = audioConfig(harness, QStringLiteral("dimension-preview.wav"));
            const auto result = harness.runtime().audioExports().preview(base.documentId, config);
            EXPECT(matrix,
                   result && result.get().baseDirectory == config.fileDirectory &&
                       result.get().filePaths.size() == 1 &&
                       result.get().filePaths.first() ==
                           QDir(config.fileDirectory).absoluteFilePath(config.fileName) &&
                       harness.audioExportState()->createCount == 1 &&
                       harness.audioExportState()->executeCount == 0 &&
                       harness.runtime().automationTasks().size() == tasksBefore &&
                       harness.runtime().documentVersion() == base,
                   QStringLiteral("audio preview must return a typed snapshot without a task"));
        });

        matrix.run(operationId, "AFD-EXP-PREVIEW-002-INPUT-BEFORE-HOST", [&] {
            RuntimeHarness harness({.audioExportServices = false});
            auto config = audioConfig(harness, QStringLiteral("valid.wav"));
            config.fileName.clear();
            const auto result = harness.runtime().audioExports().preview(
                harness.runtime().documentVersion().documentId, config);
            EXPECT(matrix,
                   isError(result, Automation::AutomationErrorCode::PathRequired, operationId),
                   QStringLiteral("preview config validation must precede host availability"));
        });

        matrix.run(operationId, "AFD-EXP-PREVIEW-003-HOST-UNAVAILABLE", [&] {
            RuntimeHarness harness({.audioExportServices = false});
            const auto result = harness.runtime().audioExports().preview(
                harness.runtime().documentVersion().documentId,
                audioConfig(harness, QStringLiteral("dimension-host.wav")));
            EXPECT(matrix,
                   isError(result, Automation::AutomationErrorCode::ModuleNotReady, operationId),
                   QStringLiteral("missing audio preview host must be explicit"));
        });

        matrix.run(operationId, "AFD-EXP-PREVIEW-004-CREATE-FAILURE", [&] {
            RuntimeHarness harness;
            Automation::AutomationError createError;
            createError.code = Automation::AutomationErrorCode::IoError;
            createError.message = QStringLiteral("dimension preview creation failed");
            harness.audioCreateError = createError;
            const auto base = harness.runtime().documentVersion();
            const auto result = harness.runtime().audioExports().preview(
                base.documentId, audioConfig(harness, QStringLiteral("dimension-create.wav")));
            EXPECT(matrix,
                   isError(result, Automation::AutomationErrorCode::IoError, operationId) &&
                       harness.audioExportState()->createCount == 1 &&
                       harness.runtime().documentVersion() == base,
                   QStringLiteral("preview creation failure must be typed and non-mutating"));
        });

        matrix.run(operationId, "AFD-EXP-PREVIEW-005-DOCUMENT-PRIORITY", [&] {
            RuntimeHarness harness({.audioExportServices = false});
            auto invalidConfig = audioConfig(harness, QStringLiteral("valid.wav"));
            invalidConfig.fileName.clear();
            const auto result = harness.runtime().audioExports().preview(
                Automation::DocumentId::create(), invalidConfig);
            EXPECT(matrix,
                   isError(result, Automation::AutomationErrorCode::DocumentChanged, operationId),
                   QStringLiteral("explicit document routing must precede config and host checks"));
        });

        matrix.run(operationId, "AFD-EXP-PREVIEW-006-REPEAT-NO-MUTATION", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto historyBefore = harness.runtime().history().getState(base.documentId);
            const auto config = audioConfig(harness, QStringLiteral("dimension-repeat.wav"));
            const auto first = harness.runtime().audioExports().preview(base.documentId, config);
            const auto second = harness.runtime().audioExports().preview(base.documentId, config);
            const auto historyAfter = harness.runtime().history().getState(base.documentId);
            EXPECT(matrix,
                   first && second && first.get() == second.get() &&
                       harness.audioExportState()->createCount == 2 &&
                       harness.runtime().documentVersion() == base && historyBefore &&
                       historyAfter && sameHistory(historyBefore.get(), historyAfter.get()),
                   QStringLiteral("preview is repeatable but must not cache mutable jobs"));
        });

        matrix.run(operationId, "AFD-EXP-PREVIEW-007-WARNING-SNAPSHOT", [&] {
            RuntimeHarness harness;
            harness.audioExportState()->warningFlags =
                Automation::AudioExportWillOverwrite | Automation::AudioExportLossyFormat;
            const auto result = harness.runtime().audioExports().preview(
                harness.runtime().documentVersion().documentId,
                audioConfig(harness, QStringLiteral("dimension-warning.wav")));
            EXPECT(matrix,
                   result && result.get().warningFlags == (Automation::AudioExportWillOverwrite |
                                                           Automation::AudioExportLossyFormat),
                   QStringLiteral("preview must preserve all backend warning flags"));
        });
    }

    void testAudioCleanupDimensions(Matrix &matrix) {
        const auto operationId = Automation::OperationIds::exports::audio::cleanup;

        matrix.run(operationId, "AFD-EXP-CLEANUP-001-VALIDATE-RELEASED-JOB", [&] {
            RuntimeHarness harness;
            const auto taskId =
                finishAudioExport(harness, QStringLiteral("dimension-clean-validate.wav"));
            const auto preview =
                harness.runtime().audioExports().cleanup(harness.context(true), taskId);
            const auto commit = harness.runtime().audioExports().cleanup(harness.context(), taskId);
            EXPECT(matrix,
                   !taskId.isNull() && preview && preview.get().validatedOnly &&
                       !preview.get().changed && harness.audioExportState()->cleanupCount == 0 &&
                       commit && !commit.get().changed,
                   QStringLiteral(
                       "successful export must release its record before cleanup validation"));
        });

        matrix.run(operationId, "AFD-EXP-CLEANUP-002-COMMIT-NO-DOCUMENT-MUTATION", [&] {
            RuntimeHarness harness;
            const auto taskId =
                finishAudioExport(harness, QStringLiteral("dimension-clean-commit.wav"));
            const auto base = harness.runtime().documentVersion();
            const auto historyBefore = harness.runtime().history().getState(base.documentId);
            const auto result = harness.runtime().audioExports().cleanup(harness.context(), taskId);
            const auto historyAfter = harness.runtime().history().getState(base.documentId);
            EXPECT(matrix,
                   !taskId.isNull() && result && !result.get().changed &&
                       harness.audioExportState()->cleanupCount == 0 &&
                       harness.runtime().documentVersion() == base && historyBefore &&
                       historyAfter && sameHistory(historyBefore.get(), historyAfter.get()),
                   QStringLiteral(
                       "cleanup after success must be a no-op without document mutation"));
        });

        matrix.run(operationId, "AFD-EXP-CLEANUP-003-REPEATED-NO-OP", [&] {
            RuntimeHarness harness;
            const auto taskId =
                finishAudioExport(harness, QStringLiteral("dimension-clean-repeat.wav"));
            const auto first = harness.runtime().audioExports().cleanup(harness.context(), taskId);
            const auto repeated =
                harness.runtime().audioExports().cleanup(harness.context(), taskId);
            EXPECT(matrix,
                   !taskId.isNull() && first && !first.get().changed && repeated &&
                       !repeated.get().changed && harness.audioExportState()->cleanupCount == 0,
                   QStringLiteral("cleanup must remain idempotent after automatic record release"));
        });

        matrix.run(operationId, "AFD-EXP-CLEANUP-004-NONTERMINAL-BUSY", [&] {
            RuntimeHarness harness;
            const auto accepted = harness.runtime().audioExports().start(
                harness.context(), audioConfig(harness, QStringLiteral("dimension-clean-busy.wav")),
                {});
            const auto result =
                accepted ? harness.runtime().audioExports().cleanup(harness.context(),
                                                                    accepted.get().taskId)
                         : Automation::AutomationResult<Automation::ApplicationMutationResult>(
                               Automation::AutomationError{});
            EXPECT(matrix,
                   accepted &&
                       isError(result, Automation::AutomationErrorCode::Busy, operationId) &&
                       harness.audioExportState()->cleanupCount == 0,
                   QStringLiteral("cleanup must reject a queued or running export"));
        });

        matrix.run(operationId, "AFD-EXP-CLEANUP-005-UNKNOWN-TASK", [&] {
            RuntimeHarness harness;
            const auto result = harness.runtime().audioExports().cleanup(
                harness.context(), Automation::TaskId::create());
            EXPECT(matrix, isError(result, Automation::AutomationErrorCode::NotFound, operationId),
                   QStringLiteral("cleanup must reject an unknown TaskId"));
        });

        matrix.run(operationId, "AFD-EXP-CLEANUP-006-ERROR-PRIORITY", [&] {
            RuntimeHarness harness;
            const auto unknownTask = Automation::TaskId::create();
            auto wrongDocument = harness.context();
            wrongDocument.expected.documentId = Automation::DocumentId::create();
            ++wrongDocument.expected.revision;
            auto staleRevision = harness.context();
            ++staleRevision.expected.revision;
            const auto documentError =
                harness.runtime().audioExports().cleanup(wrongDocument, unknownTask);
            const auto revisionError =
                harness.runtime().audioExports().cleanup(staleRevision, unknownTask);
            const auto taskError =
                harness.runtime().audioExports().cleanup(harness.context(), unknownTask);
            EXPECT(matrix,
                   isError(documentError, Automation::AutomationErrorCode::DocumentChanged,
                           operationId) &&
                       isError(revisionError, Automation::AutomationErrorCode::RevisionConflict,
                               operationId) &&
                       isError(taskError, Automation::AutomationErrorCode::NotFound, operationId),
                   QStringLiteral("cleanup errors must order document, revision, then TaskId"));
        });

        matrix.run(operationId, "AFD-EXP-CLEANUP-007-GENERATION-FENCE", [&] {
            RuntimeHarness harness;
            const auto oldVersion = harness.runtime().documentVersion();
            const auto taskId =
                finishAudioExport(harness, QStringLiteral("dimension-clean-generation.wav"));
            const auto staleContext = RuntimeHarness::contextFor(oldVersion);
            const auto replacement = harness.runtime().documents().commitNewDocument(
                harness.context(), RuntimeHarness::emptyDocument());
            const auto oldCleanup = harness.runtime().audioExports().cleanup(staleContext, taskId);
            const auto newCleanup =
                harness.runtime().audioExports().cleanup(harness.context(), taskId);
            EXPECT(matrix,
                   !taskId.isNull() && replacement &&
                       isError(oldCleanup, Automation::AutomationErrorCode::DocumentChanged,
                               operationId) &&
                       isError(newCleanup, Automation::AutomationErrorCode::NotFound, operationId),
                   QStringLiteral("cleanup must not access jobs discarded with an old generation"));
        });
    }

    struct SessionStateDigest {
        Automation::DocumentSnapshotDto document;
        Automation::HistoryStateDto history;
        QByteArray serializedModel;
        QList<Automation::AutomationTaskSnapshot> tasks;
        QList<quintptr> objectAddresses;
        qsizetype idempotencyRecords = 0;
        int beforeReplaceCount = 0;
    };

    [[nodiscard]] std::optional<SessionStateDigest> captureSessionState(RuntimeHarness &harness) {
        const auto version = harness.runtime().documentVersion();
        const auto document = harness.runtime().documents().getDocument(version.documentId);
        const auto history = harness.runtime().history().getState(version.documentId);
        const auto idempotency = harness.runtime().dispatcher().dispatchDocumentQuery<qsizetype>(
            Automation::OperationIds::documents::get, version.documentId,
            [](Automation::DocumentSession &session) {
                return Automation::AutomationResult<qsizetype>(session.idempotencyStore().size());
            });
        if (!document || !history || !idempotency)
            return std::nullopt;

        SessionStateDigest result;
        result.document = document.get();
        result.history = history.get();
        result.serializedModel =
            QJsonDocument(harness.model().serialize()).toJson(QJsonDocument::Compact);
        result.tasks = harness.runtime().automationTasks().list(version.documentId);
        std::sort(result.tasks.begin(), result.tasks.end(),
                  [](const auto &left, const auto &right) {
                      return left.taskId.toString() < right.taskId.toString();
                  });
        for (const auto *track : harness.model().tracks()) {
            result.objectAddresses.append(reinterpret_cast<quintptr>(track));
            for (const auto *clip : track->clips())
                result.objectAddresses.append(reinterpret_cast<quintptr>(clip));
        }
        result.idempotencyRecords = idempotency.get();
        result.beforeReplaceCount = harness.beforeReplaceCount;
        return result;
    }

    [[nodiscard]] bool sameDocumentSnapshot(const Automation::DocumentSnapshotDto &left,
                                            const Automation::DocumentSnapshotDto &right) {
        return left.document == right.document && left.path == right.path &&
               left.projectName == right.projectName && left.lifecycle == right.lifecycle &&
               left.busy == right.busy && left.saved == right.saved;
    }

    [[nodiscard]] bool sameSessionState(const SessionStateDigest &left,
                                        const SessionStateDigest &right) {
        return sameDocumentSnapshot(left.document, right.document) &&
               sameHistory(left.history, right.history) &&
               left.serializedModel == right.serializedModel && left.tasks == right.tasks &&
               left.objectAddresses == right.objectAddresses &&
               left.idempotencyRecords == right.idempotencyRecords &&
               left.beforeReplaceCount == right.beforeReplaceCount;
    }

    [[nodiscard]] Automation::TrackDraftDto dimensionTrack(const QString &clientRef) {
        Automation::TrackDraftDto track;
        track.clientRef = clientRef + QStringLiteral("-track");
        track.name = QStringLiteral("Dimension Track ") + clientRef;
        track.gain = 1.0;
        track.defaultLanguage = QStringLiteral("en");

        Automation::ClipDraftDto clip;
        clip.clientRef = clientRef + QStringLiteral("-clip");
        clip.type = Automation::ClipDraftDto::Type::Singing;
        clip.properties.name = QStringLiteral("Dimension Clip ") + clientRef;
        clip.properties.length = 960;
        clip.properties.clipLen = 960;
        clip.properties.gain = 1.0;
        clip.defaultLanguage = QStringLiteral("en");
        track.clips.append(clip);
        return track;
    }

    [[nodiscard]] Automation::DocumentDraftDto dimensionDocument(const QString &clientRef) {
        auto draft = RuntimeHarness::emptyDocument();
        draft.tracks.clear();
        draft.tracks.append(dimensionTrack(clientRef));
        return draft;
    }

    [[nodiscard]] Automation::DocumentDraftDto invalidDimensionDocument(const QString &clientRef) {
        auto draft = dimensionDocument(clientRef);
        draft.tracks.first().colorIndex = -1;
        return draft;
    }

    [[nodiscard]] Automation::BatchImportDraftDto dimensionBatch(RuntimeHarness &harness,
                                                                 const QString &clientRef) {
        Automation::BatchImportDraftDto batch;
        batch.timeline = harness.model().timeline();
        Automation::BatchImportItemDraftDto item;
        item.existingTrackId = harness.trackId();
        Automation::ClipDraftDto clip;
        clip.clientRef = clientRef + QStringLiteral("-clip");
        clip.type = Automation::ClipDraftDto::Type::Audio;
        clip.properties.name = clientRef + QStringLiteral(".wav");
        clip.properties.length = 480;
        clip.properties.clipLen = 480;
        clip.properties.gain = 1.0;
        clip.audioPath = clientRef + QStringLiteral(".wav");
        clip.audioPathStatus = AudioClip::PathStatus::Missing;
        item.clips.append(clip);
        batch.items.append(item);
        return batch;
    }

    void testDocumentGetDimensions(Matrix &matrix) {
        const auto operationId = Automation::OperationIds::documents::get;

        matrix.run(operationId, "AFD-DOC-GET-001-ACTIVE-SNAPSHOT", [&] {
            RuntimeHarness harness;
            const auto version = harness.runtime().documentVersion();
            const auto result = harness.runtime().documents().getDocument(version.documentId);
            const auto *descriptor = harness.runtime().catalog().find(operationId);
            EXPECT(matrix,
                   descriptor && descriptor->kind == Automation::OperationKind::Query &&
                       descriptor->documentPolicy == Automation::DocumentPolicy::Read && result &&
                       result.get().document == version &&
                       result.get().lifecycle == Automation::DocumentLifecycleState::Active &&
                       !result.get().busy,
                   QStringLiteral("documents.get must return the active generation snapshot"));
        });

        matrix.run(operationId, "AFD-DOC-GET-002-REPEAT-NO-MUTATION", [&] {
            RuntimeHarness harness;
            const auto before = captureSessionState(harness);
            const auto version = harness.runtime().documentVersion();
            const auto first = harness.runtime().documents().getDocument(version.documentId);
            const auto second = harness.runtime().documents().getDocument(version.documentId);
            const auto after = captureSessionState(harness);
            EXPECT(matrix,
                   before && first && second && sameDocumentSnapshot(first.get(), second.get()) &&
                       after && sameSessionState(*before, *after),
                   QStringLiteral("repeated document snapshots must not touch session state"));
        });

        matrix.run(operationId, "AFD-DOC-GET-003-WRONG-DOCUMENT", [&] {
            RuntimeHarness harness;
            const auto result =
                harness.runtime().documents().getDocument(Automation::DocumentId::create());
            EXPECT(matrix,
                   isError(result, Automation::AutomationErrorCode::DocumentChanged, operationId),
                   QStringLiteral("documents.get must reject a foreign document generation"));
        });

        matrix.run(operationId, "AFD-DOC-GET-004-REPLACEMENT-ROUTING", [&] {
            RuntimeHarness harness;
            const auto oldVersion = harness.runtime().documentVersion();
            const auto replacement = harness.runtime().documents().commitNewDocument(
                harness.context(), dimensionDocument(QStringLiteral("get-replacement")));
            const auto newVersion = harness.runtime().documentVersion();
            const auto oldResult = harness.runtime().documents().getDocument(oldVersion.documentId);
            const auto newResult = harness.runtime().documents().getDocument(newVersion.documentId);
            EXPECT(matrix,
                   replacement && newVersion.documentId != oldVersion.documentId &&
                       isError(oldResult, Automation::AutomationErrorCode::DocumentChanged,
                               operationId) &&
                       newResult && newResult.get().document == newVersion,
                   QStringLiteral("document query routing must switch atomically on replacement"));
        });

        matrix.run(operationId, "AFD-DOC-GET-005-SAVED-FLAG", [&] {
            RuntimeHarness harness;
            const auto path = harness.temporaryPath(QStringLiteral("dimension-saved.dspx"));
            const auto firstSave =
                harness.runtime().documents().saveDocument(harness.context(), path);
            const auto saved = harness.runtime().documents().getDocument(
                harness.runtime().documentVersion().documentId);
            const auto edit = harness.runtime().timeline().setTempo(harness.context(), 1920, 141.0);
            const auto dirty = harness.runtime().documents().getDocument(
                harness.runtime().documentVersion().documentId);
            const auto secondSave =
                harness.runtime().documents().saveDocument(harness.context(), path);
            const auto resaved = harness.runtime().documents().getDocument(
                harness.runtime().documentVersion().documentId);
            EXPECT(matrix,
                   firstSave && saved && saved.get().saved && edit && dirty && !dirty.get().saved &&
                       secondSave && resaved && resaved.get().saved && resaved.get().path == path,
                   QStringLiteral("documents.get must reflect savepoint transitions"));
        });

        matrix.run(operationId, "AFD-DOC-GET-006-BUSY-LIFECYCLE", [&] {
            RuntimeHarness harness;
            const auto version = harness.runtime().documentVersion();
            const auto setup =
                harness.runtime().dispatcher().dispatchDocumentQuery<Automation::AutomationUnit>(
                    operationId, version.documentId, [](Automation::DocumentSession &session) {
                        session.setLifecycleState(Automation::DocumentLifecycleState::Replacing);
                        return Automation::AutomationResult<Automation::AutomationUnit>(
                            Automation::AutomationUnit{});
                    });
            const auto result = harness.runtime().documents().getDocument(version.documentId);
            EXPECT(matrix,
                   setup && isError(result, Automation::AutomationErrorCode::Busy, operationId),
                   QStringLiteral("documents.get must reject a non-active lifecycle"));
        });
    }

    enum class ReplacementKind {
        NewDocument,
        OpenDocument,
    };

    struct ReplacementCase {
        ReplacementKind kind;
        Automation::OperationId operationId;
    };

    [[nodiscard]] QList<ReplacementCase> replacementCases() {
        return {
            {ReplacementKind::NewDocument,  Automation::OperationIds::documents::commit_new },
            {ReplacementKind::OpenDocument, Automation::OperationIds::documents::commit_open},
        };
    }

    [[nodiscard]] Automation::AutomationResult<Automation::MutationResult>
        invokeReplacement(RuntimeHarness &harness, const ReplacementCase &testCase,
                          const Automation::CommandContext &context,
                          const Automation::DocumentDraftDto &draft, const QString &discriminator) {
        if (testCase.kind == ReplacementKind::NewDocument)
            return harness.runtime().documents().commitNewDocument(context, draft);
        const auto name = QStringLiteral("dimension-open-%1.dspx").arg(discriminator);
        return harness.runtime().documents().commitOpenedDocument(
            context, draft, harness.temporaryPath(name), name, true);
    }

    [[nodiscard]] QString replacementScenario(const int index, const int scenario,
                                              const QString &suffix) {
        return QStringLiteral("AFD-DOC-REPLACE-%1-%2-%3")
            .arg(index, 2, 10, QLatin1Char('0'))
            .arg(scenario, 3, 10, QLatin1Char('0'))
            .arg(suffix);
    }

    void testDocumentReplacementDimensions(Matrix &matrix) {
        const auto cases = replacementCases();
        for (int index = 0; index < cases.size(); ++index) {
            const auto testCase = cases.at(index);

            matrix.run(
                testCase.operationId,
                replacementScenario(index + 1, 1, QStringLiteral("VALIDATE-SNAPSHOT")), [&] {
                    RuntimeHarness harness;
                    const auto before = captureSessionState(harness);
                    const auto result = invokeReplacement(
                        harness, testCase, harness.context(true),
                        dimensionDocument(QStringLiteral("replace-validate-%1").arg(index)),
                        QStringLiteral("validate-%1").arg(index));
                    const auto after = captureSessionState(harness);
                    EXPECT(matrix,
                           before && result && result.get().validatedOnly && result.get().changed &&
                               result.get().previous == before->document.document &&
                               result.get().current.documentId.isNull() &&
                               result.get().current.revision == 0 && after &&
                               sameSessionState(*before, *after),
                           QStringLiteral("replacement validation must not allocate a generation"));
                });

            matrix.run(testCase.operationId,
                       replacementScenario(index + 1, 2, QStringLiteral("ATOMIC-COMMIT")), [&] {
                           RuntimeHarness harness;
                           const auto oldVersion = harness.runtime().documentVersion();
                           int cancelCount = 0;
                           harness.runtime().automationTasks().createTask(
                               Automation::OperationIds::extract::midi::start, oldVersion,
                               std::nullopt, [&cancelCount] { ++cancelCount; });
                           const auto result = invokeReplacement(
                               harness, testCase, harness.context(),
                               dimensionDocument(QStringLiteral("replace-commit-%1").arg(index)),
                               QStringLiteral("commit-%1").arg(index));
                           const auto newVersion = harness.runtime().documentVersion();
                           const auto snapshot =
                               harness.runtime().documents().getDocument(newVersion.documentId);
                           EXPECT(matrix,
                                  result && result.get().changed &&
                                      result.get().previous == oldVersion &&
                                      result.get().current == newVersion &&
                                      newVersion.documentId != oldVersion.documentId &&
                                      newVersion.revision == 0 &&
                                      result.get().createdObjects.size() == 2 &&
                                      harness.model().tracks().size() == 1 &&
                                      harness.beforeReplaceCount == 1 && cancelCount == 1 &&
                                      harness.runtime().automationTasks().size() == 0 && snapshot &&
                                      snapshot.get().saved &&
                                      (testCase.kind == ReplacementKind::OpenDocument
                                           ? !snapshot.get().path.isEmpty() &&
                                                 !snapshot.get().projectName.isEmpty()
                                           : snapshot.get().path.isEmpty()),
                                  QStringLiteral("replacement must swap generation, model, tasks, "
                                                 "and metadata atomically"));
                       });

            matrix.run(
                testCase.operationId,
                replacementScenario(index + 1, 3, QStringLiteral("INVALID-FULL-ROLLBACK")), [&] {
                    RuntimeHarness harness;
                    const auto before = captureSessionState(harness);
                    const auto result = invokeReplacement(
                        harness, testCase, harness.context(),
                        invalidDimensionDocument(QStringLiteral("replace-invalid-%1").arg(index)),
                        QStringLiteral("invalid-%1").arg(index));
                    const auto after = captureSessionState(harness);
                    EXPECT(matrix,
                           before &&
                               isError(result, Automation::AutomationErrorCode::InvalidArgument,
                                       testCase.operationId) &&
                               after && sameSessionState(*before, *after),
                           QStringLiteral(
                               "invalid replacement must preserve the complete active session"));
                });

            matrix.run(
                testCase.operationId,
                replacementScenario(index + 1, 4, QStringLiteral("ERROR-PRIORITY")), [&] {
                    RuntimeHarness harness;
                    auto wrongDocument = harness.context();
                    wrongDocument.expected.documentId = Automation::DocumentId::create();
                    ++wrongDocument.expected.revision;
                    auto staleRevision = harness.context();
                    ++staleRevision.expected.revision;
                    const auto invalid =
                        invalidDimensionDocument(QStringLiteral("replace-priority-%1").arg(index));
                    const auto documentError =
                        invokeReplacement(harness, testCase, wrongDocument, invalid,
                                          QStringLiteral("document-%1").arg(index));
                    const auto revisionError =
                        invokeReplacement(harness, testCase, staleRevision, invalid,
                                          QStringLiteral("revision-%1").arg(index));
                    EXPECT(
                        matrix,
                        isError(documentError, Automation::AutomationErrorCode::DocumentChanged,
                                testCase.operationId) &&
                            isError(revisionError,
                                    Automation::AutomationErrorCode::RevisionConflict,
                                    testCase.operationId),
                        QStringLiteral(
                            "replacement must gate document and revision before draft validation"));
                });

            matrix.run(
                testCase.operationId,
                replacementScenario(index + 1, 5, QStringLiteral("IDEMPOTENCY-UNSUPPORTED")), [&] {
                    RuntimeHarness harness;
                    const auto before = captureSessionState(harness);
                    auto context =
                        keyedContext(harness, QStringLiteral("afda4000-0000-4000-8000-%1")
                                                  .arg(index + 1, 12, 10, QLatin1Char('0')));
                    const auto result = invokeReplacement(
                        harness, testCase, context,
                        dimensionDocument(QStringLiteral("replace-key-%1").arg(index)),
                        QStringLiteral("key-%1").arg(index));
                    const auto after = captureSessionState(harness);
                    EXPECT(matrix,
                           before &&
                               isError(result, Automation::AutomationErrorCode::InvalidArgument,
                                       testCase.operationId) &&
                               result.getError().fieldPath == QStringLiteral("idempotency_key") &&
                               after && sameSessionState(*before, *after),
                           QStringLiteral(
                               "generation replacement must reject unsupported idempotency keys"));
                });

            matrix.run(
                testCase.operationId,
                replacementScenario(index + 1, 6, QStringLiteral("GENERATION-FENCE")), [&] {
                    RuntimeHarness harness;
                    const auto staleContext = harness.context();
                    const auto first = invokeReplacement(
                        harness, testCase, harness.context(),
                        dimensionDocument(QStringLiteral("replace-first-%1").arg(index)),
                        QStringLiteral("first-%1").arg(index));
                    const auto second = invokeReplacement(
                        harness, testCase, staleContext,
                        dimensionDocument(QStringLiteral("replace-late-%1").arg(index)),
                        QStringLiteral("late-%1").arg(index));
                    EXPECT(matrix,
                           first &&
                               isError(second, Automation::AutomationErrorCode::DocumentChanged,
                                       testCase.operationId),
                           QStringLiteral("prepared replacement must never cross a generation"));
                });
        }
    }

    void testDocumentImportDimensions(Matrix &matrix) {
        const auto operationId = Automation::OperationIds::documents::commit_import;

        matrix.run(operationId, "AFD-DOC-IMPORT-001-VALIDATE-SNAPSHOT", [&] {
            RuntimeHarness harness;
            const auto before = captureSessionState(harness);
            const auto result = harness.runtime().documents().commitImportedDocument(
                harness.context(true), dimensionDocument(QStringLiteral("document-import-preview")),
                false, false);
            const auto after = captureSessionState(harness);
            EXPECT(matrix,
                   before && result && result.get().validatedOnly && result.get().changed &&
                       result.get().current.revision == before->document.document.revision + 1 &&
                       result.get().createdObjects.isEmpty() && after &&
                       sameSessionState(*before, *after),
                   QStringLiteral("document import validation must not allocate or mutate"));
        });

        matrix.run(operationId, "AFD-DOC-IMPORT-002-LEGAL-NO-OP", [&] {
            RuntimeHarness harness;
            Automation::DocumentDraftDto empty;
            empty.timeline = harness.model().timeline();
            const auto before = captureSessionState(harness);
            const auto result = harness.runtime().documents().commitImportedDocument(
                harness.context(), empty, false, false);
            const auto after = captureSessionState(harness);
            EXPECT(matrix,
                   before && result && !result.get().changed && after &&
                       sameSessionState(*before, *after),
                   QStringLiteral("empty disabled-timeline import must be a true no-op"));
        });

        matrix.run(operationId, "AFD-DOC-IMPORT-003-ATOMIC-COMMIT", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto tracksBefore = harness.model().tracks().size();
            const auto result = harness.runtime().documents().commitImportedDocument(
                harness.context(), dimensionDocument(QStringLiteral("document-import-commit")),
                false, false);
            EXPECT(
                matrix,
                result && result.get().changed &&
                    result.get().current.revision == base.revision + 1 &&
                    result.get().createdObjects.size() == 2 &&
                    harness.model().tracks().size() == tracksBefore + 1,
                QStringLiteral("document import must commit one revision and its created objects"));
        });

        matrix.run(operationId, "AFD-DOC-IMPORT-004-INVALID-FULL-ROLLBACK", [&] {
            RuntimeHarness harness;
            const auto before = captureSessionState(harness);
            const auto result = harness.runtime().documents().commitImportedDocument(
                harness.context(),
                invalidDimensionDocument(QStringLiteral("document-import-invalid")), false, false);
            const auto after = captureSessionState(harness);
            EXPECT(matrix,
                   before &&
                       isError(result, Automation::AutomationErrorCode::InvalidArgument,
                               operationId) &&
                       after && sameSessionState(*before, *after),
                   QStringLiteral("invalid document import must preserve complete session state"));
        });

        matrix.run(operationId, "AFD-DOC-IMPORT-005-ERROR-PRIORITY", [&] {
            RuntimeHarness harness;
            auto wrongDocument = harness.context();
            wrongDocument.expected.documentId = Automation::DocumentId::create();
            ++wrongDocument.expected.revision;
            auto staleRevision = harness.context();
            ++staleRevision.expected.revision;
            const auto invalid =
                invalidDimensionDocument(QStringLiteral("document-import-priority"));
            const auto documentError = harness.runtime().documents().commitImportedDocument(
                wrongDocument, invalid, false, false);
            const auto revisionError = harness.runtime().documents().commitImportedDocument(
                staleRevision, invalid, false, false);
            EXPECT(matrix,
                   isError(documentError, Automation::AutomationErrorCode::DocumentChanged,
                           operationId) &&
                       isError(revisionError, Automation::AutomationErrorCode::RevisionConflict,
                               operationId),
                   QStringLiteral(
                       "document import must gate document and revision before validation"));
        });

        matrix.run(operationId, "AFD-DOC-IMPORT-006-IDEMPOTENT-REPLAY", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto tracksBefore = harness.model().tracks().size();
            const auto context =
                keyedContext(harness, QStringLiteral("afda5000-0000-4000-8000-000000000001"));
            const auto draft = dimensionDocument(QStringLiteral("document-import-key"));
            const auto first =
                harness.runtime().documents().commitImportedDocument(context, draft, false, false);
            const auto replay =
                harness.runtime().documents().commitImportedDocument(context, draft, false, false);
            EXPECT(matrix,
                   first && replay && replay.get() == first.get() &&
                       harness.runtime().documentVersion().revision == base.revision + 1 &&
                       harness.model().tracks().size() == tracksBefore + 1,
                   QStringLiteral("document import replay must commit exactly once"));
        });

        matrix.run(operationId, "AFD-DOC-IMPORT-007-GENERATION-FENCE", [&] {
            RuntimeHarness harness;
            const auto staleContext = harness.context();
            const auto replacement = harness.runtime().documents().commitNewDocument(
                harness.context(), dimensionDocument(QStringLiteral("import-generation")));
            const auto result = harness.runtime().documents().commitImportedDocument(
                staleContext, dimensionDocument(QStringLiteral("import-late")), false, false);
            EXPECT(matrix,
                   replacement && isError(result, Automation::AutomationErrorCode::DocumentChanged,
                                          operationId),
                   QStringLiteral("late document import must not cross a replacement generation"));
        });
    }

    void testDocumentSaveDimensions(Matrix &matrix) {
        const auto operationId = Automation::OperationIds::documents::save;

        matrix.run(operationId, "AFD-DOC-SAVE-001-VALIDATE-NO-IO", [&] {
            RuntimeHarness harness;
            const auto before = captureSessionState(harness);
            const auto path = harness.temporaryPath(QStringLiteral("dimension-save-preview.dspx"));
            const auto result =
                harness.runtime().documents().saveDocument(harness.context(true), path);
            const auto after = captureSessionState(harness);
            EXPECT(matrix,
                   before && result && result.get().validatedOnly && result.get().changed &&
                       harness.saveCount == 0 && after && sameSessionState(*before, *after),
                   QStringLiteral("save validation must not call the writer or alter metadata"));
        });

        matrix.run(operationId, "AFD-DOC-SAVE-002-COMMIT-METADATA", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto path = harness.temporaryPath(QStringLiteral("dimension-save-commit.dspx"));
            const auto result = harness.runtime().documents().saveDocument(harness.context(), path);
            const auto snapshot = harness.runtime().documents().getDocument(base.documentId);
            EXPECT(matrix,
                   result && result.get().changed && !result.get().validatedOnly &&
                       result.get().current == base &&
                       harness.runtime().documentVersion() == base && harness.saveCount == 1 &&
                       harness.lastSavePath == path && snapshot && snapshot.get().path == path &&
                       snapshot.get().projectName == QFileInfo(path).fileName() &&
                       snapshot.get().saved,
                   QStringLiteral("save must update path/savepoint without changing revision"));
        });

        matrix.run(operationId, "AFD-DOC-SAVE-003-IO-FAILURE-ROLLBACK-RETRY", [&] {
            RuntimeHarness harness;
            harness.saveSucceeds = false;
            const auto context =
                keyedContext(harness, QStringLiteral("afda6000-0000-4000-8000-000000000001"));
            const auto path = harness.temporaryPath(QStringLiteral("dimension-save-failed.dspx"));
            const auto before = captureSessionState(harness);
            const auto failed = harness.runtime().documents().saveDocument(context, path);
            const auto afterFailure = captureSessionState(harness);
            harness.saveSucceeds = true;
            const auto retried = harness.runtime().documents().saveDocument(context, path);
            EXPECT(
                matrix,
                before && isError(failed, Automation::AutomationErrorCode::IoError, operationId) &&
                    afterFailure && sameSessionState(*before, *afterFailure) && retried &&
                    harness.saveCount == 2,
                QStringLiteral("failed save must roll back fully and release its idempotency key"));
        });

        matrix.run(operationId, "AFD-DOC-SAVE-004-HOST-UNAVAILABLE", [&] {
            RuntimeHarness harness({.documentServices = false});
            const auto before = captureSessionState(harness);
            const auto result = harness.runtime().documents().saveDocument(
                harness.context(),
                harness.temporaryPath(QStringLiteral("dimension-save-host.dspx")));
            const auto after = captureSessionState(harness);
            EXPECT(matrix,
                   before &&
                       isError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                               operationId) &&
                       after && sameSessionState(*before, *after),
                   QStringLiteral("missing save host must preserve complete session state"));
        });

        matrix.run(operationId, "AFD-DOC-SAVE-005-INPUT-BEFORE-HOST", [&] {
            RuntimeHarness harness({.documentServices = false});
            const auto before = captureSessionState(harness);
            const auto result =
                harness.runtime().documents().saveDocument(harness.context(), QStringLiteral("  "));
            const auto after = captureSessionState(harness);
            EXPECT(matrix,
                   before &&
                       isError(result, Automation::AutomationErrorCode::InvalidArgument,
                               operationId) &&
                       after && sameSessionState(*before, *after),
                   QStringLiteral("empty save path must fail before host availability"));
        });

        matrix.run(operationId, "AFD-DOC-SAVE-006-ERROR-PRIORITY", [&] {
            RuntimeHarness harness({.documentServices = false});
            auto wrongDocument = harness.context();
            wrongDocument.expected.documentId = Automation::DocumentId::create();
            ++wrongDocument.expected.revision;
            auto staleRevision = harness.context();
            ++staleRevision.expected.revision;
            const auto documentError =
                harness.runtime().documents().saveDocument(wrongDocument, {});
            const auto revisionError =
                harness.runtime().documents().saveDocument(staleRevision, {});
            const auto inputError =
                harness.runtime().documents().saveDocument(harness.context(), {});
            EXPECT(matrix,
                   isError(documentError, Automation::AutomationErrorCode::DocumentChanged,
                           operationId) &&
                       isError(revisionError, Automation::AutomationErrorCode::RevisionConflict,
                               operationId) &&
                       isError(inputError, Automation::AutomationErrorCode::InvalidArgument,
                               operationId),
                   QStringLiteral("save errors must order document, revision, input, then host"));
        });

        matrix.run(operationId, "AFD-DOC-SAVE-007-IDEMPOTENT-WRITE", [&] {
            RuntimeHarness harness;
            const auto context =
                keyedContext(harness, QStringLiteral("afda6000-0000-4000-8000-000000000002"));
            const auto path = harness.temporaryPath(QStringLiteral("dimension-save-key.dspx"));
            const auto first = harness.runtime().documents().saveDocument(context, path);
            const auto replay = harness.runtime().documents().saveDocument(context, path);
            EXPECT(matrix, first && replay && replay.get() == first.get() && harness.saveCount == 1,
                   QStringLiteral("save idempotency replay must call the writer exactly once"));
        });

        matrix.run(operationId, "AFD-DOC-SAVE-008-GENERATION-FENCE", [&] {
            RuntimeHarness harness;
            const auto staleContext = harness.context();
            const auto replacement = harness.runtime().documents().commitNewDocument(
                harness.context(), dimensionDocument(QStringLiteral("save-generation")));
            const auto result = harness.runtime().documents().saveDocument(
                staleContext, harness.temporaryPath(QStringLiteral("dimension-save-late.dspx")));
            EXPECT(matrix,
                   replacement &&
                       isError(result, Automation::AutomationErrorCode::DocumentChanged,
                               operationId) &&
                       harness.saveCount == 0,
                   QStringLiteral("late save must not write across a replacement generation"));
        });
    }

    void testBatchImportDimensions(Matrix &matrix) {
        const auto operationId = Automation::OperationIds::imports::commit_batch;

        matrix.run(operationId, "AFD-IMPORT-BATCH-001-VALIDATE-SNAPSHOT", [&] {
            RuntimeHarness harness;
            const auto before = captureSessionState(harness);
            const auto result = harness.runtime().project().commitBatchImport(
                harness.context(true), dimensionBatch(harness, QStringLiteral("batch-preview")));
            const auto after = captureSessionState(harness);
            EXPECT(matrix,
                   before && result && result.get().validatedOnly && result.get().changed &&
                       result.get().createdObjects.isEmpty() && after &&
                       sameSessionState(*before, *after),
                   QStringLiteral("batch validation must not allocate or mutate"));
        });

        matrix.run(operationId, "AFD-IMPORT-BATCH-002-LEGAL-NO-OP", [&] {
            RuntimeHarness harness;
            Automation::BatchImportDraftDto empty;
            empty.timeline = harness.model().timeline();
            const auto before = captureSessionState(harness);
            const auto result =
                harness.runtime().project().commitBatchImport(harness.context(), empty);
            const auto after = captureSessionState(harness);
            EXPECT(matrix,
                   before && result && !result.get().changed && after &&
                       sameSessionState(*before, *after),
                   QStringLiteral("empty unchanged batch must be a true no-op"));
        });

        matrix.run(operationId, "AFD-IMPORT-BATCH-003-ATOMIC-COMMIT", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto clipsBefore = harness.model().tracks().first()->clips().count();
            const auto result = harness.runtime().project().commitBatchImport(
                harness.context(), dimensionBatch(harness, QStringLiteral("batch-commit")));
            EXPECT(matrix,
                   result && result.get().changed &&
                       result.get().current.revision == base.revision + 1 &&
                       result.get().createdObjects.size() == 1 &&
                       harness.model().tracks().first()->clips().count() == clipsBefore + 1,
                   QStringLiteral("batch import must atomically add one clip and revision"));
        });

        matrix.run(operationId, "AFD-IMPORT-BATCH-004-INVALID-FULL-ROLLBACK", [&] {
            RuntimeHarness harness;
            Automation::BatchImportDraftDto invalid;
            invalid.timeline = harness.model().timeline();
            Automation::BatchImportItemDraftDto emptyItem;
            emptyItem.existingTrackId = harness.trackId();
            invalid.items.append(emptyItem);
            const auto before = captureSessionState(harness);
            const auto result =
                harness.runtime().project().commitBatchImport(harness.context(), invalid);
            const auto after = captureSessionState(harness);
            EXPECT(matrix,
                   before &&
                       isError(result, Automation::AutomationErrorCode::InvalidArgument,
                               operationId) &&
                       after && sameSessionState(*before, *after),
                   QStringLiteral("invalid batch must preserve complete session state"));
        });

        matrix.run(operationId, "AFD-IMPORT-BATCH-005-MISSING-TARGET-ROLLBACK", [&] {
            RuntimeHarness harness;
            auto batch = dimensionBatch(harness, QStringLiteral("batch-missing"));
            batch.items.first().existingTrackId = Automation::TrackId(999981);
            const auto before = captureSessionState(harness);
            const auto result =
                harness.runtime().project().commitBatchImport(harness.context(), batch);
            const auto after = captureSessionState(harness);
            EXPECT(matrix,
                   before &&
                       isError(result, Automation::AutomationErrorCode::NotFound, operationId) &&
                       after && sameSessionState(*before, *after),
                   QStringLiteral("missing batch target must not partially import clips"));
        });

        matrix.run(operationId, "AFD-IMPORT-BATCH-006-ERROR-PRIORITY", [&] {
            RuntimeHarness harness;
            auto wrongDocument = harness.context();
            wrongDocument.expected.documentId = Automation::DocumentId::create();
            ++wrongDocument.expected.revision;
            auto staleRevision = harness.context();
            ++staleRevision.expected.revision;
            Automation::BatchImportDraftDto invalid;
            const auto documentError =
                harness.runtime().project().commitBatchImport(wrongDocument, invalid);
            const auto revisionError =
                harness.runtime().project().commitBatchImport(staleRevision, invalid);
            EXPECT(matrix,
                   isError(documentError, Automation::AutomationErrorCode::DocumentChanged,
                           operationId) &&
                       isError(revisionError, Automation::AutomationErrorCode::RevisionConflict,
                               operationId),
                   QStringLiteral("batch import must gate document and revision before payload"));
        });

        matrix.run(operationId, "AFD-IMPORT-BATCH-007-IDEMPOTENT-REPLAY", [&] {
            RuntimeHarness harness;
            const auto base = harness.runtime().documentVersion();
            const auto clipsBefore = harness.model().tracks().first()->clips().count();
            const auto context =
                keyedContext(harness, QStringLiteral("afda7000-0000-4000-8000-000000000001"));
            const auto batch = dimensionBatch(harness, QStringLiteral("batch-key"));
            const auto first = harness.runtime().project().commitBatchImport(context, batch);
            const auto replay = harness.runtime().project().commitBatchImport(context, batch);
            EXPECT(matrix,
                   first && replay && replay.get() == first.get() &&
                       harness.runtime().documentVersion().revision == base.revision + 1 &&
                       harness.model().tracks().first()->clips().count() == clipsBefore + 1,
                   QStringLiteral("batch idempotency replay must commit exactly once"));
        });
    }

    [[nodiscard]] QList<Automation::OperationId> expectedDimensionOperations() {
        QList<Automation::OperationId> operations{
            Automation::OperationIds::documents::commit_import,
            Automation::OperationIds::documents::commit_new,
            Automation::OperationIds::documents::commit_open,
            Automation::OperationIds::documents::get,
            Automation::OperationIds::documents::save,
            Automation::OperationIds::audio_clips::apply_decode_cache,
            Automation::OperationIds::audio_clips::apply_resolved_path,
            Automation::OperationIds::audio_clips::confirm_path,
            Automation::OperationIds::audio_clips::relocate,
            Automation::OperationIds::audio_clips::set_hash,
            Automation::OperationIds::audio_clips::set_path_status,
            Automation::OperationIds::imports::commit_batch,
            Automation::OperationIds::extract::midi::start,
            Automation::OperationIds::extract::pitch::start,
            Automation::OperationIds::exports::audio::cleanup,
            Automation::OperationIds::exports::audio::preview,
            Automation::OperationIds::exports::audio::start,
            Automation::OperationIds::exports::midi::start,
            Automation::OperationIds::formats::list,
            Automation::OperationIds::tasks::cancel,
            Automation::OperationIds::tasks::get,
            Automation::OperationIds::tasks::list,
        };
        for (const auto &testCase : inferenceDimensionCases())
            operations.append(testCase.operationId);
        return operations;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    Matrix matrix;
    testMidiExtraction(matrix);
    testPitchExtraction(matrix);
    testAudioExport(matrix);
    testOperationQueries(matrix);
    testInferenceDimensions(matrix);
    testAudioClipDimensions(matrix);
    testFormatsDimensions(matrix);
    testMidiExportDimensions(matrix);
    testAudioPreviewDimensions(matrix);
    testAudioCleanupDimensions(matrix);
    testDocumentGetDimensions(matrix);
    testDocumentReplacementDimensions(matrix);
    testDocumentImportDimensions(matrix);
    testDocumentSaveDimensions(matrix);
    testBatchImportDimensions(matrix);

    matrix.requireAtLeast(Automation::OperationIds::extract::midi::start, 9);
    matrix.requireAtLeast(Automation::OperationIds::extract::pitch::start, 9);
    matrix.requireAtLeast(Automation::OperationIds::exports::audio::start, 9);
    matrix.requireAtLeast(Automation::OperationIds::tasks::get, 6);
    matrix.requireAtLeast(Automation::OperationIds::tasks::list, 6);
    matrix.requireAtLeast(Automation::OperationIds::tasks::cancel, 6);
    for (const auto &testCase : inferenceDimensionCases())
        matrix.requireAtLeast(testCase.operationId, 6);
    for (const auto &testCase : audioDimensionCases())
        matrix.requireAtLeast(testCase.operationId, 8);
    matrix.requireAtLeast(Automation::OperationIds::formats::list, 6);
    matrix.requireAtLeast(Automation::OperationIds::exports::midi::start, 8);
    matrix.requireAtLeast(Automation::OperationIds::exports::audio::preview, 7);
    matrix.requireAtLeast(Automation::OperationIds::exports::audio::cleanup, 7);
    matrix.requireAtLeast(Automation::OperationIds::documents::get, 6);
    matrix.requireAtLeast(Automation::OperationIds::documents::commit_new, 6);
    matrix.requireAtLeast(Automation::OperationIds::documents::commit_open, 6);
    matrix.requireAtLeast(Automation::OperationIds::documents::commit_import, 7);
    matrix.requireAtLeast(Automation::OperationIds::documents::save, 8);
    matrix.requireAtLeast(Automation::OperationIds::imports::commit_batch, 7);
    const auto expectedOperations = expectedDimensionOperations();
    matrix.requireOperationsExactly(expectedOperations);
    for (const auto &operationId : expectedOperations)
        matrix.requireBetween(operationId, 6, 9);
    matrix.requireDimension("Queued", 1);
    matrix.requireDimension("Running", 1);
    matrix.requireDimension("CancelRequested", 1);
    matrix.requireDimension("Committing", 1);
    matrix.requireDimension("terminal", 1);
    const QList<Automation::OperationId> asyncOperations{
        Automation::OperationIds::extract::midi::start,
        Automation::OperationIds::extract::pitch::start,
        Automation::OperationIds::exports::audio::start,
        Automation::OperationIds::tasks::cancel,
    };
    for (const auto &operationId : asyncOperations) {
        matrix.requireAsyncDimension(operationId, "Queued");
        matrix.requireAsyncDimension(operationId, "Running");
        matrix.requireAsyncDimension(operationId, "CancelRequested");
        matrix.requireAsyncDimension(operationId, "Committing");
        matrix.requireAsyncDimension(operationId, "terminal");
    }
    return matrix.finish();
}
