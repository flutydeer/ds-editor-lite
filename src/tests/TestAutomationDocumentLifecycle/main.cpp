#include "Automation/AutomationDispatcher.h"
#include "Automation/CoreRuntime.h"
#include "Automation/OperationIds.h"
#include "Automation/ProjectAutomationDtos.h"
#include "TestRuntime.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QCoreApplication>
#include <QDataStream>
#include <QIODevice>
#include <QJsonDocument>
#include <QTextStream>

#include <algorithm>
#include <functional>
#include <limits>
#include <optional>
#include <utility>

namespace {
    class TestRun final {
    public:
        void expect(const bool condition, const char *scenario, const char *message) {
            ++m_assertions;
            if (condition)
                return;
            m_ok = false;
            QTextStream(stderr) << "FAILED [" << scenario << "]: " << message << Qt::endl;
        }

        [[nodiscard]] bool ok() const {
            return m_ok;
        }

        [[nodiscard]] int assertions() const {
            return m_assertions;
        }

    private:
        bool m_ok = true;
        int m_assertions = 0;
    };

    struct DocumentHostState {
        LoopSettings loopSettings;
        bool saveSucceeds = true;
        int saveCalls = 0;
        QString lastSavePath;
        QList<Automation::DocumentId> replacementNotifications;
    };

    Automation::DocumentRuntimeServices documentServices(DocumentHostState &host) {
        Automation::DocumentRuntimeServices services;
        services.applyLoopSettings = [&host](const LoopSettings &settings) {
            host.loopSettings = settings;
        };
        services.saveProject = [&host](const QString &path, AppModel *, QString &errorMessage) {
            ++host.saveCalls;
            host.lastSavePath = path;
            if (host.saveSucceeds)
                return true;
            errorMessage = QStringLiteral("controlled save failure");
            return false;
        };
        services.beforeReplaceGeneration = [&host](const Automation::DocumentId &documentId) {
            host.replacementNotifications.append(documentId);
        };
        return services;
    }

    class LifecycleFixture final {
    public:
        LifecycleFixture()
            : history(resetHistory()), runtime(&model, history, documentServices(host)) {
        }

        ~LifecycleFixture() {
            history->reset();
        }

        AppModel model;
        HistoryManager *history;
        DocumentHostState host;
        Automation::CoreRuntime runtime;

    private:
        static HistoryManager *resetHistory() {
            auto *history = HistoryManager::instance();
            history->reset(HistoryManager::ResetState::Saved);
            return history;
        }
    };

    Automation::CommandContext commandContext(const Automation::CoreRuntime &runtime,
                                              QString idempotencyKey = {},
                                              const bool validateOnly = false) {
        return {
            .expected = runtime.documentVersion(),
            .validateOnly = validateOnly,
            .idempotencyKey = std::move(idempotencyKey),
            .source = Automation::InvocationSource::Test,
        };
    }

    Automation::DocumentDraftDto makeDocumentDraft(const QString &trackName,
                                                   const QString &clientRef,
                                                   const LoopSettings &loopSettings = {}) {
        AppModel source;
        source.newProject();
        auto draft = Automation::documentDraftDto(source.takeProjectData(), loopSettings);
        if (!draft.tracks.isEmpty()) {
            auto &track = draft.tracks.first();
            track.name = trackName;
            track.clientRef = clientRef + QStringLiteral("-track");
            if (!track.clips.isEmpty()) {
                track.clips.first().properties.name = trackName + QStringLiteral(" Clip");
                track.clips.first().clientRef = clientRef + QStringLiteral("-clip");
            }
        }
        return draft;
    }

    QByteArray projectDigest(const Automation::ProjectSnapshotDto &project) {
        QByteArray result;
        QDataStream stream(&result, QIODevice::WriteOnly);
        stream << project.document.documentId.toString() << project.document.revision
               << project.tracks.size();
        for (const auto &track : project.tracks) {
            auto completeTrack = track.data;
            stream << track.id.value() << track.clips.size();
            for (const auto &clip : track.clips) {
                completeTrack.clips.append(clip.data);
                stream << clip.id.value() << clip.trackId.value();
            }
            stream << Automation::fingerprint(completeTrack);
        }
        return result;
    }

    QByteArray timelineDigest(const Automation::TimelineSnapshotDto &timeline) {
        QByteArray result;
        QDataStream stream(&result, QIODevice::WriteOnly);
        stream << timeline.document.documentId.toString() << timeline.document.revision
               << timeline.tempos.size();
        for (const auto &tempo : timeline.tempos)
            stream << tempo.pos << tempo.value;
        stream << timeline.timeSignatures.size();
        for (const auto &signature : timeline.timeSignatures)
            stream << signature.barIndex << signature.numerator << signature.denominator;
        return result;
    }

    struct StateDigest {
        Automation::DocumentSnapshotDto document;
        Automation::HistoryStateDto history;
        QByteArray project;
        QByteArray timeline;
        QByteArray serializedModel;
        QList<Automation::AutomationTaskSnapshot> tasks;
        QList<quintptr> objectAddresses;
        LoopSettings loopSettings;
        qsizetype idempotencyRecords = 0;
        int replacementNotifications = 0;
    };

    std::optional<StateDigest> captureState(LifecycleFixture &fixture) {
        const auto version = fixture.runtime.documentVersion();
        const auto document = fixture.runtime.documents().getDocument(version.documentId);
        const auto history = fixture.runtime.history().getState(version.documentId);
        const auto project = fixture.runtime.project().getProject(version.documentId);
        const auto timeline = fixture.runtime.timeline().getTimeline(version.documentId);
        const auto idempotencyRecords =
            fixture.runtime.dispatcher().dispatchDocumentQuery<qsizetype>(
                Automation::OperationIds::documents::get, version.documentId,
                [](Automation::DocumentSession &session) {
                    return Automation::AutomationResult<qsizetype>(
                        session.idempotencyStore().size());
                });
        if (!document || !history || !project || !timeline || !idempotencyRecords)
            return std::nullopt;

        StateDigest result;
        result.document = document.get();
        result.history = history.get();
        result.project = projectDigest(project.get());
        result.timeline = timelineDigest(timeline.get());
        result.serializedModel =
            QJsonDocument(fixture.model.serialize()).toJson(QJsonDocument::Compact);
        result.tasks = fixture.runtime.automationTasks().list(version.documentId);
        std::sort(result.tasks.begin(), result.tasks.end(),
                  [](const auto &left, const auto &right) {
                      return left.taskId.toString() < right.taskId.toString();
                  });
        for (const auto *track : fixture.model.tracks()) {
            result.objectAddresses.append(reinterpret_cast<quintptr>(track));
            for (const auto *clip : track->clips())
                result.objectAddresses.append(reinterpret_cast<quintptr>(clip));
        }
        result.loopSettings = fixture.host.loopSettings;
        result.idempotencyRecords = idempotencyRecords.get();
        result.replacementNotifications = fixture.host.replacementNotifications.size();
        return result;
    }

    bool sameDocument(const Automation::DocumentSnapshotDto &left,
                      const Automation::DocumentSnapshotDto &right) {
        return left.document == right.document && left.path == right.path &&
               left.projectName == right.projectName && left.lifecycle == right.lifecycle &&
               left.busy == right.busy && left.saved == right.saved;
    }

    bool sameHistory(const Automation::HistoryStateDto &left,
                     const Automation::HistoryStateDto &right) {
        return left.document == right.document && left.canUndo == right.canUndo &&
               left.canRedo == right.canRedo && left.onSavePoint == right.onSavePoint &&
               left.undoName == right.undoName && left.redoName == right.redoName;
    }

    bool sameState(const StateDigest &left, const StateDigest &right) {
        return sameDocument(left.document, right.document) &&
               sameHistory(left.history, right.history) && left.project == right.project &&
               left.timeline == right.timeline && left.serializedModel == right.serializedModel &&
               left.tasks == right.tasks && left.objectAddresses == right.objectAddresses &&
               left.loopSettings == right.loopSettings &&
               left.idempotencyRecords == right.idempotencyRecords &&
               left.replacementNotifications == right.replacementNotifications;
    }

    enum class PreparationOutcome {
        Ready,
        Failed,
        Canceled,
    };

    class ControlledPreparationGate final {
    public:
        bool deliver(const PreparationOutcome outcome, const std::function<void()> &commit) {
            ++completionCount;
            if (outcome != PreparationOutcome::Ready)
                return false;
            ++commitCount;
            commit();
            return true;
        }

        int completionCount = 0;
        int commitCount = 0;
    };

    void testInitialUntitledSession(TestRun &test) {
        constexpr auto scenario = "AFC-DOC-LIFECYCLE-001";
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();
        const auto version = runtime.documentVersion();
        const auto document = runtime.documents().getDocument(version.documentId);

        test.expect(!version.documentId.isNull(), scenario,
                    "the initial untitled session must have a non-null document ID");
        test.expect(version.revision == 0, scenario,
                    "the initial untitled session must start at revision zero");
        test.expect(document && document.get().document == version &&
                        document.get().path.isEmpty() && document.get().projectName.isEmpty() &&
                        document.get().lifecycle == Automation::DocumentLifecycleState::Active &&
                        !document.get().busy && document.get().saved,
                    scenario,
                    "the initial document snapshot must be a clean active untitled session");
    }

    void testNewOpenAndImport(TestRun &test) {
        LifecycleFixture fixture;
        auto &runtime = fixture.runtime;

        const auto initial = runtime.documentVersion();
        const LoopSettings newLoop(true, 120, 960);
        const auto newDocument = makeDocumentDraft(QStringLiteral("New Lifecycle Track"),
                                                   QStringLiteral("new-lifecycle"), newLoop);
        const auto committedNew =
            runtime.documents().commitNewDocument(commandContext(runtime), newDocument);
        const auto afterNew = runtime.documentVersion();
        const auto newSnapshot = runtime.documents().getDocument(afterNew.documentId);
        const auto newHistory = runtime.history().getState(afterNew.documentId);

        test.expect(committedNew && committedNew.get().changed &&
                        committedNew.get().previous == initial &&
                        committedNew.get().current == afterNew && !afterNew.documentId.isNull() &&
                        afterNew.documentId != initial.documentId && afterNew.revision == 0 &&
                        !committedNew.get().createdObjects.isEmpty(),
                    "AFC-DOC-LIFECYCLE-002",
                    "commit-new must atomically rotate identity, reset revision, and bind objects");
        test.expect(newSnapshot && newSnapshot.get().path.isEmpty() &&
                        newSnapshot.get().projectName.isEmpty() && newSnapshot.get().saved &&
                        newHistory && !newHistory.get().canUndo && !newHistory.get().canRedo &&
                        newHistory.get().onSavePoint && fixture.host.loopSettings == newLoop,
                    "AFC-DOC-LIFECYCLE-002",
                    "commit-new must establish a clean untitled baseline and apply loop state");

        auto emptyImport =
            makeDocumentDraft(QStringLiteral("unused"), QStringLiteral("empty-import"));
        emptyImport.tracks.clear();
        const auto beforeEmptyImport = runtime.documentVersion();
        const auto noOpImport = runtime.documents().commitImportedDocument(
            commandContext(runtime), emptyImport, false, false);
        test.expect(noOpImport && !noOpImport.get().changed &&
                        runtime.documentVersion() == beforeEmptyImport,
                    "AFC-DOC-LIFECYCLE-003",
                    "an empty import must be a no-op without a revision or History entry");

        const auto importedDocument = makeDocumentDraft(QStringLiteral("Imported Lifecycle Track"),
                                                        QStringLiteral("import-lifecycle"));
        const auto beforeImport = runtime.documentVersion();
        const auto committedImport = runtime.documents().commitImportedDocument(
            commandContext(runtime), importedDocument, false, false);
        const auto afterImport = runtime.documentVersion();
        const auto importHistory = runtime.history().getState(afterImport.documentId);
        const auto importedProject = runtime.project().getProject(afterImport.documentId);
        test.expect(committedImport && committedImport.get().changed &&
                        afterImport.documentId == beforeImport.documentId &&
                        afterImport.revision == beforeImport.revision + 1 && importHistory &&
                        importHistory.get().canUndo && !importHistory.get().onSavePoint &&
                        importedProject && importedProject.get().tracks.size() == 2,
                    "AFC-DOC-LIFECYCLE-003",
                    "commit-import must retain identity and create one History/revision change");

        const LoopSettings openedLoop(true, 480, 1440);
        auto openedDocument = makeDocumentDraft(QStringLiteral("Opened Lifecycle Track"),
                                                QStringLiteral("open-lifecycle"), openedLoop);
        auto &legacyProperties = openedDocument.tracks.first().clips.first().properties;
        legacyProperties.length = 960;
        legacyProperties.clipStart = 240;
        legacyProperties.clipLen = 1440;
        const QString openedPath = QStringLiteral("fixtures/opened-lifecycle.dspx");
        const auto committedOpen = runtime.documents().commitOpenedDocument(
            commandContext(runtime), openedDocument, openedPath,
            QStringLiteral("opened-lifecycle.dspx"), true);
        const auto afterOpen = runtime.documentVersion();
        const auto openSnapshot = runtime.documents().getDocument(afterOpen.documentId);
        const auto openHistory = runtime.history().getState(afterOpen.documentId);
        const auto openedProject = runtime.project().getProject(afterOpen.documentId);
        const auto staleSnapshot = runtime.documents().getDocument(afterImport.documentId);

        test.expect(committedOpen && committedOpen.get().changed &&
                        committedOpen.get().previous == afterImport &&
                        committedOpen.get().current == afterOpen &&
                        afterOpen.documentId != afterImport.documentId && afterOpen.revision == 0,
                    "AFC-DOC-LIFECYCLE-004",
                    "commit-open must rotate the document generation and reset revision");
        test.expect(
            openSnapshot && openSnapshot.get().path == openedPath &&
                openSnapshot.get().projectName == QStringLiteral("opened-lifecycle.dspx") &&
                openSnapshot.get().saved && openHistory && !openHistory.get().canUndo &&
                !openHistory.get().canRedo && openHistory.get().onSavePoint && openedProject &&
                openedProject.get().tracks.size() == 1 &&
                openedProject.get().tracks.first().clips.size() == 1 &&
                openedProject.get().tracks.first().clips.first().data.properties.length ==
                    legacyProperties.length &&
                openedProject.get().tracks.first().clips.first().data.properties.clipStart ==
                    legacyProperties.clipStart &&
                openedProject.get().tracks.first().clips.first().data.properties.clipLen ==
                    legacyProperties.clipLen &&
                fixture.host.loopSettings == openedLoop && !staleSnapshot &&
                staleSnapshot.getError().code == Automation::AutomationErrorCode::DocumentChanged,
            "AFC-DOC-LIFECYCLE-004",
            "commit-open must preserve legacy clip geometry, publish its savepoint, and "
            "reject the old generation");
    }

    void testFailureAndCancellationRollback(TestRun &test) {
        constexpr auto scenario = "AFC-DOC-LIFECYCLE-005";
        LifecycleFixture fixture;
        auto &runtime = fixture.runtime;

        const LoopSettings baselineLoop(true, 240, 1200);
        const auto baselineDocument = makeDocumentDraft(
            QStringLiteral("Rollback Baseline"), QStringLiteral("rollback-baseline"), baselineLoop);
        const auto committedBaseline =
            runtime.documents().commitNewDocument(commandContext(runtime), baselineDocument);
        test.expect(bool(committedBaseline), scenario,
                    "the rollback fixture must establish its baseline generation");

        const auto savedBaseline = runtime.documents().saveDocument(
            commandContext(runtime), QStringLiteral("fixtures/rollback-baseline.dspx"));
        const auto imported = runtime.documents().commitImportedDocument(
            commandContext(runtime),
            makeDocumentDraft(QStringLiteral("Rollback Import"), QStringLiteral("rollback-import")),
            false, false);
        Automation::TrackDraftDto idempotentTrack;
        idempotentTrack.clientRef = QStringLiteral("rollback-idempotent-track");
        idempotentTrack.name = QStringLiteral("Rollback Idempotent Track");
        idempotentTrack.gain = 1.0;
        const auto insertedTrack = runtime.project().insertTrack(
            commandContext(runtime, QStringLiteral("rollback-track-key")), 0, idempotentTrack);
        int taskCancelCount = 0;
        const auto task = runtime.automationTasks().createTask(
            Automation::OperationIds::extract::pitch::start, runtime.documentVersion(),
            std::nullopt, [&taskCancelCount] { ++taskCancelCount; });
        runtime.automationTasks().markRunning(task.taskId);
        const auto beforeFailure = captureState(fixture);
        test.expect(savedBaseline && imported && insertedTrack && beforeFailure.has_value(),
                    scenario,
                    "the rollback fixture must contain path, History, idempotency, and task state");

        auto invalidDocument = makeDocumentDraft(QStringLiteral("Invalid Replacement"),
                                                 QStringLiteral("invalid-replacement"));
        invalidDocument.tracks.first().colorIndex = -1;
        const auto invalidReplacement = runtime.documents().commitOpenedDocument(
            commandContext(runtime), invalidDocument, QStringLiteral("fixtures/invalid.dspx"),
            QStringLiteral("invalid.dspx"), true);
        const auto afterValidationFailure = captureState(fixture);
        test.expect(!invalidReplacement &&
                        invalidReplacement.getError().code ==
                            Automation::AutomationErrorCode::InvalidArgument &&
                        invalidReplacement.getError().operationId ==
                            Automation::OperationIds::documents::commit_open &&
                        beforeFailure && afterValidationFailure &&
                        sameState(*beforeFailure, *afterValidationFailure),
                    scenario,
                    "replacement validation failure must preserve the complete active generation");

        auto unsupportedContext = commandContext(runtime, QStringLiteral("replace-key"));
        const auto unsupportedReplacement = runtime.documents().commitNewDocument(
            unsupportedContext, makeDocumentDraft(QStringLiteral("Rejected Replacement"),
                                                  QStringLiteral("rejected-replacement")));
        const auto afterUnsupportedRequest = captureState(fixture);
        test.expect(!unsupportedReplacement &&
                        unsupportedReplacement.getError().code ==
                            Automation::AutomationErrorCode::InvalidArgument &&
                        beforeFailure && afterUnsupportedRequest &&
                        sameState(*beforeFailure, *afterUnsupportedRequest),
                    scenario,
                    "a rejected replacement request must not disturb any generation-owned state");

        ControlledPreparationGate gate;
        int forwardedCommits = 0;
        const auto wouldCommit = [&forwardedCommits] { ++forwardedCommits; };
        const bool deliveredFailure = gate.deliver(PreparationOutcome::Failed, wouldCommit);
        const auto afterPreparationFailure = captureState(fixture);
        const bool deliveredCancellation = gate.deliver(PreparationOutcome::Canceled, wouldCommit);
        const auto afterCancellation = captureState(fixture);
        test.expect(!deliveredFailure && !deliveredCancellation && gate.completionCount == 2 &&
                        gate.commitCount == 0 && forwardedCommits == 0 && beforeFailure &&
                        afterPreparationFailure && afterCancellation &&
                        sameState(*beforeFailure, *afterPreparationFailure) &&
                        sameState(*beforeFailure, *afterCancellation),
                    scenario,
                    "host parse failure and user cancellation must stop before the commit point");

        const auto preparedAgainst = runtime.documentVersion();
        const auto interveningEdit =
            runtime.timeline().setTempo(commandContext(runtime), 960, 132.0);
        const auto beforeRevisionFailure = captureState(fixture);
        Automation::CommandContext staleContext{
            .expected = preparedAgainst,
            .source = Automation::InvocationSource::Test,
        };
        const auto staleCommit = runtime.documents().commitOpenedDocument(
            staleContext,
            makeDocumentDraft(QStringLiteral("Stale Prepared Project"),
                              QStringLiteral("stale-prepared")),
            QStringLiteral("fixtures/stale.dspx"), QStringLiteral("stale.dspx"), true);
        const auto afterRevisionFailure = captureState(fixture);
        test.expect(interveningEdit && !staleCommit &&
                        staleCommit.getError().code ==
                            Automation::AutomationErrorCode::RevisionConflict &&
                        beforeRevisionFailure && afterRevisionFailure &&
                        sameState(*beforeRevisionFailure, *afterRevisionFailure),
                    "AFC-DOC-LIFECYCLE-006",
                    "a prepared replacement with a stale base revision must roll back completely");

        test.expect(taskCancelCount == 0, "AFC-DOC-LIFECYCLE-006",
                    "failed/canceled replacement attempts must preserve the active task state");
    }

    void testSaveAndSaveAs(TestRun &test) {
        constexpr auto scenario = "AFC-DOC-LIFECYCLE-007";
        LifecycleFixture fixture;
        auto &runtime = fixture.runtime;

        const auto committedNew = runtime.documents().commitNewDocument(
            commandContext(runtime),
            makeDocumentDraft(QStringLiteral("Save Baseline"), QStringLiteral("save-baseline")));
        const auto imported = runtime.documents().commitImportedDocument(
            commandContext(runtime),
            makeDocumentDraft(QStringLiteral("Dirty Before Save As"),
                              QStringLiteral("dirty-before-save-as")),
            false, false);
        const auto dirtyVersion = runtime.documentVersion();
        const auto dirtySnapshot = runtime.documents().getDocument(dirtyVersion.documentId);
        const QString firstPath = QStringLiteral("fixtures/first-save-as.dspx");
        const auto saveAs = runtime.documents().saveDocument(commandContext(runtime), firstPath);
        const auto afterSaveAs = runtime.documents().getDocument(dirtyVersion.documentId);

        test.expect(committedNew && imported && dirtySnapshot && !dirtySnapshot.get().saved &&
                        saveAs && runtime.documentVersion() == dirtyVersion &&
                        fixture.host.saveCalls == 1 && afterSaveAs &&
                        afterSaveAs.get().path == firstPath &&
                        afterSaveAs.get().projectName == QStringLiteral("first-save-as.dspx") &&
                        afterSaveAs.get().saved,
                    scenario, "save-as must preserve identity/revision and set path/savepoint");

        const auto edited = runtime.timeline().setTempo(commandContext(runtime), 960, 128.0);
        const auto versionBeforeSave = runtime.documentVersion();
        const auto save = runtime.documents().saveDocument(commandContext(runtime), firstPath);
        const auto afterSave = runtime.documents().getDocument(versionBeforeSave.documentId);
        test.expect(edited && save && runtime.documentVersion() == versionBeforeSave &&
                        fixture.host.saveCalls == 2 && afterSave &&
                        afterSave.get().path == firstPath && afterSave.get().saved,
                    scenario,
                    "saving an existing path must preserve identity/revision and advance "
                    "savepoint only");

        const auto editedAgain = runtime.timeline().setTempo(commandContext(runtime), 1920, 136.0);
        const auto beforeFailedSave = captureState(fixture);
        const auto failedVersion = runtime.documentVersion();
        fixture.host.saveSucceeds = false;
        const QString failedPath = QStringLiteral("fixtures/failed-save-as.dspx");
        const auto failedSave =
            runtime.documents().saveDocument(commandContext(runtime), failedPath);
        const auto afterFailedSave = captureState(fixture);
        test.expect(
            editedAgain && !failedSave &&
                failedSave.getError().code == Automation::AutomationErrorCode::IoError &&
                failedSave.getError().operationId == Automation::OperationIds::documents::save &&
                beforeFailedSave && afterFailedSave &&
                sameState(*beforeFailedSave, *afterFailedSave),
            scenario, "save I/O failure must leave document, path, and savepoint unchanged");

        fixture.host.saveSucceeds = true;
        const auto retriedSave =
            runtime.documents().saveDocument(commandContext(runtime), failedPath);
        const auto afterRetry = runtime.documents().getDocument(failedVersion.documentId);
        test.expect(retriedSave && runtime.documentVersion() == failedVersion &&
                        fixture.host.saveCalls == 4 && afterRetry &&
                        afterRetry.get().path == failedPath &&
                        afterRetry.get().projectName == QStringLiteral("failed-save-as.dspx") &&
                        afterRetry.get().saved,
                    scenario, "a failed save must be safely retryable");
    }

    void testGenerationCleanup(TestRun &test) {
        constexpr auto scenario = "AFC-DOC-LIFECYCLE-008";
        LifecycleFixture fixture;
        auto &runtime = fixture.runtime;

        const auto committedNew = runtime.documents().commitNewDocument(
            commandContext(runtime), makeDocumentDraft(QStringLiteral("Cleanup Baseline"),
                                                       QStringLiteral("cleanup-baseline")));
        const auto firstImportDraft = makeDocumentDraft(QStringLiteral("Cleanup Import One"),
                                                        QStringLiteral("cleanup-import-one"));
        const auto firstImport = runtime.documents().commitImportedDocument(
            commandContext(runtime), firstImportDraft, false, false);
        Automation::TrackDraftDto idempotentTrack;
        idempotentTrack.clientRef = QStringLiteral("generation-track");
        idempotentTrack.name = QStringLiteral("Generation Track");
        idempotentTrack.gain = 1.0;
        const auto firstInsert = runtime.project().insertTrack(
            commandContext(runtime, QStringLiteral("generation-key")), 0, idempotentTrack);
        const auto secondImport = runtime.documents().commitImportedDocument(
            commandContext(runtime),
            makeDocumentDraft(QStringLiteral("Cleanup Import Two"),
                              QStringLiteral("cleanup-import-two")),
            false, false);
        const auto undo = runtime.history().undo(commandContext(runtime));
        const auto historyBefore = runtime.history().getState(runtime.documentVersion().documentId);

        int runningTaskCancelCount = 0;
        const auto runningTask = runtime.automationTasks().createTask(
            Automation::OperationIds::extract::pitch::start, runtime.documentVersion(),
            std::nullopt, [&runningTaskCancelCount] { ++runningTaskCancelCount; });
        runtime.automationTasks().markRunning(runningTask.taskId);
        const auto terminalTask = runtime.automationTasks().createTask(
            Automation::OperationIds::exports::audio::start, runtime.documentVersion());
        runtime.automationTasks().markRunning(terminalTask.taskId);
        Automation::MutationResult terminalMutation;
        terminalMutation.previous = runtime.documentVersion();
        terminalMutation.current = runtime.documentVersion();
        runtime.automationTasks().succeed(terminalTask.taskId, terminalMutation);

        const auto oldVersion = runtime.documentVersion();
        const auto notificationCount = fixture.host.replacementNotifications.size();
        const auto replacement = runtime.documents().commitOpenedDocument(
            commandContext(runtime),
            makeDocumentDraft(QStringLiteral("Cleanup Replacement"),
                              QStringLiteral("cleanup-replacement")),
            QStringLiteral("fixtures/cleanup-replacement.dspx"),
            QStringLiteral("cleanup-replacement.dspx"), true);
        const auto newVersion = runtime.documentVersion();
        const auto historyAfter = runtime.history().getState(newVersion.documentId);
        const auto idempotencyAfter = runtime.dispatcher().dispatchDocumentQuery<qsizetype>(
            Automation::OperationIds::documents::get, newVersion.documentId,
            [](Automation::DocumentSession &session) {
                return Automation::AutomationResult<qsizetype>(session.idempotencyStore().size());
            });
        const auto oldDocument = runtime.documents().getDocument(oldVersion.documentId);
        const auto oldTaskFromOldGeneration =
            runtime.tasks().getTask(oldVersion.documentId, runningTask.taskId);
        const auto oldTaskFromNewGeneration =
            runtime.tasks().getTask(newVersion.documentId, runningTask.taskId);

        test.expect(committedNew && firstImport && firstInsert && secondImport && undo &&
                        historyBefore && historyBefore.get().canUndo && historyBefore.get().canRedo,
                    scenario,
                    "the cleanup fixture must contain both History branches before replacement");
        test.expect(replacement && newVersion.documentId != oldVersion.documentId &&
                        newVersion.revision == 0 && historyAfter && !historyAfter.get().canUndo &&
                        !historyAfter.get().canRedo && historyAfter.get().onSavePoint &&
                        idempotencyAfter && idempotencyAfter.get() == 0 &&
                        runtime.automationTasks().size() == 0 && runningTaskCancelCount == 1 &&
                        fixture.host.replacementNotifications.size() == notificationCount + 1 &&
                        fixture.host.replacementNotifications.last() == oldVersion.documentId,
                    scenario,
                    "successful replacement must clear History, idempotency, and task generation "
                    "state");
        test.expect(!oldDocument &&
                        oldDocument.getError().code ==
                            Automation::AutomationErrorCode::DocumentChanged &&
                        !oldTaskFromOldGeneration &&
                        oldTaskFromOldGeneration.getError().code ==
                            Automation::AutomationErrorCode::DocumentChanged &&
                        !oldTaskFromNewGeneration &&
                        oldTaskFromNewGeneration.getError().code ==
                            Automation::AutomationErrorCode::NotFound,
                    scenario,
                    "old document/task identities must not be observable from the new generation");

        const auto reusedKey = runtime.project().insertTrack(
            commandContext(runtime, QStringLiteral("generation-key")), 0, idempotentTrack);
        test.expect(reusedKey && reusedKey.get().changed &&
                        runtime.documentVersion().documentId == newVersion.documentId &&
                        runtime.documentVersion().revision == 1,
                    scenario,
                    "an idempotency key from the old generation must be reusable after "
                    "replacement");
    }

    struct GenerationObjectIndex {
        Automation::DocumentId oldDocument;
        Automation::DocumentId currentDocument;
        int collidingObjectId = -1;

        [[nodiscard]] bool contains(const Automation::DocumentId &documentId,
                                    const int objectId) const {
            return objectId == collidingObjectId &&
                   (documentId == oldDocument || documentId == currentDocument);
        }
    };

    void testOldIdCollisionAndErrorPriority(TestRun &test) {
        constexpr auto collisionScenario = "AFC-DOC-LIFECYCLE-009";
        Automation::DocumentSession currentSession(nullptr, nullptr);
        Automation::SingleDocumentSessionResolver resolver(currentSession);
        Automation::SingleWindowContext window;
        Automation::AutomationDispatcher dispatcher(resolver, window);
        const auto oldDocument = Automation::DocumentId::create();
        const GenerationObjectIndex index{oldDocument, currentSession.documentId(), 37};
        Automation::CommandContext oldContext{
            .expected = {oldDocument, currentSession.revision()},
            .source = Automation::InvocationSource::Test,
        };
        bool objectLookupAttempted = false;
        const auto collision = dispatcher.dispatchDocumentCommand(
            Automation::OperationIds::tracks::set_color, oldContext,
            [&index, &objectLookupAttempted](Automation::DocumentSession &session, const bool) {
                objectLookupAttempted = true;
                if (!index.contains(session.documentId(), index.collidingObjectId)) {
                    return Automation::AutomationResult<Automation::MutationResult>(
                        Automation::AutomationError::notFound(
                            {Automation::ObjectKind::Track, index.collidingObjectId},
                            QStringLiteral("controlled object is missing")));
                }
                Automation::MutationResult result;
                result.previous = session.version();
                result.current = session.version();
                return Automation::AutomationResult<Automation::MutationResult>(result);
            });
        test.expect(
            index.contains(oldDocument, 37) && index.contains(currentSession.documentId(), 37) &&
                !collision &&
                collision.getError().code == Automation::AutomationErrorCode::DocumentChanged &&
                collision.getError().operationId == Automation::OperationIds::tracks::set_color &&
                !objectLookupAttempted,
            collisionScenario,
            "DocumentId must reject a stale generation before a colliding integer "
            "object ID");

        constexpr auto priorityScenario = "AFC-DOC-LIFECYCLE-010";
        LifecycleFixture fixture;
        auto &runtime = fixture.runtime;
        const auto committedNew = runtime.documents().commitNewDocument(
            commandContext(runtime), makeDocumentDraft(QStringLiteral("Priority Old Track"),
                                                       QStringLiteral("priority-old")));
        const auto oldVersion = runtime.documentVersion();
        const auto replacement = runtime.documents().commitOpenedDocument(
            commandContext(runtime),
            makeDocumentDraft(QStringLiteral("Priority Current Track"),
                              QStringLiteral("priority-current")),
            QStringLiteral("fixtures/priority-current.dspx"),
            QStringLiteral("priority-current.dspx"), true);
        const auto currentVersion = runtime.documentVersion();
        const auto currentProject = runtime.project().getProject(currentVersion.documentId);
        test.expect(committedNew && replacement && currentProject &&
                        !currentProject.get().tracks.isEmpty(),
                    priorityScenario, "the error-priority fixture must have a current track");
        if (!currentProject || currentProject.get().tracks.isEmpty())
            return;

        const auto currentTrackId = currentProject.get().tracks.first().id;
        Automation::CommandContext oldGenerationContext{
            .expected = oldVersion,
            .source = Automation::InvocationSource::Test,
        };
        const auto documentError =
            runtime.project().setTrackColor(oldGenerationContext, currentTrackId, -1);

        Automation::CommandContext staleRevisionContext{
            .expected = {currentVersion.documentId, currentVersion.revision + 1},
            .source = Automation::InvocationSource::Test,
        };
        const auto unknownTrackId = Automation::TrackId(std::numeric_limits<int>::max());
        const auto revisionError =
            runtime.project().setTrackColor(staleRevisionContext, unknownTrackId, -1);
        const auto objectError =
            runtime.project().setTrackColor(commandContext(runtime), unknownTrackId, -1);
        const auto domainError =
            runtime.project().setTrackColor(commandContext(runtime), currentTrackId, -1);

        test.expect(
            !documentError &&
                documentError.getError().code == Automation::AutomationErrorCode::DocumentChanged &&
                !revisionError &&
                revisionError.getError().code ==
                    Automation::AutomationErrorCode::RevisionConflict &&
                !objectError &&
                objectError.getError().code == Automation::AutomationErrorCode::NotFound &&
                !domainError &&
                domainError.getError().code == Automation::AutomationErrorCode::InvalidArgument,
            priorityScenario,
            "errors must be ordered as document, revision, object, then domain validation");
        test.expect(
            documentError.getError().operationId == Automation::OperationIds::tracks::set_color &&
                revisionError.getError().operationId ==
                    Automation::OperationIds::tracks::set_color &&
                objectError.getError().operationId == Automation::OperationIds::tracks::set_color &&
                domainError.getError().operationId == Automation::OperationIds::tracks::set_color,
            priorityScenario, "every prioritized error must retain the centralized operation ID");
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    TestRun test;

    testInitialUntitledSession(test);
    testNewOpenAndImport(test);
    testFailureAndCancellationRollback(test);
    testSaveAndSaveAs(test);
    testGenerationCleanup(test);
    testOldIdCollisionAndErrorPriority(test);

    QTextStream(stdout) << "TestAutomationDocumentLifecycle: " << test.assertions() << " assertions"
                        << Qt::endl;
    return test.ok() ? 0 : 1;
}
