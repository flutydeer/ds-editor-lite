#include "Automation/AutomationDispatcher.h"
#include "Automation/AudioExportAutomationFacade.h"
#include "Automation/CoreRuntime.h"
#include "Automation/OperationIds.h"
#include "Automation/ProjectAutomationDtos.h"
#include "TestRuntime.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QMetaObject>
#include <QTemporaryDir>
#include <QTextStream>

#include <atomic>
#include <barrier>
#include <functional>
#include <memory>
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

    Automation::CommandContext commandContext(const Automation::CoreRuntime &runtime,
                                              const QString &idempotencyKey = {},
                                              const bool validateOnly = false) {
        return {
            .expected = runtime.documentVersion(),
            .validateOnly = validateOnly,
            .idempotencyKey = idempotencyKey,
            .source = Automation::InvocationSource::Test,
        };
    }

    Automation::CommandContext commandContext(const Automation::DocumentSession &session,
                                              const QString &idempotencyKey = {},
                                              const bool validateOnly = false) {
        return {
            .expected = session.version(),
            .validateOnly = validateOnly,
            .idempotencyKey = idempotencyKey,
            .source = Automation::InvocationSource::Test,
        };
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

    bool serialReplayExecutesOnce() {
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();
        const auto context =
            commandContext(runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000001"));
        int executionCount = 0;
        const auto handler = countedHandler(executionCount);

        const auto first = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, context, QByteArrayLiteral("serial"),
            handler);
        const auto replay = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, context, QByteArrayLiteral("serial"),
            handler);

        return expect(first && replay && first.get() == replay.get(),
                      QStringLiteral("serial replay must return the first result")) &&
               expect(executionCount == 1 && runtime.documentVersion().revision == 1,
                      QStringLiteral("serial replay must execute and advance revision once"));
    }

    bool concurrentReplayExecutesOnce(const int laneCount) {
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();
        const auto context = commandContext(
            runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000002-%1").arg(laneCount));
        std::atomic<int> executionCount = 0;
        Automation::AutomationDispatcher::DocumentCommandHandler handler =
            [&executionCount](Automation::DocumentSession &session, const bool validateOnly) {
                executionCount.fetch_add(1, std::memory_order_relaxed);
                return MutationResult(successfulMutation(session, validateOnly));
            };

        std::vector<std::optional<MutationResult>> results(static_cast<size_t>(laneCount));
        std::barrier<> startGate(laneCount + 1);
        std::atomic<int> completed = 0;
        QEventLoop eventLoop;
        std::vector<std::thread> workers;
        workers.reserve(static_cast<size_t>(laneCount));
        for (int index = 0; index < laneCount; ++index) {
            workers.emplace_back([&, index] {
                startGate.arrive_and_wait();
                results[static_cast<size_t>(index)].emplace(
                    runtime.dispatcher().dispatchDocumentCommand(
                        Automation::OperationIds::tracks::insert, context,
                        QByteArrayLiteral("concurrent"), handler));
                if (completed.fetch_add(1, std::memory_order_acq_rel) + 1 == laneCount) {
                    QMetaObject::invokeMethod(&eventLoop, "quit", Qt::QueuedConnection);
                }
            });
        }

        startGate.arrive_and_wait();
        eventLoop.exec();
        for (auto &worker : workers)
            worker.join();

        bool allReplayed = !results.empty() && results.front() && *results.front();
        if (allReplayed) {
            const auto expected = results.front()->get();
            for (const auto &result : results) {
                allReplayed &= result && *result && result->get() == expected;
            }
        }
        return expect(allReplayed,
                      QStringLiteral("all %1 concurrent callers must receive one result")
                          .arg(laneCount)) &&
               expect(executionCount.load(std::memory_order_relaxed) == 1 &&
                          runtime.documentVersion().revision == 1,
                      QStringLiteral("%1 concurrent callers must execute once").arg(laneCount));
    }

    bool successfulKeyConflictsAreStable() {
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();
        auto original =
            commandContext(runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000003"));
        int executionCount = 0;
        const auto handler = countedHandler(executionCount);

        const auto first = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, original, QByteArrayLiteral("alpha"),
            handler);
        const auto changedParameters = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, original, QByteArrayLiteral("beta"), handler);
        auto changedExpectedRevision = original;
        changedExpectedRevision.expected = runtime.documentVersion();
        const auto changedExpected = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, changedExpectedRevision,
            QByteArrayLiteral("alpha"), handler);
        const auto changedOperation = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::move, original, QByteArrayLiteral("alpha"), handler);

        const auto isConflict = [](const MutationResult &result) {
            return !result &&
                   result.getError().code == Automation::AutomationErrorCode::IdempotencyConflict &&
                   result.getError().fieldPath == QStringLiteral("idempotency_key");
        };
        return expect(first && isConflict(changedParameters) && isConflict(changedExpected) &&
                          isConflict(changedOperation),
                      QStringLiteral("a successful key must conflict on parameters, original "
                                     "expected revision, and operation")) &&
               expect(executionCount == 1,
                      QStringLiteral("idempotency conflicts must not invoke the handler"));
    }

    bool audioClipStateChangesConflict() {
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();

        Automation::TrackDraftDto track;
        track.clientRef = QStringLiteral("idempotency-audio-track");
        track.name = QStringLiteral("Audio Track");
        track.gain = 1.0;
        const auto insertedTrack = runtime.project().insertTrack(commandContext(runtime), 0, track);
        if (!expect(insertedTrack && insertedTrack.get().affectedObjects.size() == 1,
                    QStringLiteral("audio idempotency fixture track must be inserted"))) {
            return false;
        }
        const Automation::TrackId trackId(insertedTrack.get().affectedObjects.first().value);

        Automation::ClipDraftDto audio;
        audio.clientRef = QStringLiteral("idempotency-audio-clip");
        audio.type = Automation::ClipDraftDto::Type::Audio;
        audio.properties.name = QStringLiteral("audio.wav");
        audio.properties.length = 480;
        audio.properties.clipLen = 480;
        audio.properties.gain = 1.0;
        audio.properties.trimStartMs = 0.0;
        audio.properties.playLengthMs = 500.0;
        audio.properties.materialLengthMs = 1000.0;
        audio.audioPath = QStringLiteral("audio.wav");
        const QList<Automation::ClipInsertDto> request{
            {.trackId = trackId, .clip = audio}
        };
        const auto context =
            commandContext(runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000017"));
        const auto first = runtime.project().insertClips(context, request);
        if (!expect(first && first.get().changed,
                    QStringLiteral("the first audio clip request must commit"))) {
            return false;
        }

        QList<QList<Automation::ClipInsertDto>> variants;
        auto statusVariant = request;
        statusVariant.first().clip.audioPathStatus = AudioClip::PathStatus::Missing;
        variants.append(statusVariant);
        auto anchorVariant = request;
        anchorVariant.first().clip.hasRealTimeAnchor = true;
        variants.append(anchorVariant);
        auto chunkVariant = request;
        chunkVariant.first().clip.audioInfo.chunkSize = 1024;
        variants.append(chunkVariant);
        auto mipmapScaleVariant = request;
        mipmapScaleVariant.first().clip.audioInfo.mipmapScale = 4;
        variants.append(mipmapScaleVariant);
        auto mipmapVariant = request;
        mipmapVariant.first().clip.audioInfo.peakCacheMipmap.append({-12, 34});
        variants.append(mipmapVariant);

        bool conflicts = true;
        for (const auto &variant : variants) {
            const auto result = runtime.project().insertClips(context, variant);
            conflicts &=
                !result &&
                result.getError().code == Automation::AutomationErrorCode::IdempotencyConflict &&
                result.getError().fieldPath == QStringLiteral("idempotency_key") &&
                result.getError().operationId == Automation::OperationIds::clips::insert;
        }
        return expect(conflicts,
                      QStringLiteral("every constructed audio state field must conflict under a "
                                     "reused idempotency key")) &&
               expect(runtime.documentVersion().revision == context.expected.revision + 1,
                      QStringLiteral("audio state conflicts must not commit another clip"));
    }

    bool curveCollectionsChangeConflict() {
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();

        Automation::TrackDraftDto track;
        track.clientRef = QStringLiteral("idempotency-curve-track");
        track.name = QStringLiteral("Curve Track");
        track.gain = 1.0;
        const auto insertedTrack = runtime.project().insertTrack(commandContext(runtime), 0, track);
        if (!expect(insertedTrack && insertedTrack.get().affectedObjects.size() == 1,
                    QStringLiteral("curve idempotency fixture track must be inserted"))) {
            return false;
        }
        const Automation::TrackId trackId(insertedTrack.get().affectedObjects.first().value);

        Automation::CurveDraftDto curve;
        curve.type = Automation::CurveDraftDto::Type::Draw;
        curve.step = 5;
        curve.values = {1, 2, 0};
        Automation::ParamCurvesDraftDto parameter;
        parameter.name = ParamInfo::Pitch;
        parameter.type = Param::Edited;
        parameter.curves = {curve};
        Automation::ClipDraftDto singing;
        singing.clientRef = QStringLiteral("idempotency-curve-clip");
        singing.type = Automation::ClipDraftDto::Type::Singing;
        singing.properties.name = QStringLiteral("Curve Clip");
        singing.properties.length = 480;
        singing.properties.clipLen = 480;
        singing.properties.gain = 1.0;
        singing.defaultLanguage = QStringLiteral("en");
        singing.params = {parameter};
        const QList<Automation::ClipInsertDto> request{
            {.trackId = trackId, .clip = singing}
        };
        const auto context =
            commandContext(runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000018"));
        const auto first = runtime.project().insertClips(context, request);
        if (!expect(first && first.get().changed,
                    QStringLiteral("the first curve request must commit"))) {
            return false;
        }

        auto redistributed = request;
        auto &redistributedCurve = redistributed.first().clip.params.first().curves.first();
        redistributedCurve.values.clear();
        redistributedCurve.nodes.append({
            .position = 1,
            .value = 2,
            .interpolation = static_cast<AnchorNode::InterpMode>(0),
        });
        const auto conflict = runtime.project().insertClips(context, redistributed);
        return expect(!conflict &&
                          conflict.getError().code ==
                              Automation::AutomationErrorCode::IdempotencyConflict &&
                          conflict.getError().fieldPath == QStringLiteral("idempotency_key") &&
                          conflict.getError().operationId ==
                              Automation::OperationIds::clips::insert,
                      QStringLiteral("curve value and node collection boundaries must produce "
                                     "distinct idempotency fingerprints")) &&
               expect(runtime.documentVersion().revision == context.expected.revision + 1,
                      QStringLiteral("curve collection conflicts must not commit another clip"));
    }

    bool previewsAndValidationFailuresDoNotClaimKeys() {
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();
        int previewExecutions = 0;
        auto previewContext =
            commandContext(runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000004"), true);
        const auto previewHandler = countedHandler(previewExecutions);
        const auto preview = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, previewContext,
            QByteArrayLiteral("preview-input"), previewHandler);

        auto commitContext = previewContext;
        commitContext.validateOnly = false;
        const auto committed = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, commitContext,
            QByteArrayLiteral("different-committed-input"), previewHandler);

        const auto validationContext =
            commandContext(runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000005"));
        int validationAttempts = 0;
        Automation::AutomationDispatcher::DocumentCommandHandler invalid =
            [&validationAttempts](Automation::DocumentSession &, bool) {
                ++validationAttempts;
                return MutationResult(Automation::AutomationError::invalidArgument(
                    QStringLiteral("request"), QStringLiteral("simulated validation failure")));
            };
        Automation::AutomationDispatcher::DocumentCommandHandler valid =
            [&validationAttempts](Automation::DocumentSession &session, const bool validateOnly) {
                ++validationAttempts;
                return MutationResult(successfulMutation(session, validateOnly));
            };
        const auto rejected = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, validationContext,
            QByteArrayLiteral("invalid-input"), invalid);
        const auto accepted = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, validationContext,
            QByteArrayLiteral("valid-input"), valid);

        return expect(preview && preview.get().validatedOnly && committed &&
                          !committed.get().validatedOnly && previewExecutions == 2,
                      QStringLiteral("validate-only must not claim its idempotency key")) &&
               expect(!rejected &&
                          rejected.getError().code ==
                              Automation::AutomationErrorCode::InvalidArgument &&
                          accepted && validationAttempts == 2,
                      QStringLiteral("validation failure must not claim its idempotency key"));
    }

    bool commitFailureDoesNotClaimKey() {
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();
        const auto context =
            commandContext(runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000006"));
        int attempts = 0;
        Automation::AutomationDispatcher::DocumentCommandHandler failing =
            [&attempts](Automation::DocumentSession &, bool) {
                ++attempts;
                Automation::AutomationError error;
                error.code = Automation::AutomationErrorCode::IoError;
                error.message = QStringLiteral("simulated commit failure");
                return MutationResult(std::move(error));
            };
        Automation::AutomationDispatcher::DocumentCommandHandler succeeding =
            [&attempts](Automation::DocumentSession &session, const bool validateOnly) {
                ++attempts;
                return MutationResult(successfulMutation(session, validateOnly));
            };

        const auto failed = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, context, QByteArrayLiteral("failed-commit"),
            failing);
        const auto retried = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, context,
            QByteArrayLiteral("retry-after-failure"), succeeding);

        return expect(!failed &&
                          failed.getError().code == Automation::AutomationErrorCode::IoError &&
                          retried && attempts == 2 && runtime.documentVersion().revision == 1,
                      QStringLiteral("commit failure must release the idempotency key"));
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

    Automation::OperationDescriptor documentCommandDescriptor(const Automation::OperationId &id) {
        return {
            .id = id,
            .category = QStringLiteral("tracks"),
            .kind = Automation::OperationKind::Command,
            .syncMode = Automation::SyncMode::Synchronous,
            .documentPolicy = Automation::DocumentPolicy::Write,
            .revisionPolicy = Automation::RevisionPolicy::Increment,
            .historyPolicy = Automation::HistoryPolicy::Record,
            .fileAccess = Automation::FileAccessPolicy::None,
            .hostAvailability = Automation::HostAvailability::Core,
            .safety = Automation::SafetyClass::Reversible,
            .exposure = Automation::ExposurePolicy::InternalOnly,
            .idempotency = Automation::IdempotencyPolicy::DocumentGeneration,
        };
    }

    bool documentsHaveIndependentKeySpaces() {
        Automation::DocumentSession first(nullptr, nullptr);
        Automation::DocumentSession second(nullptr, nullptr);
        TwoDocumentResolver resolver(first, second);
        Automation::SingleWindowContext window;
        Automation::OperationCatalog catalog;
        if (!expect(catalog.add(documentCommandDescriptor(Automation::OperationIds::tracks::insert))
                        .isPresent(),
                    QStringLiteral("document-isolation descriptor must register"))) {
            return false;
        }
        Automation::AutomationDispatcher dispatcher(resolver, window, catalog);
        const auto key = QStringLiteral("d0d00000-0000-4000-8000-000000000007");
        int executions = 0;
        const auto handler = countedHandler(executions);

        const auto firstResult = dispatcher.dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, commandContext(first, key),
            QByteArrayLiteral("shared-input"), handler);
        const auto secondResult = dispatcher.dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, commandContext(second, key),
            QByteArrayLiteral("shared-input"), handler);

        return expect(first.documentId() != second.documentId() && firstResult && secondResult &&
                          executions == 2 && first.revision() == 1 && second.revision() == 1,
                      QStringLiteral("the same key must execute once in each document"));
    }

    bool generationsHaveIndependentKeySpaces() {
        AutomationTestSupport::TestRuntime fixture;
        auto &runtime = fixture.runtime();
        const auto key = QStringLiteral("d0d00000-0000-4000-8000-000000000008");
        const auto originalContext = commandContext(runtime, key);
        int executions = 0;
        const auto handler = countedHandler(executions);
        const auto original = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, originalContext,
            QByteArrayLiteral("generation-input"), handler);
        const auto originalDocumentId = runtime.documentVersion().documentId;

        AppModel replacementModel;
        replacementModel.newProject();
        const auto replacementDraft =
            Automation::documentDraftDto(replacementModel.takeProjectData(), {});
        const auto replacement =
            runtime.documents().commitNewDocument(commandContext(runtime), replacementDraft);
        const auto replacementContext = commandContext(runtime, key);
        const auto reused = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, replacementContext,
            QByteArrayLiteral("generation-input"), handler);

        return expect(original && replacement && reused &&
                          runtime.documentVersion().documentId != originalDocumentId &&
                          executions == 2 && runtime.documentVersion().revision == 1,
                      QStringLiteral("a replaced generation must accept the old key as new"));
    }

    class ServiceRuntime final {
    public:
        ServiceRuntime(Automation::DocumentRuntimeServices documentServices = {},
                       Automation::AudioExportRuntimeServices audioExportServices = {})
            : m_history(resetHistory()) {
            m_runtime = std::make_unique<Automation::CoreRuntime>(
                &m_model, m_history, std::move(documentServices),
                Automation::PlaybackRuntimeServices{}, Automation::EditorRuntimeServices{},
                Automation::SettingsRuntimeServices{}, Automation::PresetRuntimeServices{},
                Automation::PackageRuntimeServices{}, Automation::InferenceRuntimeServices{},
                Automation::FileRuntimeServices{}, std::move(audioExportServices));
        }

        ~ServiceRuntime() {
            m_runtime.reset();
            m_history->reset();
        }

        Automation::CoreRuntime &runtime() const {
            return *m_runtime;
        }

    private:
        static HistoryManager *resetHistory() {
            auto *history = HistoryManager::instance();
            history->reset();
            return history;
        }

        AppModel m_model;
        HistoryManager *m_history;
        std::unique_ptr<Automation::CoreRuntime> m_runtime;
    };

    bool saveDoesNotClearCache() {
        int saveCount = 0;
        Automation::DocumentRuntimeServices services;
        services.saveProject = [&saveCount](const QString &, AppModel *, QString &) {
            ++saveCount;
            return true;
        };
        ServiceRuntime fixture(std::move(services));
        auto &runtime = fixture.runtime();
        QTemporaryDir directory;
        if (!expect(directory.isValid(), QStringLiteral("temporary save directory must exist")))
            return false;

        const auto cachedContext =
            commandContext(runtime, QStringLiteral("d0d00000-0000-4000-8000-000000000009"));
        int executionCount = 0;
        const auto handler = countedHandler(executionCount);
        const auto cached = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, cachedContext,
            QByteArrayLiteral("cache-before-save"), handler);
        const auto beforeSave = runtime.documentVersion();
        const auto saved = runtime.documents().saveDocument(
            commandContext(runtime), directory.filePath(QStringLiteral("project.dspx")));
        const auto replayed = runtime.dispatcher().dispatchDocumentCommand(
            Automation::OperationIds::tracks::insert, cachedContext,
            QByteArrayLiteral("cache-before-save"), handler);

        return expect(cached && saved && replayed && cached.get() == replayed.get() &&
                          runtime.documentVersion() == beforeSave && executionCount == 1 &&
                          saveCount == 1,
                      QStringLiteral("save must preserve prior generation cache entries"));
    }

    class ControlledScheduler final {
    public:
        void schedule(std::function<void()> execute) {
            m_pending.append(std::move(execute));
        }

        [[nodiscard]] qsizetype size() const {
            return m_pending.size();
        }

        bool runNext() {
            if (m_pending.isEmpty())
                return false;
            auto execute = m_pending.takeFirst();
            execute();
            return true;
        }

    private:
        QList<std::function<void()>> m_pending;
    };

    struct FakeAudioControl {
        int createCount = 0;
        int executeCount = 0;
        int cancelCount = 0;
        int cleanupCount = 0;
        int createFailuresRemaining = 0;
        Automation::AudioExportBackendState backendState =
            Automation::AudioExportBackendState::Succeeded;
    };

    class FakeAudioJob final : public Automation::IAudioExportJob {
    public:
        FakeAudioJob(Automation::AudioExportConfigDto config,
                     std::shared_ptr<FakeAudioControl> control)
            : m_config(std::move(config)), m_control(std::move(control)) {
        }

        Automation::AudioExportPreviewDto preview() const override {
            Automation::AudioExportPreviewDto result;
            result.baseDirectory = m_config.fileDirectory;
            result.filePaths.append(
                QDir(m_config.fileDirectory).absoluteFilePath(m_config.fileName));
            return result;
        }

        Automation::AudioExportBackendResult
            execute(const Automation::AudioExportObserver &) override {
            ++m_control->executeCount;
            return {
                .state = m_control->backendState,
                .errorMessage =
                    m_control->backendState == Automation::AudioExportBackendState::Failed
                        ? QStringLiteral("simulated runtime failure")
                        : QString(),
            };
        }

        void cancel() override {
            ++m_control->cancelCount;
        }

        void cleanup() override {
            ++m_control->cleanupCount;
        }

    private:
        Automation::AudioExportConfigDto m_config;
        std::shared_ptr<FakeAudioControl> m_control;
    };

    Automation::AutomationError simulatedSubmissionFailure() {
        Automation::AutomationError error;
        error.code = Automation::AutomationErrorCode::ModuleNotReady;
        error.message = QStringLiteral("simulated submission failure");
        return error;
    }

    Automation::AudioExportRuntimeServices
        audioServices(ControlledScheduler &scheduler,
                      const std::shared_ptr<FakeAudioControl> &control) {
        Automation::AudioExportRuntimeServices services;
        services.createJob = [control](AppModel *, const QString &,
                                       const Automation::AudioExportConfigDto &config) {
            ++control->createCount;
            if (control->createFailuresRemaining > 0) {
                --control->createFailuresRemaining;
                return Automation::AutomationResult<std::shared_ptr<Automation::IAudioExportJob>>(
                    simulatedSubmissionFailure());
            }
            std::shared_ptr<Automation::IAudioExportJob> job =
                std::make_shared<FakeAudioJob>(config, control);
            return Automation::AutomationResult<std::shared_ptr<Automation::IAudioExportJob>>(
                std::move(job));
        };
        services.schedule = [&scheduler](std::function<void()> execute) {
            scheduler.schedule(std::move(execute));
        };
        return services;
    }

    class AudioHarness final {
    public:
        AudioHarness()
            : m_control(std::make_shared<FakeAudioControl>()),
              m_runtime({}, audioServices(m_scheduler, m_control)) {
        }

        [[nodiscard]] bool isValid() const {
            return m_directory.isValid();
        }

        Automation::CoreRuntime &runtime() {
            return m_runtime.runtime();
        }

        ControlledScheduler &scheduler() {
            return m_scheduler;
        }

        FakeAudioControl &control() {
            return *m_control;
        }

        Automation::AudioExportConfigDto config(const QString &fileName) const {
            return {
                .fileName = fileName,
                .fileDirectory = m_directory.path(),
            };
        }

    private:
        QTemporaryDir m_directory;
        ControlledScheduler m_scheduler;
        std::shared_ptr<FakeAudioControl> m_control;
        ServiceRuntime m_runtime;
    };

    bool asyncAcceptedReplaySchedulesOnce() {
        AudioHarness harness;
        if (!expect(harness.isValid(), QStringLiteral("async-replay audio harness must be valid")))
            return false;
        const auto context = commandContext(harness.runtime(),
                                            QStringLiteral("d0d00000-0000-4000-8000-000000000010"));
        const auto config = harness.config(QStringLiteral("serial-replay.wav"));
        const auto first = harness.runtime().audioExports().start(context, config, {});
        const auto replay = harness.runtime().audioExports().start(context, config, {});
        const bool acceptedOnce =
            first && replay && first.get() == replay.get() && harness.scheduler().size() == 1 &&
            harness.runtime().automationTasks().size() == 1 && harness.control().createCount == 1;
        const auto ran = harness.scheduler().runNext();
        std::optional<Automation::AutomationResult<Automation::AutomationTaskSnapshot>> completed;
        if (first) {
            completed.emplace(harness.runtime().tasks().getTask(
                harness.runtime().documentVersion().documentId, first.get().taskId));
        }
        const auto terminalReplay = harness.runtime().audioExports().start(context, config, {});

        return expect(acceptedOnce,
                      QStringLiteral(
                          "accepted async replay must return one TaskId and schedule once")) &&
               expect(ran && completed && *completed &&
                          completed->get().state == Automation::AutomationTaskState::Succeeded &&
                          terminalReplay && terminalReplay.get() == first.get() &&
                          harness.scheduler().size() == 0 && harness.control().createCount == 2 &&
                          harness.control().executeCount == 1,
                      QStringLiteral("successful async replay must retain the original TaskId"));
    }

    bool asyncPreAcceptanceFailuresDoNotClaimKeys() {
        bool ok = true;
        {
            AudioHarness harness;
            ok &= expect(harness.isValid(),
                         QStringLiteral("validate-only audio harness must be valid"));
            auto previewContext = commandContext(
                harness.runtime(), QStringLiteral("d0d00000-0000-4000-8000-000000000011"), true);
            const auto preview = harness.runtime().audioExports().start(
                previewContext, harness.config(QStringLiteral("preview.wav")), {});
            auto acceptedContext = previewContext;
            acceptedContext.validateOnly = false;
            const auto accepted = harness.runtime().audioExports().start(
                acceptedContext, harness.config(QStringLiteral("accepted.wav")), {});
            ok &= expect(
                preview && preview.get().validatedOnly && preview.get().taskId.isNull() &&
                    accepted && !accepted.get().taskId.isNull() &&
                    harness.scheduler().size() == 1 &&
                    harness.runtime().automationTasks().size() == 1,
                QStringLiteral("async validate-only must not claim a key or allocate a task"));
        }
        {
            AudioHarness harness;
            ok &= expect(harness.isValid(),
                         QStringLiteral("validation-failure audio harness must be valid"));
            const auto context = commandContext(
                harness.runtime(), QStringLiteral("d0d00000-0000-4000-8000-000000000012"));
            auto invalidConfig = harness.config(QString{});
            const auto rejected =
                harness.runtime().audioExports().start(context, invalidConfig, {});
            const auto accepted = harness.runtime().audioExports().start(
                context, harness.config(QStringLiteral("accepted.wav")), {});
            ok &= expect(!rejected &&
                             rejected.getError().code ==
                                 Automation::AutomationErrorCode::PathRequired &&
                             accepted && harness.scheduler().size() == 1 &&
                             harness.runtime().automationTasks().size() == 1,
                         QStringLiteral("async validation failure must not claim a key"));
        }
        {
            AudioHarness harness;
            ok &= expect(harness.isValid(),
                         QStringLiteral("submission-failure audio harness must be valid"));
            harness.control().createFailuresRemaining = 1;
            const auto context = commandContext(
                harness.runtime(), QStringLiteral("d0d00000-0000-4000-8000-000000000013"));
            const auto rejected = harness.runtime().audioExports().start(
                context, harness.config(QStringLiteral("rejected.wav")), {});
            const auto accepted = harness.runtime().audioExports().start(
                context, harness.config(QStringLiteral("accepted.wav")), {});
            ok &= expect(!rejected &&
                             rejected.getError().code ==
                                 Automation::AutomationErrorCode::ModuleNotReady &&
                             accepted && harness.control().createCount == 2 &&
                             harness.scheduler().size() == 1 &&
                             harness.runtime().automationTasks().size() == 1,
                         QStringLiteral("async submission failure must release its key"));
        }
        return ok;
    }

    bool queuedCancellationReleasesKey() {
        AudioHarness harness;
        if (!expect(harness.isValid(), QStringLiteral("queued-cancel audio harness must be valid")))
            return false;
        const auto key = QStringLiteral("d0d00000-0000-4000-8000-000000000014");
        const auto context = commandContext(harness.runtime(), key);
        const auto config = harness.config(QStringLiteral("queued.wav"));
        const auto first = harness.runtime().audioExports().start(context, config, {});
        if (!expect(first && harness.scheduler().size() == 1,
                    QStringLiteral("first queued task must be accepted"))) {
            return false;
        }
        const auto cancel = harness.runtime().tasks().cancelTask(commandContext(harness.runtime()),
                                                                 first.get().taskId);
        const auto ran = harness.scheduler().runNext();
        const auto canceled = harness.runtime().tasks().getTask(
            harness.runtime().documentVersion().documentId, first.get().taskId);
        const auto retried = harness.runtime().audioExports().start(context, config, {});

        return expect(cancel && ran && canceled &&
                          canceled.get().state == Automation::AutomationTaskState::Canceled,
                      QStringLiteral("queued task must reach canceled before retry")) &&
               expect(retried && retried.get().taskId != first.get().taskId &&
                          harness.scheduler().size() == 1,
                      QStringLiteral("queued cancellation must release the idempotency key"));
    }

    bool runningFailureReleasesKey() {
        AudioHarness harness;
        if (!expect(harness.isValid(),
                    QStringLiteral("runtime-failure audio harness must be valid")))
            return false;
        harness.control().backendState = Automation::AudioExportBackendState::Failed;
        const auto key = QStringLiteral("d0d00000-0000-4000-8000-000000000015");
        const auto context = commandContext(harness.runtime(), key);
        const auto config = harness.config(QStringLiteral("runtime-failure.wav"));
        const auto first = harness.runtime().audioExports().start(context, config, {});
        if (!expect(first && harness.scheduler().runNext(),
                    QStringLiteral("first failing task must run"))) {
            return false;
        }
        const auto failed = harness.runtime().tasks().getTask(
            harness.runtime().documentVersion().documentId, first.get().taskId);
        harness.control().backendState = Automation::AudioExportBackendState::Succeeded;
        const auto retried = harness.runtime().audioExports().start(context, config, {});
        bool retrySucceeded = false;
        if (retried && retried.get().taskId != first.get().taskId &&
            harness.scheduler().runNext()) {
            const auto succeeded = harness.runtime().tasks().getTask(
                harness.runtime().documentVersion().documentId, retried.get().taskId);
            retrySucceeded =
                succeeded && succeeded.get().state == Automation::AutomationTaskState::Succeeded;
        }

        return expect(failed && failed.get().state == Automation::AutomationTaskState::Failed &&
                          failed.get().error &&
                          failed.get().error->code == Automation::AutomationErrorCode::IoError,
                      QStringLiteral("backend failure must reach a stable failed task")) &&
               expect(retried && retried.get().taskId != first.get().taskId && retrySucceeded,
                      QStringLiteral("runtime failure must release the idempotency key"));
    }

    bool synchronousFailureBeforeStoreReleasesKey() {
        auto control = std::make_shared<FakeAudioControl>();
        control->backendState = Automation::AudioExportBackendState::Failed;
        Automation::AudioExportRuntimeServices services;
        services.createJob = [control](AppModel *, const QString &,
                                       const Automation::AudioExportConfigDto &config) {
            ++control->createCount;
            std::shared_ptr<Automation::IAudioExportJob> job =
                std::make_shared<FakeAudioJob>(config, control);
            return Automation::AutomationResult<std::shared_ptr<Automation::IAudioExportJob>>(
                std::move(job));
        };
        services.schedule = [](std::function<void()> execute) { execute(); };
        ServiceRuntime fixture({}, std::move(services));
        QTemporaryDir directory;
        if (!expect(directory.isValid(),
                    QStringLiteral("synchronous-failure directory must be valid"))) {
            return false;
        }

        const auto context = commandContext(fixture.runtime(),
                                            QStringLiteral("d0d00000-0000-4000-8000-000000000016"));
        const Automation::AudioExportConfigDto config{
            .fileName = QStringLiteral("synchronous.wav"),
            .fileDirectory = directory.path(),
        };
        const auto first = fixture.runtime().audioExports().start(context, config, {});
        if (!expect(bool(first), QStringLiteral("synchronous failing task must be accepted")))
            return false;
        const auto failed = fixture.runtime().tasks().getTask(
            fixture.runtime().documentVersion().documentId, first.get().taskId);

        control->backendState = Automation::AudioExportBackendState::Succeeded;
        const auto retried = fixture.runtime().audioExports().start(context, config, {});
        std::optional<Automation::AutomationResult<Automation::AutomationTaskSnapshot>> succeeded;
        if (retried) {
            succeeded.emplace(fixture.runtime().tasks().getTask(
                fixture.runtime().documentVersion().documentId, retried.get().taskId));
        }

        return expect(failed && failed.get().state == Automation::AutomationTaskState::Failed,
                      QStringLiteral("synchronous backend failure must be terminal")) &&
               expect(retried && retried.get().taskId != first.get().taskId && succeeded &&
                          *succeeded &&
                          succeeded->get().state == Automation::AutomationTaskState::Succeeded &&
                          control->executeCount == 2,
                      QStringLiteral("failure before dispatcher store must still release the key"));
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    bool ok = true;
    ok &= serialReplayExecutesOnce();
    ok &= concurrentReplayExecutesOnce(16);
    ok &= concurrentReplayExecutesOnce(64);
    ok &= successfulKeyConflictsAreStable();
    ok &= audioClipStateChangesConflict();
    ok &= curveCollectionsChangeConflict();
    ok &= previewsAndValidationFailuresDoNotClaimKeys();
    ok &= commitFailureDoesNotClaimKey();
    ok &= documentsHaveIndependentKeySpaces();
    ok &= generationsHaveIndependentKeySpaces();
    ok &= saveDoesNotClearCache();
    ok &= asyncAcceptedReplaySchedulesOnce();
    ok &= asyncPreAcceptanceFailuresDoNotClaimKeys();
    ok &= queuedCancellationReleasesKey();
    ok &= runningFailureReleasesKey();
    ok &= synchronousFailureBeforeStoreReleasesKey();
    return ok ? 0 : 1;
}
